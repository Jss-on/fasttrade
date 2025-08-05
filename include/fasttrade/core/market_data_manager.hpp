#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <nlohmann/json.hpp>

#include "fasttrade/utils/decimal.hpp"
#include "websocket_connection.hpp"

namespace fasttrade::core {

// Forward declarations
class OrderBook;

/**
 * @brief Market data tick containing price/volume information
 */
struct MarketTick {
    std::string symbol;
    utils::Decimal price;
    utils::Decimal quantity;
    uint64_t timestamp;
    bool is_bid;  // true for bid, false for ask
    
    MarketTick(const std::string& sym, const utils::Decimal& p, const utils::Decimal& q, 
               uint64_t ts, bool bid) 
        : symbol(sym), price(p), quantity(q), timestamp(ts), is_bid(bid) {}
};

/**
 * @brief Trade tick containing executed trade information
 */
struct TradeTick {
    std::string symbol;
    utils::Decimal price;
    utils::Decimal quantity;
    uint64_t timestamp;
    std::string side;  // "buy" or "sell"
    
    TradeTick(const std::string& sym, const utils::Decimal& p, const utils::Decimal& q,
              uint64_t ts, const std::string& s)
        : symbol(sym), price(p), quantity(q), timestamp(ts), side(s) {}
};

/**
 * @brief Exchange-specific market data connector interface
 */
class MarketDataConnector {
public:
    virtual ~MarketDataConnector() = default;
    
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool subscribe_orderbook(const std::string& symbol) = 0;
    virtual bool subscribe_trades(const std::string& symbol) = 0;
    virtual bool unsubscribe_orderbook(const std::string& symbol) = 0;
    virtual bool unsubscribe_trades(const std::string& symbol) = 0;
    virtual bool is_connected() const = 0;
    virtual std::string get_exchange_name() const = 0;
    
    // Callbacks for market data
    std::function<void(const MarketTick&)> on_market_tick;
    std::function<void(const TradeTick&)> on_trade_tick;
    std::function<void(const std::string&)> on_error;
    std::function<void()> on_disconnect;

protected:
    std::unique_ptr<WebSocketAssistant> ws_assistant_;
    std::atomic<bool> connected_{false};
    mutable std::mutex mutex_;
};

/**
 * @brief Binance market data connector
 */
class BinanceConnector : public MarketDataConnector {
public:
    BinanceConnector();
    ~BinanceConnector() override;
    
    bool connect() override;
    void disconnect() override;
    bool subscribe_orderbook(const std::string& symbol) override;
    bool subscribe_trades(const std::string& symbol) override;
    bool unsubscribe_orderbook(const std::string& symbol) override;
    bool unsubscribe_trades(const std::string& symbol) override;
    bool is_connected() const override;
    std::string get_exchange_name() const override { return "binance"; }

private:
    void run_websocket();
    void send_subscription(const std::vector<std::string>& streams);
    std::string format_symbol(const std::string& symbol) const;
    void process_message(const std::string& message);
    std::string extract_symbol_from_stream(const std::string& stream) const;
    void process_orderbook_data(const std::string& symbol, const nlohmann::json& data);
    void process_trade_data(const std::string& symbol, const nlohmann::json& data);
    
    std::thread ws_thread_;
    std::vector<std::string> subscribed_streams_;
    std::string ws_url_;
};

/**
 * @brief Bybit market data connector
 */
class BybitConnector : public MarketDataConnector {
public:
    BybitConnector();
    ~BybitConnector() override;
    
    bool connect() override;
    void disconnect() override;
    bool subscribe_orderbook(const std::string& symbol) override;
    bool subscribe_trades(const std::string& symbol) override;
    bool unsubscribe_orderbook(const std::string& symbol) override;
    bool unsubscribe_trades(const std::string& symbol) override;
    bool is_connected() const override;
    std::string get_exchange_name() const override { return "bybit"; }

private:
    void run_websocket();
    void send_subscription(const std::string& op, const std::vector<std::string>& topics);
    std::string format_symbol(const std::string& symbol) const;
    void process_message(const std::string& message);
    std::string extract_symbol_from_topic(const std::string& topic) const;
    void process_orderbook_data(const std::string& symbol, const nlohmann::json& data);
    void process_trade_data(const std::string& symbol, const nlohmann::json& data);
    
    std::thread ws_thread_;
    std::vector<std::string> subscribed_topics_;
    std::string ws_url_;
};

/**
 * @brief OKX market data connector
 */
class OkxConnector : public MarketDataConnector {
public:
    OkxConnector();
    ~OkxConnector() override;
    
    bool connect() override;
    void disconnect() override;
    bool subscribe_orderbook(const std::string& symbol) override;
    bool subscribe_trades(const std::string& symbol) override;
    bool unsubscribe_orderbook(const std::string& symbol) override;
    bool unsubscribe_trades(const std::string& symbol) override;
    bool is_connected() const override;
    std::string get_exchange_name() const override { return "okx"; }

private:
    void run_websocket();
    void send_subscription(const std::string& op, const std::string& channel, const std::string& symbol);
    std::string format_symbol(const std::string& symbol) const;
    void process_message(const std::string& message);
    void process_orderbook_data(const std::string& symbol, const nlohmann::json& data);
    void process_trade_data(const std::string& symbol, const nlohmann::json& data);
    
    std::thread ws_thread_;
    std::vector<std::string> subscribed_channels_;
    std::string ws_url_;
    std::atomic<bool> connected_{false};
    std::mutex mutex_;
    
    std::unique_ptr<WebSocketAssistant> ws_assistant_;
};

/**
 * @brief Market data manager that coordinates multiple exchange connectors
 */
class MarketDataManager {
public:
    enum class Exchange {
        BINANCE,
        BYBIT,
        OKX
    };
    
    MarketDataManager();
    ~MarketDataManager();
    
    // Initialize with specific exchanges
    bool initialize(const std::vector<Exchange>& exchanges);
    void shutdown();
    
    // Subscribe to market data for a symbol on all configured exchanges
    bool subscribe_market_data(const std::string& symbol);
    bool unsubscribe_market_data(const std::string& symbol);
    
    // Subscribe to market data for a symbol on a specific exchange
    bool subscribe_market_data(const std::string& symbol, Exchange exchange);
    bool unsubscribe_market_data(const std::string& symbol, Exchange exchange);
    
    // Get list of subscribed symbols
    std::vector<std::string> get_subscribed_symbols() const;
    
    // Check if any exchange is connected
    bool is_connected() const;
    
    // Set callbacks for market data events
    void set_market_tick_callback(std::function<void(const MarketTick&, Exchange)> callback);
    void set_trade_tick_callback(std::function<void(const TradeTick&, Exchange)> callback);
    void set_error_callback(std::function<void(const std::string&, Exchange)> callback);
    
private:
    void on_market_tick(const MarketTick& tick, Exchange exchange);
    void on_trade_tick(const TradeTick& tick, Exchange exchange);
    void on_error(const std::string& error, Exchange exchange);
    void on_disconnect(Exchange exchange);
    
    std::unordered_map<Exchange, std::unique_ptr<MarketDataConnector>> connectors_;
    std::unordered_map<std::string, std::vector<Exchange>> symbol_subscriptions_;
    
    // Callbacks
    std::function<void(const MarketTick&, Exchange)> market_tick_callback_;
    std::function<void(const TradeTick&, Exchange)> trade_tick_callback_;
    std::function<void(const std::string&, Exchange)> error_callback_;
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_{false};
};

} // namespace fasttrade::core
