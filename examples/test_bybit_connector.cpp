#include "fasttrade/core/market_data_manager.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>

using namespace fasttrade::core;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== Bybit Connector Test ===" << std::endl;
    
    // Create Bybit connector
    BybitConnector connector;
    
    // Set up callbacks
    int market_tick_count = 0;
    int trade_tick_count = 0;
    
    connector.on_market_tick = [&market_tick_count](const MarketTick& tick) {
        market_tick_count++;
        std::cout << "[Market Tick #" << market_tick_count << "] " 
                  << tick.symbol << " - Price: " << tick.price.to_string() 
                  << ", Qty: " << tick.quantity.to_string()
                  << ", Side: " << (tick.is_bid ? "BID" : "ASK") 
                  << ", Time: " << tick.timestamp << std::endl;
    };
    
    connector.on_trade_tick = [&trade_tick_count](const TradeTick& tick) {
        trade_tick_count++;
        std::cout << "[Trade Tick #" << trade_tick_count << "] " 
                  << tick.symbol << " - Price: " << tick.price.to_string() 
                  << ", Qty: " << tick.quantity.to_string()
                  << ", Side: " << tick.side 
                  << ", Time: " << tick.timestamp << std::endl;
    };
    
    connector.on_error = [](const std::string& error) {
        std::cerr << "[ERROR] " << error << std::endl;
    };
    
    connector.on_disconnect = []() {
        std::cout << "[INFO] Bybit connector disconnected" << std::endl;
    };
    
    // Test connection
    std::cout << "Connecting to Bybit..." << std::endl;
    if (!connector.connect()) {
        std::cerr << "Failed to connect to Bybit" << std::endl;
        return 1;
    }
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!connector.is_connected()) {
        std::cerr << "Connection not established" << std::endl;
        return 1;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    
    // Test subscriptions
    std::cout << "Subscribing to BTC-USDT orderbook..." << std::endl;
    if (!connector.subscribe_orderbook("BTCUSDT")) {
        std::cerr << "Failed to subscribe to orderbook" << std::endl;
    }
    
    std::cout << "Subscribing to BTC-USDT trades..." << std::endl;
    if (!connector.subscribe_trades("BTCUSDT")) {
        std::cerr << "Failed to subscribe to trades" << std::endl;
    }
    
    std::cout << "Subscribing to ETH-USDT orderbook..." << std::endl;
    if (!connector.subscribe_orderbook("ETHUSDT")) {
        std::cerr << "Failed to subscribe to ETH orderbook" << std::endl;
    }
    
    // Run for 30 seconds or until interrupted
    std::cout << "Listening for market data for 30 seconds (Ctrl+C to stop early)..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();
    
    while (running) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        
        if (elapsed.count() >= 30) {
            std::cout << "30 seconds elapsed, stopping..." << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Test unsubscription
    std::cout << "Unsubscribing from BTC-USDT..." << std::endl;
    connector.unsubscribe_orderbook("BTCUSDT");
    connector.unsubscribe_trades("BTCUSDT");
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Disconnect
    std::cout << "Disconnecting..." << std::endl;
    connector.disconnect();
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Market ticks received: " << market_tick_count << std::endl;
    std::cout << "Trade ticks received: " << trade_tick_count << std::endl;
    
    if (market_tick_count > 0 && trade_tick_count > 0) {
        std::cout << "✓ Bybit connector test PASSED!" << std::endl;
    } else {
        std::cout << "✗ Bybit connector test FAILED - No data received" << std::endl;
        return 1;
    }
    
    return 0;
}
