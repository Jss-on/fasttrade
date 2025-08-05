#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace fasttrade;
using namespace fasttrade::core;

int main() {
    std::cout << "FastTrade Market Data Subscription Example" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Initialize the fasttrade framework
    fasttrade::initialize();
    
    try {
        // Create trading core
        auto trading_core = TradingCoreBuilder()
            .with_clock_mode(ClockMode::REALTIME)
            .build();
        
        // Set up callbacks for market data events
        TradingCallbacks callbacks;
        callbacks.on_market_data = [](const std::string& symbol, const utils::Decimal& price, 
                                    const utils::Decimal& quantity, bool is_bid) {
            std::cout << "[Market Data] " << symbol << " - " 
                      << (is_bid ? "BID" : "ASK") << ": $" << price.to_string() 
                      << " @ " << quantity.to_string() << std::endl;
        };
        
        callbacks.on_trade = [](const std::string& symbol, const utils::Decimal& price,
                              const utils::Decimal& quantity, bool is_buy) {
            std::cout << "[Trade] " << symbol << " - " 
                      << (is_buy ? "BUY" : "SELL") << ": $" << price.to_string() 
                      << " @ " << quantity.to_string() << std::endl;
        };
        
        trading_core->set_callbacks(callbacks);
        
        // Initialize market data connections for all supported exchanges
        std::vector<MarketDataManager::Exchange> exchanges = {
            MarketDataManager::Exchange::BINANCE,
            MarketDataManager::Exchange::BYBIT,
            MarketDataManager::Exchange::OKX
        };
        
        std::cout << "Initializing market data connections..." << std::endl;
        if (!trading_core->initialize_market_data(exchanges)) {
            std::cerr << "Failed to initialize market data connections!" << std::endl;
            return 1;
        }
        
        // Check connection status
        if (trading_core->is_market_data_connected()) {
            std::cout << "✓ Market data connections established" << std::endl;
        } else {
            std::cout << "✗ No market data connections available" << std::endl;
            return 1;
        }
        
        // Start the trading core
        trading_core->start();
        std::cout << "✓ Trading core started" << std::endl;
        
        // Subscribe to market data for BTC-USDT and ETH-USDT
        std::cout << "\nSubscribing to market data..." << std::endl;
        
        // Subscribe to BTC-USDT on all exchanges
        trading_core->subscribe_market_data("BTC-USDT");
        std::cout << "✓ Subscribed to BTC-USDT market data" << std::endl;
        
        // Subscribe to ETH-USDT on all exchanges
        trading_core->subscribe_market_data("ETH-USDT");
        std::cout << "✓ Subscribed to ETH-USDT market data" << std::endl;
        
        // Subscribe to additional symbol on specific exchanges only
        std::vector<MarketDataManager::Exchange> binance_only = {MarketDataManager::Exchange::BINANCE};
        trading_core->subscribe_market_data("ADA-USDT", binance_only);
        std::cout << "✓ Subscribed to ADA-USDT market data (Binance only)" << std::endl;
        
        // Display current subscriptions
        auto subscribed_symbols = trading_core->get_subscribed_symbols();
        std::cout << "\nCurrently subscribed symbols:" << std::endl;
        for (const auto& symbol : subscribed_symbols) {
            std::cout << "  - " << symbol << std::endl;
        }
        
        std::cout << "\nMarket data subscription active. Press Ctrl+C to stop..." << std::endl;
        std::cout << "Note: This is a mock implementation for demonstration purposes." << std::endl;
        std::cout << "In a production environment, you would see real market data here." << std::endl;
        
        // Let it run for a bit to show market data (in a real implementation)
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Demonstrate unsubscribing
        std::cout << "\nUnsubscribing from ADA-USDT..." << std::endl;
        trading_core->unsubscribe_market_data("ADA-USDT");
        
        // Show updated subscriptions
        subscribed_symbols = trading_core->get_subscribed_symbols();
        std::cout << "Updated subscribed symbols:" << std::endl;
        for (const auto& symbol : subscribed_symbols) {
            std::cout << "  - " << symbol << std::endl;
        }
        
        // Stop the trading core
        std::cout << "\nShutting down..." << std::endl;
        trading_core->stop();
        std::cout << "✓ Trading core stopped" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        fasttrade::cleanup();
        return 1;
    }
    
    // Clean up the fasttrade framework
    fasttrade::cleanup();
    std::cout << "✓ FastTrade framework cleaned up" << std::endl;
    
    return 0;
}
