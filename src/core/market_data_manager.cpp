#include "fasttrade/core/market_data_manager.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <thread>

namespace fasttrade::core {

// =============================================================================
// MarketDataManager Implementation
// =============================================================================

MarketDataManager::MarketDataManager() = default;

MarketDataManager::~MarketDataManager() {
    shutdown();
}

bool MarketDataManager::initialize(const std::vector<Exchange>& exchanges) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.load()) {
        return true;
    }
    
    try {
        for (auto exchange : exchanges) {
            std::unique_ptr<MarketDataConnector> connector;
            
            switch (exchange) {
                case Exchange::BINANCE:
                    connector = std::make_unique<BinanceConnector>();
                    break;
                case Exchange::BYBIT:
                    connector = std::make_unique<BybitConnector>();
                    break;
                case Exchange::OKX:
                    connector = std::make_unique<OkxConnector>();
                    break;
            }
            
            if (connector) {
                // Set up callbacks
                connector->on_market_tick = [this, exchange](const MarketTick& tick) {
                    on_market_tick(tick, exchange);
                };
                connector->on_trade_tick = [this, exchange](const TradeTick& tick) {
                    on_trade_tick(tick, exchange);
                };
                connector->on_error = [this, exchange](const std::string& error) {
                    on_error(error, exchange);
                };
                connector->on_disconnect = [this, exchange]() {
                    on_disconnect(exchange);
                };
                
                // Connect to exchange
                if (connector->connect()) {
                    connectors_[exchange] = std::move(connector);
                    std::cout << "Successfully initialized connector for exchange " << static_cast<int>(exchange) << std::endl;
                } else {
                    std::cerr << "Failed to connect to exchange " << static_cast<int>(exchange) << std::endl;
                    return false;
                }
            }
        }
        
        initialized_ = true;
        std::cout << "MarketDataManager initialized with " << connectors_.size() << " exchanges" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize MarketDataManager: " << e.what() << std::endl;
        return false;
    }
}

void MarketDataManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_.load()) {
        return;
    }
    
    for (auto& [exchange, connector] : connectors_) {
        if (connector) {
            connector->disconnect();
        }
    }
    
    connectors_.clear();
    symbol_subscriptions_.clear();
    initialized_ = false;
    
    std::cout << "MarketDataManager shutdown complete" << std::endl;
}

bool MarketDataManager::subscribe_market_data(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_.load()) {
        return false;
    }
    
    bool success = true;
    std::vector<Exchange> successful_exchanges;
    
    for (auto& [exchange, connector] : connectors_) {
        if (connector && connector->is_connected()) {
            bool orderbook_success = connector->subscribe_orderbook(symbol);
            bool trades_success = connector->subscribe_trades(symbol);
            
            if (orderbook_success && trades_success) {
                successful_exchanges.push_back(exchange);
            } else {
                success = false;
            }
        }
    }
    
    if (!successful_exchanges.empty()) {
        symbol_subscriptions_[symbol] = successful_exchanges;
    }
    
    return success;
}

bool MarketDataManager::unsubscribe_market_data(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = symbol_subscriptions_.find(symbol);
    if (it == symbol_subscriptions_.end()) {
        return false;
    }
    
    bool success = true;
    for (auto exchange : it->second) {
        auto connector_it = connectors_.find(exchange);
        if (connector_it != connectors_.end() && connector_it->second) {
            bool orderbook_success = connector_it->second->unsubscribe_orderbook(symbol);
            bool trades_success = connector_it->second->unsubscribe_trades(symbol);
            
            if (!orderbook_success || !trades_success) {
                success = false;
            }
        }
    }
    
    symbol_subscriptions_.erase(it);
    return success;
}

bool MarketDataManager::subscribe_market_data(const std::string& symbol, Exchange exchange) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_.load()) {
        return false;
    }
    
    auto connector_it = connectors_.find(exchange);
    if (connector_it == connectors_.end() || !connector_it->second) {
        return false;
    }
    
    auto& connector = connector_it->second;
    if (!connector->is_connected()) {
        return false;
    }
    
    bool orderbook_success = connector->subscribe_orderbook(symbol);
    bool trades_success = connector->subscribe_trades(symbol);
    
    if (orderbook_success && trades_success) {
        // Add this exchange to the symbol's subscription list
        auto& exchanges = symbol_subscriptions_[symbol];
        if (std::find(exchanges.begin(), exchanges.end(), exchange) == exchanges.end()) {
            exchanges.push_back(exchange);
        }
        return true;
    }
    
    return false;
}

bool MarketDataManager::unsubscribe_market_data(const std::string& symbol, Exchange exchange) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto symbol_it = symbol_subscriptions_.find(symbol);
    if (symbol_it == symbol_subscriptions_.end()) {
        return false;
    }
    
    auto connector_it = connectors_.find(exchange);
    if (connector_it == connectors_.end() || !connector_it->second) {
        return false;
    }
    
    auto& connector = connector_it->second;
    bool orderbook_success = connector->unsubscribe_orderbook(symbol);
    bool trades_success = connector->unsubscribe_trades(symbol);
    
    // Remove this exchange from the symbol's subscription list
    auto& exchanges = symbol_it->second;
    exchanges.erase(std::remove(exchanges.begin(), exchanges.end(), exchange), exchanges.end());
    
    // If no more exchanges are subscribed to this symbol, remove the symbol entry
    if (exchanges.empty()) {
        symbol_subscriptions_.erase(symbol_it);
    }
    
    return orderbook_success && trades_success;
}

bool MarketDataManager::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [exchange, connector] : connectors_) {
        if (connector && connector->is_connected()) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::string> MarketDataManager::get_subscribed_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> symbols;
    symbols.reserve(symbol_subscriptions_.size());
    
    for (const auto& [symbol, exchanges] : symbol_subscriptions_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

void MarketDataManager::set_market_tick_callback(std::function<void(const MarketTick&, Exchange)> callback) {
    market_tick_callback_ = callback;
}

void MarketDataManager::set_trade_tick_callback(std::function<void(const TradeTick&, Exchange)> callback) {
    trade_tick_callback_ = callback;
}

void MarketDataManager::set_error_callback(std::function<void(const std::string&, Exchange)> callback) {
    error_callback_ = callback;
}

void MarketDataManager::on_market_tick(const MarketTick& tick, Exchange exchange) {
    if (market_tick_callback_) {
        market_tick_callback_(tick, exchange);
    }
}

void MarketDataManager::on_trade_tick(const TradeTick& tick, Exchange exchange) {
    if (trade_tick_callback_) {
        trade_tick_callback_(tick, exchange);
    }
}

void MarketDataManager::on_error(const std::string& error, Exchange exchange) {
    if (error_callback_) {
        error_callback_(error, exchange);
    }
}

void MarketDataManager::on_disconnect(Exchange exchange) {
    std::cout << "Exchange " << static_cast<int>(exchange) << " disconnected" << std::endl;
}

// =============================================================================
// Connector Implementations (Mock implementations for demonstration)
// =============================================================================

BinanceConnector::BinanceConnector() : ws_url_("wss://stream.binance.com:9443/ws/") {
    ws_assistant_ = WebSocketFactory::create_assistant();
}

BinanceConnector::~BinanceConnector() {
    disconnect();
}

bool BinanceConnector::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_.load()) return true;
    
    // Set up WebSocket event handlers
    ws_assistant_->set_message_handler([this](const WSMessage& message) {
        process_message(message.data);
    });
    
    ws_assistant_->set_error_handler([this](const std::string& error) {
        std::cerr << "[Binance] WebSocket error: " << error << std::endl;
        if (on_error) {
            on_error(error);
        }
    });
    
    ws_assistant_->set_close_handler([this]() {
        std::cout << "[Binance] WebSocket connection closed" << std::endl;
        connected_ = false;
        if (on_disconnect) {
            on_disconnect();
        }
    });
    
    // Connect to Binance WebSocket
    if (ws_assistant_->connect(ws_url_)) {
        connected_ = true;
        std::cout << "[Binance] Connected to market data stream" << std::endl;
        return true;
    } else {
        std::cerr << "[Binance] Failed to connect to WebSocket" << std::endl;
        return false;
    }
}

void BinanceConnector::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return;
    
    if (ws_assistant_) {
        ws_assistant_->disconnect();
    }
    connected_ = false;
    subscribed_streams_.clear();
    std::cout << "[Binance] Disconnected from market data stream" << std::endl;
}

bool BinanceConnector::subscribe_orderbook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string stream = formatted_symbol + "@depth@100ms";
    
    // Check if already subscribed
    if (std::find(subscribed_streams_.begin(), subscribed_streams_.end(), stream) != subscribed_streams_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["method"] = "SUBSCRIBE";
    subscription["params"] = nlohmann::json::array();
    subscription["params"].push_back(stream);
    subscription["id"] = static_cast<int>(subscribed_streams_.size() + 1);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_streams_.push_back(stream);
        std::cout << "[Binance] Subscribed to orderbook for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool BinanceConnector::subscribe_trades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string stream = formatted_symbol + "@trade";
    
    // Check if already subscribed
    if (std::find(subscribed_streams_.begin(), subscribed_streams_.end(), stream) != subscribed_streams_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["method"] = "SUBSCRIBE";
    subscription["params"] = nlohmann::json::array();
    subscription["params"].push_back(stream);
    subscription["id"] = static_cast<int>(subscribed_streams_.size() + 1);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_streams_.push_back(stream);
        std::cout << "[Binance] Subscribed to trades for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool BinanceConnector::unsubscribe_orderbook(const std::string& symbol) {
    std::cout << "[Binance] Unsubscribed from orderbook for " << symbol << std::endl;
    return true;
}

bool BinanceConnector::unsubscribe_trades(const std::string& symbol) {
    std::cout << "[Binance] Unsubscribed from trades for " << symbol << std::endl;
    return true;
}

bool BinanceConnector::is_connected() const { return connected_.load(); }

std::string BinanceConnector::format_symbol(const std::string& symbol) const {
    // Convert BTC-USDT to btcusdt
    std::string formatted = symbol;
    std::transform(formatted.begin(), formatted.end(), formatted.begin(), ::tolower);
    formatted.erase(std::remove(formatted.begin(), formatted.end(), '-'), formatted.end());
    return formatted;
}

void BinanceConnector::process_message(const std::string& message) {
    try {
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(message);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[Binance] Failed to parse JSON: " << message << std::endl;
            return;
        }
        
        // Handle subscription response
        if (root.contains("result") && root["result"].is_null()) {
            std::cout << "[Binance] Subscription confirmed: " << message << std::endl;
            return;
        }
        
        // Handle market data
        if (root.contains("stream") && root.contains("data")) {
            std::string stream = root["stream"].get<std::string>();
            nlohmann::json data = root["data"];
            
            // Extract symbol from stream
            std::string symbol = extract_symbol_from_stream(stream);
            
            if (stream.find("@depth") != std::string::npos) {
                // Process order book data
                process_orderbook_data(symbol, data);
            } else if (stream.find("@trade") != std::string::npos) {
                // Process trade data
                process_trade_data(symbol, data);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Binance] Error processing message: " << e.what() << std::endl;
    }
}

std::string BinanceConnector::extract_symbol_from_stream(const std::string& stream) const {
    // Extract symbol from stream like "btcusdt@depth@100ms"
    size_t at_pos = stream.find('@');
    if (at_pos != std::string::npos) {
        std::string symbol = stream.substr(0, at_pos);
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
        // Convert btcusdt to BTC-USDT
        if (symbol.length() >= 6 && symbol.substr(symbol.length() - 4) == "USDT") {
            return symbol.substr(0, symbol.length() - 4) + "-USDT";
        }
    }
    return stream;
}

void BinanceConnector::process_orderbook_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_market_tick && data.contains("b") && data.contains("a")) {
        // Process bids
        if (data["b"].is_array() && !data["b"].empty()) {
            for (const auto& bid : data["b"]) {
                if (bid.is_array() && bid.size() >= 2) {
                    utils::Decimal price(bid[0].get<std::string>());
                    utils::Decimal quantity(bid[1].get<std::string>());
                    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    MarketTick tick(symbol, price, quantity, timestamp, true);
                    on_market_tick(tick);
                }
            }
        }
        
        // Process asks
        if (data["a"].is_array() && data["a"].size() > 0) {
            for (const auto& ask : data["a"]) {
                if (ask.is_array() && ask.size() >= 2) {
                    utils::Decimal price(ask[0].get<std::string>());
                    utils::Decimal quantity(ask[1].get<std::string>());
                    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    MarketTick tick(symbol, price, quantity, timestamp, false);
                    on_market_tick(tick);
                }
            }
        }
    }
}

void BinanceConnector::process_trade_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_trade_tick && data.contains("p") && data.contains("q") && data.contains("m")) {
        utils::Decimal price(data["p"].get<std::string>());
        utils::Decimal quantity(data["q"].get<std::string>());
        std::string side = data["m"].get<bool>() ? "sell" : "buy"; // m=true means buyer is market maker (sell)
        uint64_t timestamp = data["T"].get<uint64_t>();
        
        TradeTick tick(symbol, price, quantity, timestamp, side);
        on_trade_tick(tick);
    }
}

// Similar implementations for Bybit and OKX...
BybitConnector::BybitConnector() : ws_url_("wss://stream.bybit.com/v5/public/spot") {
    ws_assistant_ = WebSocketFactory::create_assistant();
}
BybitConnector::~BybitConnector() { disconnect(); }

bool BybitConnector::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_.load()) return true;
    
    // Set up WebSocket event handlers
    ws_assistant_->set_message_handler([this](const WSMessage& message) {
        process_message(message.data);
    });
    
    ws_assistant_->set_error_handler([this](const std::string& error) {
        std::cerr << "[Bybit] WebSocket error: " << error << std::endl;
        if (on_error) {
            on_error(error);
        }
    });
    
    ws_assistant_->set_close_handler([this]() {
        std::cout << "[Bybit] WebSocket connection closed" << std::endl;
        connected_ = false;
        if (on_disconnect) {
            on_disconnect();
        }
    });
    
    // Connect to Bybit WebSocket
    if (ws_assistant_->connect(ws_url_)) {
        connected_ = true;
        std::cout << "[Bybit] Connected to market data stream" << std::endl;
        return true;
    } else {
        std::cerr << "[Bybit] Failed to connect to WebSocket" << std::endl;
        return false;
    }
}

void BybitConnector::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return;
    
    if (ws_assistant_) {
        ws_assistant_->disconnect();
    }
    connected_ = false;
    subscribed_topics_.clear();
    std::cout << "[Bybit] Disconnected from market data stream" << std::endl;
}

bool BybitConnector::subscribe_orderbook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string topic = "orderbook.50." + formatted_symbol;
    
    if (std::find(subscribed_topics_.begin(), subscribed_topics_.end(), topic) != subscribed_topics_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["op"] = "subscribe";
    subscription["args"] = nlohmann::json::array();
    subscription["args"].push_back(topic);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_topics_.push_back(topic);
        std::cout << "[Bybit] Subscribed to orderbook for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool BybitConnector::subscribe_trades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string topic = "publicTrade." + formatted_symbol;
    
    if (std::find(subscribed_topics_.begin(), subscribed_topics_.end(), topic) != subscribed_topics_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["op"] = "subscribe";
    subscription["args"] = nlohmann::json::array();
    subscription["args"].push_back(topic);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_topics_.push_back(topic);
        std::cout << "[Bybit] Subscribed to trades for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool BybitConnector::unsubscribe_orderbook(const std::string& symbol) {
    std::cout << "[Bybit] Unsubscribed from orderbook for " << symbol << std::endl;
    return true;
}

bool BybitConnector::unsubscribe_trades(const std::string& symbol) {
    std::cout << "[Bybit] Unsubscribed from trades for " << symbol << std::endl;
    return true;
}

bool BybitConnector::is_connected() const { return connected_.load(); }

std::string BybitConnector::format_symbol(const std::string& symbol) const {
    // Convert BTC-USDT to BTCUSDT
    std::string formatted = symbol;
    std::transform(formatted.begin(), formatted.end(), formatted.begin(), ::toupper);
    formatted.erase(std::remove(formatted.begin(), formatted.end(), '-'), formatted.end());
    return formatted;
}

void BybitConnector::process_message(const std::string& message) {
    try {
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(message);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[Bybit] Failed to parse JSON: " << message << std::endl;
            return;
        }
        
        // Handle subscription response
        if (root.contains("op") && root["op"].get<std::string>() == "subscribe") {
            if (root.contains("success") && root["success"].get<bool>()) {
                std::cout << "[Bybit] Subscription confirmed: " << message << std::endl;
            } else {
                std::cerr << "[Bybit] Subscription failed: " << message << std::endl;
            }
            return;
        }
        
        // Handle market data
        if (root.contains("topic") && root.contains("data")) {
            std::string topic = root["topic"].get<std::string>();
            nlohmann::json data = root["data"];
            
            if (topic.find("orderbook") != std::string::npos) {
                // Process order book data
                std::string symbol = extract_symbol_from_topic(topic);
                process_orderbook_data(symbol, data);
            } else if (topic.find("publicTrade") != std::string::npos) {
                // Process trade data
                std::string symbol = extract_symbol_from_topic(topic);
                process_trade_data(symbol, data);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Bybit] Error processing message: " << e.what() << std::endl;
    }
}

std::string BybitConnector::extract_symbol_from_topic(const std::string& topic) const {
    // Extract symbol from topic like "orderbook.50.BTCUSDT" or "publicTrade.BTCUSDT"
    size_t last_dot = topic.find_last_of('.');
    if (last_dot != std::string::npos) {
        std::string symbol = topic.substr(last_dot + 1);
        // Convert BTCUSDT to BTC-USDT
        if (symbol.length() >= 6 && symbol.substr(symbol.length() - 4) == "USDT") {
            return symbol.substr(0, symbol.length() - 4) + "-USDT";
        }
    }
    return topic;
}

void BybitConnector::process_orderbook_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_market_tick && data.contains("b") && data.contains("a")) {
        // Process bids
        if (data["b"].is_array()) {
            for (const auto& bid : data["b"]) {
                if (bid.is_array() && bid.size() >= 2) {
                    utils::Decimal price(bid[0].get<std::string>());
                    utils::Decimal quantity(bid[1].get<std::string>());
                    uint64_t timestamp = data.contains("ts") ? data["ts"].get<uint64_t>() : 
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                    MarketTick tick(symbol, price, quantity, timestamp, true);
                    on_market_tick(tick);
                }
            }
        }
        
        // Process asks
        if (data["a"].is_array()) {
            for (const auto& ask : data["a"]) {
                if (ask.is_array() && ask.size() >= 2) {
                    utils::Decimal price(ask[0].get<std::string>());
                    utils::Decimal quantity(ask[1].get<std::string>());
                    uint64_t timestamp = data.contains("ts") ? data["ts"].get<uint64_t>() : 
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                    MarketTick tick(symbol, price, quantity, timestamp, false);
                    on_market_tick(tick);
                }
            }
        }
    }
}

void BybitConnector::process_trade_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_trade_tick && data.is_array()) {
        for (const auto& trade : data) {
            if (trade.contains("p") && trade.contains("v") && trade.contains("S")) {
                utils::Decimal price(trade["p"].get<std::string>());
                utils::Decimal quantity(trade["v"].get<std::string>());
                std::string side = trade["S"].get<std::string>(); // "Buy" or "Sell"
                std::transform(side.begin(), side.end(), side.begin(), ::tolower);
                uint64_t timestamp = trade.contains("T") ? trade["T"].get<uint64_t>() : 
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                
                TradeTick tick(symbol, price, quantity, timestamp, side);
                on_trade_tick(tick);
            }
        }
    }
}

OkxConnector::OkxConnector() : ws_url_("wss://ws.okx.com:8443/ws/v5/public") {
    ws_assistant_ = WebSocketFactory::create_assistant();
}
OkxConnector::~OkxConnector() { disconnect(); }

bool OkxConnector::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_.load()) return true;
    
    // Set up WebSocket event handlers
    ws_assistant_->set_message_handler([this](const WSMessage& message) {
        process_message(message.data);
    });
    
    ws_assistant_->set_error_handler([this](const std::string& error) {
        std::cerr << "[OKX] WebSocket error: " << error << std::endl;
        if (on_error) {
            on_error(error);
        }
    });
    
    ws_assistant_->set_close_handler([this]() {
        std::cout << "[OKX] WebSocket connection closed" << std::endl;
        connected_ = false;
        if (on_disconnect) {
            on_disconnect();
        }
    });
    
    // Connect to OKX WebSocket
    if (ws_assistant_->connect(ws_url_)) {
        connected_ = true;
        std::cout << "[OKX] Connected to market data stream" << std::endl;
        return true;
    } else {
        std::cerr << "[OKX] Failed to connect to WebSocket" << std::endl;
        return false;
    }
}

void OkxConnector::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return;
    
    if (ws_assistant_) {
        ws_assistant_->disconnect();
    }
    connected_ = false;
    subscribed_channels_.clear();
    std::cout << "[OKX] Disconnected from market data stream" << std::endl;
}

bool OkxConnector::subscribe_orderbook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string channel = "books:" + formatted_symbol;
    
    if (std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel) != subscribed_channels_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["op"] = "subscribe";
    subscription["args"] = nlohmann::json::array();
    
    nlohmann::json arg;
    arg["channel"] = "books";
    arg["instId"] = formatted_symbol;
    subscription["args"].push_back(arg);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_channels_.push_back(channel);
        std::cout << "[OKX] Subscribed to orderbook for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool OkxConnector::subscribe_trades(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_.load()) return false;
    
    std::string formatted_symbol = format_symbol(symbol);
    std::string channel = "trades:" + formatted_symbol;
    
    if (std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel) != subscribed_channels_.end()) {
        return true;
    }
    
    // Create subscription request
    nlohmann::json subscription;
    subscription["op"] = "subscribe";
    subscription["args"] = nlohmann::json::array();
    
    nlohmann::json arg;
    arg["channel"] = "trades";
    arg["instId"] = formatted_symbol;
    subscription["args"].push_back(arg);
    
    WSRequest request(subscription);
    if (ws_assistant_->subscribe(request)) {
        subscribed_channels_.push_back(channel);
        std::cout << "[OKX] Subscribed to trades for " << symbol << std::endl;
        return true;
    }
    
    return false;
}

bool OkxConnector::unsubscribe_orderbook(const std::string& symbol) {
    std::cout << "[OKX] Unsubscribed from orderbook for " << symbol << std::endl;
    return true;
}

bool OkxConnector::unsubscribe_trades(const std::string& symbol) {
    std::cout << "[OKX] Unsubscribed from trades for " << symbol << std::endl;
    return true;
}

bool OkxConnector::is_connected() const { return connected_.load(); }

std::string OkxConnector::format_symbol(const std::string& symbol) const {
    // OKX uses the same format as input (BTC-USDT)
    return symbol;
}

void OkxConnector::process_message(const std::string& message) {
    try {
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(message);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[OKX] Failed to parse JSON: " << message << std::endl;
            return;
        }
        
        // Handle subscription response
        if (root.contains("event") && root["event"].get<std::string>() == "subscribe") {
            if (root.contains("code") && root["code"].get<std::string>() == "0") {
                std::cout << "[OKX] Subscription confirmed: " << message << std::endl;
            } else {
                std::cerr << "[OKX] Subscription failed: " << message << std::endl;
            }
            return;
        }
        
        // Handle market data
        if (root.contains("arg") && root.contains("data")) {
            nlohmann::json arg = root["arg"];
            nlohmann::json data = root["data"];
            
            if (arg.contains("channel") && arg.contains("instId")) {
                std::string channel = arg["channel"].get<std::string>();
                std::string symbol = arg["instId"].get<std::string>();
                
                if (channel == "books") {
                    // Process order book data
                    process_orderbook_data(symbol, data);
                } else if (channel == "trades") {
                    // Process trade data
                    process_trade_data(symbol, data);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[OKX] Error processing message: " << e.what() << std::endl;
    }
}

void OkxConnector::process_orderbook_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_market_tick && data.is_array()) {
        for (const auto& book_data : data) {
            if (book_data.contains("bids") && book_data.contains("asks")) {
                // Process bids
                if (book_data["bids"].is_array()) {
                    for (const auto& bid : book_data["bids"]) {
                        if (bid.is_array() && bid.size() >= 2) {
                            utils::Decimal price(bid[0].get<std::string>());
                            utils::Decimal quantity(bid[1].get<std::string>());
                            uint64_t timestamp = book_data.contains("ts") ? 
                                std::stoull(book_data["ts"].get<std::string>()) : 
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count();
                            MarketTick tick(symbol, price, quantity, timestamp, true);
                            on_market_tick(tick);
                        }
                    }
                }
                
                // Process asks
                if (book_data["asks"].is_array()) {
                    for (const auto& ask : book_data["asks"]) {
                        if (ask.is_array() && ask.size() >= 2) {
                            utils::Decimal price(ask[0].get<std::string>());
                            utils::Decimal quantity(ask[1].get<std::string>());
                            uint64_t timestamp = book_data.contains("ts") ? 
                                std::stoull(book_data["ts"].get<std::string>()) : 
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count();
                            MarketTick tick(symbol, price, quantity, timestamp, false);
                            on_market_tick(tick);
                        }
                    }
                }
            }
        }
    }
}

void OkxConnector::process_trade_data(const std::string& symbol, const nlohmann::json& data) {
    if (on_trade_tick && data.is_array()) {
        for (const auto& trade : data) {
            if (trade.contains("p") && trade.contains("v") && trade.contains("S")) {
                utils::Decimal price(trade["p"].get<std::string>());
                utils::Decimal quantity(trade["v"].get<std::string>());
                std::string side = trade["S"].get<std::string>(); // "Buy" or "Sell"
                
                uint64_t timestamp = trade.contains("T") ? trade["T"].get<uint64_t>() : 
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                
                TradeTick tick(symbol, price, quantity, timestamp, side);
                on_trade_tick(tick);
            }
        }
    }
}

} // namespace fasttrade::core
