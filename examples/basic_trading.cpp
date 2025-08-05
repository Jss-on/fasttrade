#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    std::cout << "=== FastTrade Basic Trading Example ===" << std::endl;
    
    // Initialize the framework
    fasttrade::initialize();
    
    // Create a trading core with simple configuration
    auto trading_core = TradingCoreBuilder()
        .with_clock_mode(ClockMode::REALTIME)
        .build();
    
    // Set up event callbacks
    TradingCallbacks callbacks;
    callbacks.on_order_filled = [](const LimitOrder& order) {
        std::cout << "✅ Order filled: " << order.client_order_id() 
                  << " (" << order.quantity() << " @ " << order.price() << ")" << std::endl;
    };
    
    callbacks.on_order_cancelled = [](const LimitOrder& order) {
        std::cout << "❌ Order cancelled: " << order.client_order_id() << std::endl;
    };
    
    callbacks.on_trade_executed = [](const Trade& trade) {
        std::cout << "💰 Trade executed: " << trade.symbol 
                  << " " << trade.quantity << " @ " << trade.price << std::endl;
    };
    
    trading_core->set_callbacks(callbacks);
    
    // Initialize and start the trading core
    trading_core->initialize();
    trading_core->start();
    
    std::cout << "Trading core started..." << std::endl;
    
    // Subscribe to market data
    trading_core->subscribe_market_data("BTC-USDT");
    trading_core->subscribe_market_data("ETH-USDT");
    
    // Simulate some market data updates
    auto& btc_book = trading_core->get_order_book("BTC-USDT");
    btc_book.update_bid(Decimal("49900"), Decimal("1.5"), 1001);
    btc_book.update_bid(Decimal("49800"), Decimal("2.3"), 1002);
    btc_book.update_ask(Decimal("50000"), Decimal("1.2"), 1003);
    btc_book.update_ask(Decimal("50100"), Decimal("0.8"), 1004);
    
    // Create and submit orders using the fluent interface
    std::cout << "\nCreating buy orders..." << std::endl;
    
    auto buy_order = OrderBuilder()
        .id("BUY_BTC_001")
        .pair("BTC-USDT")
        .buy(Decimal("0.1"))
        .at_price(Decimal("49950"))
        .build();
    
    auto sell_order = OrderBuilder()
        .id("SELL_BTC_001")
        .pair("BTC-USDT")
        .sell(Decimal("0.05"))
        .at_price(Decimal("50050"))
        .build();
    
    // Submit orders
    if (trading_core->submit_order(buy_order)) {
        std::cout << "✅ Buy order submitted: " << buy_order.client_order_id() << std::endl;
    }
    
    if (trading_core->submit_order(sell_order)) {
        std::cout << "✅ Sell order submitted: " << sell_order.client_order_id() << std::endl;
    }
    
    // Display current order book
    std::cout << "\n=== Current Order Book (BTC-USDT) ===" << std::endl;
    auto bids = btc_book.get_bids(5);
    auto asks = btc_book.get_asks(5);
    
    std::cout << "ASKS:" << std::endl;
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  " << it->price << " | " << it->amount << std::endl;
    }
    std::cout << "  ────────────────────" << std::endl;
    std::cout << "  Best: " << btc_book.best_bid() << " / " << btc_book.best_ask() << std::endl;
    std::cout << "  Spread: " << btc_book.spread() << std::endl;
    std::cout << "  ────────────────────" << std::endl;
    std::cout << "BIDS:" << std::endl;
    for (const auto& bid : bids) {
        std::cout << "  " << bid.price << " | " << bid.amount << std::endl;
    }
    
    // Check active orders
    auto active_orders = trading_core->get_active_orders();
    std::cout << "\nActive orders: " << active_orders.size() << std::endl;
    for (const auto& order : active_orders) {
        std::cout << "  " << order.client_order_id() << ": "
                  << (order.is_buy() ? "BUY" : "SELL") << " "
                  << order.quantity() << " @ " << order.price() << std::endl;
    }
    
    // Simulate some time passing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Cancel an order
    std::cout << "\nCancelling buy order..." << std::endl;
    trading_core->cancel_order("BUY_BTC_001");
    
    // Show portfolio summary
    std::cout << "\n=== Portfolio Summary ===" << std::endl;
    auto positions = trading_core->get_all_positions();
    if (positions.empty()) {
        std::cout << "No positions." << std::endl;
    } else {
        for (const auto& [symbol, position] : positions) {
            std::cout << symbol << ": " << position.quantity 
                      << " @ " << position.average_price << std::endl;
        }
    }
    
    std::cout << "Total P&L: " << trading_core->get_realized_pnl() << std::endl;
    
    // Stop the trading core
    trading_core->stop();
    
    // Clean up
    fasttrade::cleanup();
    
    std::cout << "\nTrading session completed." << std::endl;
    return 0;
}
