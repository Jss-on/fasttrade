#include "fasttrade/core/websocket_connection.hpp"
#include <iostream>
#include <sstream>

namespace fasttrade::core {

// Static members initialization
float WebSocketFactory::default_ping_timeout_ = 10.0f;
float WebSocketFactory::default_message_timeout_ = 30.0f;
size_t WebSocketFactory::default_max_message_size_ = 4 * 1024 * 1024; // 4MB

// =============================================================================
// WebSocketConnection Implementation (Stub)
// =============================================================================

WebSocketConnection::WebSocketConnection() 
    : ioc_(std::make_unique<net::io_context>()),
      ssl_ctx_(std::make_unique<ssl::context>(ssl::context::tlsv12_client)),
      connected_(false),
      connecting_(false),
      last_recv_time_(std::chrono::system_clock::now()) {
    
    // Configure SSL context
    ssl_ctx_->set_default_verify_paths();
    ssl_ctx_->set_verify_mode(ssl::verify_peer);
}

WebSocketConnection::~WebSocketConnection() {
    disconnect();
}

bool WebSocketConnection::connect(const std::string& url, 
                                 const std::map<std::string, std::string>& headers) {
    if (connected_.load() || connecting_.load()) {
        return false;
    }
    
    url_ = url;
    headers_ = headers;
    connecting_ = true;
    
    // TODO: Implement Boost.Beast WebSocket connection
    // For now, simulate successful connection
    std::cout << "WebSocket connecting to: " << url << std::endl;
    
    connected_ = true;
    connecting_ = false;
    update_last_recv_time();
    
    if (message_handler_) {
        // Simulate connection open event
        on_open();
    }
    
    return true;
}

void WebSocketConnection::disconnect() {
    if (!connected_.load() && !connecting_.load()) {
        return;
    }
    
    connected_ = false;
    connecting_ = false;
    
    std::cout << "WebSocket disconnecting..." << std::endl;
    
    if (client_thread_ && client_thread_->joinable()) {
        client_thread_->join();
    }
    client_thread_.reset();
    
    on_close();
}

bool WebSocketConnection::is_connected() const {
    return connected_.load();
}

bool WebSocketConnection::send(const WSRequest& request) {
    if (!connected_.load()) {
        return false;
    }
    
    // TODO: Implement Boost.Beast message sending
    std::cout << "WebSocket sending: " << request.payload << std::endl;
    
    return true;
}

bool WebSocketConnection::send_json(const nlohmann::json& json) {
    WSRequest request(json);
    return send(request);
}

bool WebSocketConnection::send_text(const std::string& text) {
    WSRequest request(text);
    return send(request);
}

bool WebSocketConnection::send_ping() {
    if (!connected_.load()) {
        return false;
    }
    
    std::cout << "WebSocket sending ping" << std::endl;
    return true;
}

void WebSocketConnection::set_message_handler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    message_handler_ = handler;
}

void WebSocketConnection::set_error_handler(ErrorHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    error_handler_ = handler;
}

void WebSocketConnection::set_close_handler(CloseHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    close_handler_ = handler;
}

std::chrono::system_clock::time_point WebSocketConnection::last_recv_time() const {
    std::lock_guard<std::mutex> lock(time_mutex_);
    return last_recv_time_;
}

void WebSocketConnection::run_client() {
    // TODO: Implement Boost.Beast event loop
    std::cout << "WebSocket client thread running..." << std::endl;
}

void WebSocketConnection::on_open() {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    connected_ = true;
    connecting_ = false;
    update_last_recv_time();
    std::cout << "WebSocket connection opened" << std::endl;
}

void WebSocketConnection::on_close() {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    connected_ = false;
    if (close_handler_) {
        close_handler_();
    }
    std::cout << "WebSocket connection closed" << std::endl;
}

void WebSocketConnection::on_fail(beast::error_code ec) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    connected_ = false;
    connecting_ = false;
    if (error_handler_) {
        error_handler_("WebSocket connection failed: " + ec.message());
    }
    std::cout << "WebSocket connection failed: " << ec.message() << std::endl;
}

void WebSocketConnection::on_message(beast::flat_buffer& buffer) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    update_last_recv_time();
    
    if (message_handler_) {
        std::string data = beast::buffers_to_string(buffer.data());
        WSMessage message(WSMessage::TEXT, data);
        message_handler_(message);
    }
}

void WebSocketConnection::on_pong(beast::string_view payload) {
    update_last_recv_time();
    std::cout << "WebSocket pong received: " << payload << std::endl;
}

void WebSocketConnection::update_last_recv_time() {
    std::lock_guard<std::mutex> lock(time_mutex_);
    last_recv_time_ = std::chrono::system_clock::now();
}

WSMessage::Type WebSocketConnection::convert_opcode(websocket::frame_type type) {
    // In modern Boost.Beast, frame_type is typically just close
    // Text/binary determination is handled by ws_.got_text() method
    switch (type) {
        case websocket::frame_type::close:
            return WSMessage::CLOSE;
        default:
            // For data frames, we'll determine text vs binary differently
            return WSMessage::TEXT;
    }
}

// =============================================================================
// WebSocketAssistant Implementation (Stub)
// =============================================================================

WebSocketAssistant::WebSocketAssistant()
    : connection_(std::make_unique<WebSocketConnection>()),
      ping_active_(false),
      ping_interval_(10.0f) {
}

WebSocketAssistant::~WebSocketAssistant() {
    disconnect();
}

bool WebSocketAssistant::connect(const std::string& url,
                                const std::map<std::string, std::string>& headers,
                                float ping_timeout, float message_timeout) {
    ping_interval_ = ping_timeout;
    
    // Set up event handlers
    connection_->set_message_handler([this](const WSMessage& msg) {
        on_message_wrapper(msg);
    });
    
    connection_->set_error_handler([this](const std::string& error) {
        on_error_wrapper(error);
    });
    
    connection_->set_close_handler([this]() {
        on_close_wrapper();
    });
    
    bool result = connection_->connect(url, headers);
    if (result) {
        start_ping_thread();
    }
    
    return result;
}

void WebSocketAssistant::disconnect() {
    stop_ping_thread();
    if (connection_) {
        connection_->disconnect();
    }
}

bool WebSocketAssistant::is_connected() const {
    return connection_ && connection_->is_connected();
}

bool WebSocketAssistant::send(const WSRequest& request) {
    if (!connection_ || !connection_->is_connected()) {
        return false;
    }
    
    WSRequest processed_request = pre_process_request(request);
    processed_request = authenticate_request(processed_request);
    
    return connection_->send(processed_request);
}

bool WebSocketAssistant::subscribe(const WSRequest& request) {
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.push_back(request);
    }
    
    return send(request);
}

void WebSocketAssistant::set_message_handler(WebSocketConnection::MessageHandler handler) {
    message_handler_ = handler;
}

void WebSocketAssistant::set_error_handler(WebSocketConnection::ErrorHandler handler) {
    error_handler_ = handler;
}

void WebSocketAssistant::set_close_handler(WebSocketConnection::CloseHandler handler) {
    close_handler_ = handler;
}

void WebSocketAssistant::add_pre_processor(PreProcessor processor) {
    pre_processors_.push_back(processor);
}

void WebSocketAssistant::add_post_processor(PostProcessor processor) {
    post_processors_.push_back(processor);
}

void WebSocketAssistant::set_auth_handler(AuthHandler handler) {
    auth_handler_ = handler;
}

std::chrono::system_clock::time_point WebSocketAssistant::last_recv_time() const {
    return connection_ ? connection_->last_recv_time() : std::chrono::system_clock::time_point{};
}

WSRequest WebSocketAssistant::pre_process_request(const WSRequest& request) {
    WSRequest processed = request;
    for (const auto& processor : pre_processors_) {
        processed = processor(processed);
    }
    return processed;
}

WSRequest WebSocketAssistant::authenticate_request(const WSRequest& request) {
    if (auth_handler_ && request.is_auth_required) {
        return auth_handler_(request);
    }
    return request;
}

WSMessage WebSocketAssistant::post_process_response(const WSMessage& response) {
    WSMessage processed = response;
    for (const auto& processor : post_processors_) {
        processed = processor(processed);
    }
    return processed;
}

void WebSocketAssistant::start_ping_thread() {
    if (ping_active_.load()) {
        return;
    }
    
    ping_active_ = true;
    ping_thread_ = std::make_unique<std::thread>(&WebSocketAssistant::ping_loop, this);
}

void WebSocketAssistant::stop_ping_thread() {
    ping_active_ = false;
    if (ping_thread_ && ping_thread_->joinable()) {
        ping_thread_->join();
    }
    ping_thread_.reset();
}

void WebSocketAssistant::ping_loop() {
    while (ping_active_.load()) {
        if (connection_ && connection_->is_connected()) {
            connection_->send_ping();
        }
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<long>(ping_interval_)));
    }
}

void WebSocketAssistant::on_message_wrapper(const WSMessage& message) {
    WSMessage processed_message = post_process_response(message);
    if (message_handler_) {
        message_handler_(processed_message);
    }
}

void WebSocketAssistant::on_error_wrapper(const std::string& error) {
    if (error_handler_) {
        error_handler_(error);
    }
}

void WebSocketAssistant::on_close_wrapper() {
    stop_ping_thread();
    if (close_handler_) {
        close_handler_();
    }
}

// =============================================================================
// WebSocketFactory Implementation
// =============================================================================

std::unique_ptr<WebSocketAssistant> WebSocketFactory::create_assistant() {
    return std::make_unique<WebSocketAssistant>();
}

std::unique_ptr<WebSocketConnection> WebSocketFactory::create_connection() {
    return std::make_unique<WebSocketConnection>();
}

void WebSocketFactory::set_default_ping_timeout(float timeout) {
    default_ping_timeout_ = timeout;
}

void WebSocketFactory::set_default_message_timeout(float timeout) {
    default_message_timeout_ = timeout;
}

void WebSocketFactory::set_default_max_message_size(size_t size) {
    default_max_message_size_ = size;
}

} // namespace fasttrade::core
