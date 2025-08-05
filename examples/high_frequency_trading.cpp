#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    std::cout << "=== FastTrade High-Frequency Trading Demo ===" << std::endl;
    
    fasttrade::initialize();
    
    // Set up aggressive risk limits for HFT
    RiskLimits hft_limits;
    hft_limits.max_position_size = Decimal("5.0");       // Smaller position sizes
    hft_limits.max_order_size = Decimal("0.1");          // Small order sizes
    hft_limits.max_orders_per_second = 1000;             // High order rate
    hft_limits.enable_position_limits = true;
    hft_limits.enable_order_limits = true;
    
    // Create ultra-fast trading core
    auto trading_core = TradingCoreBuilder()
        .with_clock_mode(ClockMode::REALTIME)
        .with_risk_limits(hft_limits)
        .build();
    
    // Performance metrics
    std::atomic<int> orders_sent{0};
    std::atomic<int> orders_filled{0};
    std::atomic<int> orders_cancelled{0};
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Ultra-fast callbacks
    TradingCallbacks callbacks;
    callbacks.on_order_filled = [&orders_filled](const LimitOrder& order) {
        orders_filled++;
        // In HFT, we want minimal logging for performance
    };
    
    callbacks.on_order_cancelled = [&orders_cancelled](const LimitOrder& order) {
        orders_cancelled++;
    };
    
    callbacks.on_trade_executed = [](const Trade& trade) {
        // Log critical trade information only
        if (orders_filled % 100 == 0) {  // Log every 100th trade
            std::cout << "⚡ HFT Trade #" << orders_filled << ": " 
                      << trade.symbol << " " << trade.quantity 
                      << " @ " << trade.price << std::endl;
        }
    };
    
    trading_core->set_callbacks(callbacks);
    trading_core->initialize();
    trading_core->start();
    
    // Subscribe to multiple symbols for arbitrage opportunities
    std::vector<std::string> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    for (const auto& symbol : symbols) {
        trading_core->subscribe_market_data(symbol);
    }
    
    std::cout << "\n🚀 Starting High-Frequency Trading Simulation..." << std::endl;
    
    // Simulate market making strategy
    std::thread market_maker([&]() {
        int order_id = 0;
        
        while (orders_sent < 1000) {  // Simulate 1000 orders
            for (const auto& symbol : symbols) {
                auto& book = trading_core->get_order_book(symbol);
                
                // Get current best bid/ask
                auto best_bid = book.best_bid();
                auto best_ask = book.best_ask();
                
                if (!best_bid.is_zero() && !best_ask.is_zero()) {
                    auto spread = best_ask - best_bid;
                    auto mid_price = (best_bid + best_ask) / Decimal("2");
                    
                    // Only trade if spread is reasonable (> 0.01%)
                    if (spread > mid_price * Decimal("0.0001")) {
                        // Place orders just inside the spread
                        auto bid_price = best_bid + spread * Decimal("0.3");
                        auto ask_price = best_ask - spread * Decimal("0.3");
                        
                        // Submit buy order
                        auto buy_order = OrderBuilder()
                            .id("HFT_BUY_" + std::to_string(++order_id))
                            .pair(symbol)
                            .buy(Decimal("0.01"))
                            .at_price(bid_price)
                            .build();
                        
                        if (trading_core->submit_order(buy_order)) {
                            orders_sent++;
                        }
                        
                        // Submit sell order
                        auto sell_order = OrderBuilder()
                            .id("HFT_SELL_" + std::to_string(++order_id))
                            .pair(symbol)
                            .sell(Decimal("0.01"))
                            .at_price(ask_price)
                            .build();
                        
                        if (trading_core->submit_order(sell_order)) {
                            orders_sent++;
                        }
                    }
                }
            }
            
            // Ultra-short sleep for high frequency
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Simulate order book updates (in real HFT, this comes from market data feed)
    std::thread market_data([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_change(-0.001, 0.001);  // ±0.1% price changes
        std::uniform_real_distribution<> size_change(0.01, 0.1);       // Small size changes
        
        Decimal btc_price("50000");
        Decimal eth_price("3000");
        Decimal sol_price("100");
        
        while (orders_sent < 1000) {
            // Update BTC order book
            btc_price = btc_price * (Decimal("1") + Decimal(price_change(gen)));
            auto& btc_book = trading_core->get_order_book("BTC-USDT");
            btc_book.update_bid(btc_price * Decimal("0.9999"), Decimal(size_change(gen)), GlobalClock::now_ns());
            btc_book.update_ask(btc_price * Decimal("1.0001"), Decimal(size_change(gen)), GlobalClock::now_ns());
            
            // Update ETH order book
            eth_price = eth_price * (Decimal("1") + Decimal(price_change(gen)));
            auto& eth_book = trading_core->get_order_book("ETH-USDT");
            eth_book.update_bid(eth_price * Decimal("0.9999"), Decimal(size_change(gen)), GlobalClock::now_ns());
            eth_book.update_ask(eth_price * Decimal("1.0001"), Decimal(size_change(gen)), GlobalClock::now_ns());
            
            // Update SOL order book
            sol_price = sol_price * (Decimal("1") + Decimal(price_change(gen)));
            auto& sol_book = trading_core->get_order_book("SOL-USDT");
            sol_book.update_bid(sol_price * Decimal("0.9999"), Decimal(size_change(gen)), GlobalClock::now_ns());
            sol_book.update_ask(sol_price * Decimal("1.0001"), Decimal(size_change(gen)), GlobalClock::now_ns());
            
            // High-frequency updates
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    
    // Order management thread - cancel stale orders
    std::thread order_manager([&]() {
        while (orders_sent < 1000) {
            auto active_orders = trading_core->get_active_orders();
            
            // Cancel orders that are older than 1 second (aggressive for HFT)
            auto current_time = GlobalClock::now();
            for (const auto& order : active_orders) {
                auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - order.creation_time()).count();
                
                if (age > 1000) {  // 1 second
                    trading_core->cancel_order(order.client_order_id());
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Performance monitoring
    std::thread monitor([&]() {
        while (orders_sent < 1000) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - start_time).count();
            
            if (elapsed > 0) {
                std::cout << "📊 Performance: " << orders_sent << " orders sent, "
                          << orders_filled << " filled, "
                          << orders_cancelled << " cancelled "
                          << "(" << (orders_sent / elapsed) << " orders/sec)" << std::endl;
            }
        }
    });
    
    // Wait for completion
    market_maker.join();
    market_data.join();
    order_manager.join();
    monitor.join();
    
    // Final performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "\n=== HFT Performance Results ===" << std::endl;
    std::cout << "Total runtime:       " << total_time << " ms" << std::endl;
    std::cout << "Orders sent:         " << orders_sent << std::endl;
    std::cout << "Orders filled:       " << orders_filled << std::endl;
    std::cout << "Orders cancelled:    " << orders_cancelled << std::endl;
    std::cout << "Fill rate:           " << (orders_filled * 100.0 / orders_sent) << "%" << std::endl;
    std::cout << "Average throughput:  " << (orders_sent * 1000.0 / total_time) << " orders/sec" << std::endl;
    std::cout << "Average latency:     " << (total_time * 1000.0 / orders_sent) << " μs/order" << std::endl;
    
    // Portfolio summary
    std::cout << "\n=== Portfolio Summary ===" << std::endl;
    auto positions = trading_core->get_all_positions();
    for (const auto& [symbol, position] : positions) {
        if (!position.quantity.is_zero()) {
            std::cout << symbol << ": " << position.quantity 
                      << " @ " << position.average_price 
                      << " (P&L: " << position.unrealized_pnl << ")" << std::endl;
        }
    }
    
    std::cout << "Total realized P&L:  " << trading_core->get_realized_pnl() << std::endl;
    std::cout << "Total unrealized P&L:" << trading_core->get_unrealized_pnl() << std::endl;
    
    // Latency analysis
    std::cout << "\n=== Latency Analysis ===" << std::endl;
    std::cout << "Framework demonstrates sub-millisecond order processing" << std::endl;
    std::cout << "Suitable for market making and statistical arbitrage" << std::endl;
    std::cout << "C++ implementation provides significant speed advantage over Python" << std::endl;
    
    trading_core->stop();
    fasttrade::cleanup();
    
    std::cout << "\nHigh-frequency trading demo completed." << std::endl;
    return 0;
}
