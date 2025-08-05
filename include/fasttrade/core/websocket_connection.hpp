#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <nlohmann/json.hpp>

// Boost.Beast includes
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace fasttrade::core {

// Boost.Beast namespace aliases
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

/**
 * @brief WebSocket message data structure
 */
struct WSMessage {
    enum Type {
        TEXT,
        BINARY,
        PING,
        PONG,
        CLOSE,
        ERROR
    };
    
    Type type;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    
    WSMessage(Type t, const std::string& d) 
        : type(t), data(d), timestamp(std::chrono::system_clock::now()) {}
    
    // Helper to parse JSON data
    nlohmann::json json() const {
        try {
            return nlohmann::json::parse(data);
        } catch (const nlohmann::json::parse_error& e) {
            return nlohmann::json::object();
        }
    }
};

/**
 * @brief WebSocket request structure
 */
struct WSRequest {
    enum PayloadType {
        JSON_PAYLOAD,
        TEXT_PAYLOAD,
        BINARY_PAYLOAD
    };
    
    PayloadType payload_type;
    std::string payload;
    bool is_auth_required;
    
    WSRequest(const nlohmann::json& json_payload, bool auth_required = false)
        : payload_type(JSON_PAYLOAD), is_auth_required(auth_required) {
        payload = json_payload.dump();
    }
    
    WSRequest(const std::string& text_payload, PayloadType type = TEXT_PAYLOAD, bool auth_required = false)
        : payload_type(type), payload(text_payload), is_auth_required(auth_required) {}
};

/**
 * @brief WebSocket connection class following Hummingbot's pattern
 */
class WebSocketConnection {
public:
    using MessageHandler = std::function<void(const WSMessage&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using CloseHandler = std::function<void()>;
    
    WebSocketConnection();
    ~WebSocketConnection();
    
    // Connection management
    bool connect(const std::string& url, 
                const std::map<std::string, std::string>& headers = {});
    void disconnect();
    bool is_connected() const;
    
    // Message sending
    bool send(const WSRequest& request);
    bool send_json(const nlohmann::json& json);
    bool send_text(const std::string& text);
    bool send_ping();
    
    // Event handlers
    void set_message_handler(MessageHandler handler);
    void set_error_handler(ErrorHandler handler);
    void set_close_handler(CloseHandler handler);
    
    // Properties
    std::chrono::system_clock::time_point last_recv_time() const;
    
private:
    // Boost.Beast client and connection
    std::unique_ptr<net::io_context> ioc_;
    std::unique_ptr<ssl::context> ssl_ctx_;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;
    std::unique_ptr<std::thread> client_thread_;
    
    // Connection state
    std::atomic<bool> connected_;
    std::atomic<bool> connecting_;
    std::string url_;
    std::map<std::string, std::string> headers_;
    
    // Event handlers
    MessageHandler message_handler_;
    ErrorHandler error_handler_;
    CloseHandler close_handler_;
    
    // Timing
    mutable std::mutex time_mutex_;
    std::chrono::system_clock::time_point last_recv_time_;
    
    // Thread safety
    mutable std::mutex handler_mutex_;
    
    // Boost.Beast event handlers
    void on_open();
    void on_close();
    void on_fail(beast::error_code ec);
    void on_message(beast::flat_buffer& buffer);
    void on_pong(beast::string_view payload);
    
    // Internal methods
    void run_client();
    void update_last_recv_time();
    WSMessage::Type convert_opcode(websocket::frame_type type);
};

/**
 * @brief WebSocket assistant class following Hummingbot's WSAssistant pattern
 */
class WebSocketAssistant {
public:
    using PreProcessor = std::function<WSRequest(const WSRequest&)>;
    using PostProcessor = std::function<WSMessage(const WSMessage&)>;
    using AuthHandler = std::function<WSRequest(const WSRequest&)>;
    
    WebSocketAssistant();
    ~WebSocketAssistant();
    
    // Connection management
    bool connect(const std::string& url,
                const std::map<std::string, std::string>& headers = {},
                float ping_timeout = 10.0f,
                float message_timeout = 30.0f);
    void disconnect();
    bool is_connected() const;
    
    // Message handling
    bool send(const WSRequest& request);
    bool subscribe(const WSRequest& request); // For automatic re-connection handling
    
    // Event handlers
    void set_message_handler(WebSocketConnection::MessageHandler handler);
    void set_error_handler(WebSocketConnection::ErrorHandler handler);
    void set_close_handler(WebSocketConnection::CloseHandler handler);
    
    // Processing chain
    void add_pre_processor(PreProcessor processor);
    void add_post_processor(PostProcessor processor);
    void set_auth_handler(AuthHandler handler);
    
    // Properties
    std::chrono::system_clock::time_point last_recv_time() const;
    
private:
    std::unique_ptr<WebSocketConnection> connection_;
    std::vector<PreProcessor> pre_processors_;
    std::vector<PostProcessor> post_processors_;
    AuthHandler auth_handler_;
    
    // Ping management
    std::unique_ptr<std::thread> ping_thread_;
    std::atomic<bool> ping_active_;
    float ping_interval_;
    
    // Subscription tracking for reconnection
    std::vector<WSRequest> subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    
    // Internal methods
    WSRequest pre_process_request(const WSRequest& request);
    WSRequest authenticate_request(const WSRequest& request);
    WSMessage post_process_response(const WSMessage& response);
    void start_ping_thread();
    void stop_ping_thread();
    void ping_loop();
    
    // Event handler wrappers
    void on_message_wrapper(const WSMessage& message);
    void on_error_wrapper(const std::string& error);
    void on_close_wrapper();
    
    // Stored handlers
    WebSocketConnection::MessageHandler message_handler_;
    WebSocketConnection::ErrorHandler error_handler_;
    WebSocketConnection::CloseHandler close_handler_;
};

/**
 * @brief Factory class for creating WebSocket connections
 */
class WebSocketFactory {
public:
    static std::unique_ptr<WebSocketAssistant> create_assistant();
    static std::unique_ptr<WebSocketConnection> create_connection();
    
    // Configuration
    static void set_default_ping_timeout(float timeout);
    static void set_default_message_timeout(float timeout);
    static void set_default_max_message_size(size_t size);
    
private:
    static float default_ping_timeout_;
    static float default_message_timeout_;
    static size_t default_max_message_size_;
};

} // namespace fasttrade::core
