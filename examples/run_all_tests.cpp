#include "fasttrade/core/market_data_manager.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace fasttrade::core;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

template<typename ConnectorType>
bool test_connector(const std::string& exchange_name, ConnectorType& connector) {
    std::cout << "\n=== Testing " << exchange_name << " Connector ===" << std::endl;
    
    // Set up callbacks
    int market_tick_count = 0;
    int trade_tick_count = 0;
    bool connection_error = false;
    
    connector.on_market_tick = [&market_tick_count, &exchange_name](const MarketTick& tick) {
        market_tick_count++;
        if (market_tick_count <= 5) { // Only show first 5 ticks to avoid spam
            std::cout << "[" << exchange_name << " Market] " 
                      << tick.symbol << " - " << tick.price.to_string() 
                      << " (" << (tick.is_bid ? "BID" : "ASK") << ")" << std::endl;
        }
    };
    
    connector.on_trade_tick = [&trade_tick_count, &exchange_name](const TradeTick& tick) {
        trade_tick_count++;
        if (trade_tick_count <= 5) { // Only show first 5 ticks to avoid spam
            std::cout << "[" << exchange_name << " Trade] " 
                      << tick.symbol << " - " << tick.price.to_string() 
                      << " (" << tick.side << ")" << std::endl;
        }
    };
    
    connector.on_error = [&connection_error, &exchange_name](const std::string& error) {
        std::cerr << "[" << exchange_name << " ERROR] " << error << std::endl;
        connection_error = true;
    };
    
    connector.on_disconnect = [&exchange_name]() {
        std::cout << "[" << exchange_name << "] Disconnected" << std::endl;
    };
    
    // Test connection
    std::cout << "Connecting to " << exchange_name << "..." << std::endl;
    if (!connector.connect()) {
        std::cerr << "Failed to connect to " << exchange_name << std::endl;
        return false;
    }
    
    // Wait for connection
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    if (!connector.is_connected()) {
        std::cerr << exchange_name << " connection not established" << std::endl;
        return false;
    }
    
    std::cout << exchange_name << " connected successfully!" << std::endl;
    
    // Test subscriptions
    std::cout << "Subscribing to market data..." << std::endl;
    if (!connector.subscribe_orderbook("BTC-USDT")) {
        std::cerr << "Failed to subscribe to " << exchange_name << " orderbook" << std::endl;
    }
    
    if (!connector.subscribe_trades("BTC-USDT")) {
        std::cerr << "Failed to subscribe to " << exchange_name << " trades" << std::endl;
    }
    
    // Listen for data for 15 seconds
    std::cout << "Listening for 15 seconds..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();
    
    while (running && !connection_error) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        
        if (elapsed.count() >= 15) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean up
    connector.unsubscribe_orderbook("BTC-USDT");
    connector.unsubscribe_trades("BTC-USDT");
    connector.disconnect();
    
    std::cout << exchange_name << " Results: Market=" << market_tick_count 
              << ", Trades=" << trade_tick_count << std::endl;
    
    // Consider test passed if we got some data and no critical errors
    bool passed = (market_tick_count > 0 || trade_tick_count > 0) && !connection_error;
    std::cout << (passed ? "✓ PASSED" : "✗ FAILED") << std::endl;
    
    return passed;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== FastTrade WebSocket Connector Tests ===" << std::endl;
    std::cout << "Testing real WebSocket connections to exchanges..." << std::endl;
    
    int passed_tests = 0;
    int total_tests = 3;
    
    // Test Binance
    try {
        BinanceConnector binance_connector;
        if (test_connector("Binance", binance_connector)) {
            passed_tests++;
        }
    } catch (const std::exception& e) {
        std::cerr << "Binance test failed with exception: " << e.what() << std::endl;
    }
    
    if (!running) return 1;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test Bybit
    try {
        BybitConnector bybit_connector;
        if (test_connector("Bybit", bybit_connector)) {
            passed_tests++;
        }
    } catch (const std::exception& e) {
        std::cerr << "Bybit test failed with exception: " << e.what() << std::endl;
    }
    
    if (!running) return 1;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test OKX
    try {
        OkxConnector okx_connector;
        if (test_connector("OKX", okx_connector)) {
            passed_tests++;
        }
    } catch (const std::exception& e) {
        std::cerr << "OKX test failed with exception: " << e.what() << std::endl;
    }
    
    // Final results
    std::cout << "\n=== Final Test Results ===" << std::endl;
    std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "🎉 All WebSocket connectors working correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "⚠️  Some connectors need attention." << std::endl;
        return 1;
    }
}
