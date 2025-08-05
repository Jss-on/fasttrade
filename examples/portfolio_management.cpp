#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <iomanip>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    std::cout << "=== FastTrade Portfolio Management Demo ===" << std::endl;
    
    fasttrade::initialize();
    
    // Set up risk limits
    RiskLimits risk_limits;
    risk_limits.max_position_size = Decimal("10.0");      // Max 10 BTC position
    risk_limits.max_order_size = Decimal("1.0");          // Max 1 BTC per order
    risk_limits.max_daily_loss = Decimal("1000.0");       // Max $1000 daily loss
    risk_limits.max_orders_per_second = 10;
    
    // Create trading core with portfolio management
    auto trading_core = TradingCoreBuilder()
        .with_clock_mode(ClockMode::REALTIME)
        .with_risk_limits(risk_limits)
        .build();
    
    // Set up comprehensive callbacks
    TradingCallbacks callbacks;
    callbacks.on_trade_executed = [&trading_core](const Trade& trade) {
        std::cout << "💰 Trade: " << trade.symbol << " " 
                  << (trade.side == OrderSide::BUY ? "BUY" : "SELL") << " "
                  << trade.quantity << " @ " << trade.price << std::endl;
        
        // Show updated position
        auto position = trading_core->get_position(trade.symbol);
        std::cout << "   New position: " << position.quantity 
                  << " @ avg " << position.average_price << std::endl;
    };
    
    callbacks.on_position_update = [](const Position& position) {
        std::cout << "📈 Position update: " << position.symbol 
                  << " qty=" << position.quantity 
                  << " pnl=" << position.unrealized_pnl << std::endl;
    };
    
    callbacks.on_balance_update = [](const Balance& balance) {
        std::cout << "💰 Balance update: " << balance.currency 
                  << " total=" << balance.total 
                  << " available=" << balance.available << std::endl;
    };
    
    trading_core->set_callbacks(callbacks);
    
    // Initialize and start
    trading_core->initialize();
    trading_core->start();
    
    // Subscribe to multiple symbols
    std::vector<std::string> symbols = {"BTC-USDT", "ETH-USDT", "SOL-USDT"};
    for (const auto& symbol : symbols) {
        trading_core->subscribe_market_data(symbol);
    }
    
    std::cout << "\n=== Simulating Trading Activity ===" << std::endl;
    
    // Create some sample orders and trades
    std::vector<LimitOrder> orders = {
        OrderBuilder().id("BTC_BUY_1").pair("BTC-USDT").buy(Decimal("0.5")).at_price(Decimal("50000")).build(),
        OrderBuilder().id("ETH_BUY_1").pair("ETH-USDT").buy(Decimal("2.0")).at_price(Decimal("3000")).build(),
        OrderBuilder().id("SOL_BUY_1").pair("SOL-USDT").buy(Decimal("10.0")).at_price(Decimal("100")).build(),
    };
    
    // Submit orders
    for (const auto& order : orders) {
        if (trading_core->submit_order(order)) {
            std::cout << "✅ Submitted: " << order.client_order_id() << std::endl;
        }
    }
    
    // Simulate some fills by manually updating positions (in real scenario, this comes from exchange)
    // This demonstrates the portfolio tracking capabilities
    
    std::cout << "\n=== Portfolio Status ===" << std::endl;
    
    // Display all positions
    auto positions = trading_core->get_all_positions();
    std::cout << std::fixed << std::setprecision(6);
    
    if (positions.empty()) {
        std::cout << "No positions currently held." << std::endl;
    } else {
        std::cout << "Symbol       | Quantity    | Avg Price   | Market Value | Unrealized P&L" << std::endl;
        std::cout << "─────────────┼─────────────┼─────────────┼──────────────┼───────────────" << std::endl;
        
        Decimal total_value;
        for (const auto& [symbol, position] : positions) {
            Decimal market_value = position.quantity * position.average_price;
            total_value += market_value;
            
            std::cout << std::left << std::setw(12) << symbol << " | "
                      << std::right << std::setw(11) << position.quantity << " | "
                      << std::setw(11) << position.average_price << " | "
                      << std::setw(12) << market_value << " | "
                      << std::setw(13) << position.unrealized_pnl << std::endl;
        }
        
        std::cout << "─────────────┼─────────────┼─────────────┼──────────────┼───────────────" << std::endl;
        std::cout << "Total Portfolio Value: " << total_value << " USDT" << std::endl;
    }
    
    // Display balances
    std::cout << "\n=== Account Balances ===" << std::endl;
    auto balances = trading_core->get_all_balances();
    
    if (balances.empty()) {
        std::cout << "No balances available." << std::endl;
    } else {
        std::cout << "Currency | Total       | Available   | Locked" << std::endl;
        std::cout << "─────────┼─────────────┼─────────────┼─────────────" << std::endl;
        
        for (const auto& [currency, balance] : balances) {
            std::cout << std::left << std::setw(8) << currency << " | "
                      << std::right << std::setw(11) << balance.total << " | "
                      << std::setw(11) << balance.available << " | "
                      << std::setw(11) << balance.locked << std::endl;
        }
    }
    
    // P&L Summary
    std::cout << "\n=== P&L Summary ===" << std::endl;
    std::cout << "Realized P&L:   " << trading_core->get_realized_pnl() << " USDT" << std::endl;
    std::cout << "Unrealized P&L: " << trading_core->get_unrealized_pnl() << " USDT" << std::endl;
    std::cout << "Daily P&L:      " << trading_core->get_daily_pnl() << " USDT" << std::endl;
    std::cout << "Total P&L:      " << (trading_core->get_realized_pnl() + trading_core->get_unrealized_pnl()) << " USDT" << std::endl;
    
    // Risk Analysis
    std::cout << "\n=== Risk Analysis ===" << std::endl;
    const auto& limits = trading_core->get_risk_limits();
    std::cout << "Max Position Size: " << limits.max_position_size << std::endl;
    std::cout << "Max Order Size:    " << limits.max_order_size << std::endl;
    std::cout << "Max Daily Loss:    " << limits.max_daily_loss << std::endl;
    std::cout << "Max Orders/sec:    " << limits.max_orders_per_second << std::endl;
    
    // Test risk limits
    auto large_order = OrderBuilder()
        .id("LARGE_ORDER")
        .pair("BTC-USDT")
        .buy(Decimal("15.0"))  // Exceeds max position size
        .at_price(Decimal("50000"))
        .build();
    
    if (trading_core->check_risk_limits(large_order)) {
        std::cout << "✅ Large order passes risk checks" << std::endl;
    } else {
        std::cout << "❌ Large order blocked by risk limits" << std::endl;
    }
    
    // Trade History
    std::cout << "\n=== Trade History ===" << std::endl;
    auto trades = trading_core->get_trade_history(10);
    
    if (trades.empty()) {
        std::cout << "No trades executed yet." << std::endl;
    } else {
        std::cout << "Time                 | Symbol    | Side | Quantity   | Price      | Fee" << std::endl;
        std::cout << "─────────────────────┼───────────┼──────┼────────────┼────────────┼────────" << std::endl;
        
        for (const auto& trade : trades) {
            std::cout << Clock::to_milliseconds(trade.timestamp) << " | "
                      << std::left << std::setw(9) << trade.symbol << " | "
                      << std::setw(4) << (trade.side == OrderSide::BUY ? "BUY" : "SELL") << " | "
                      << std::right << std::setw(10) << trade.quantity << " | "
                      << std::setw(10) << trade.price << " | "
                      << std::setw(6) << trade.fee << std::endl;
        }
    }
    
    // Export portfolio state
    std::cout << "\n=== Portfolio Export ===" << std::endl;
    auto state_json = trading_core->export_state();
    std::cout << "Portfolio state exported to JSON (" << state_json.length() << " characters)" << std::endl;
    
    // Statistics
    std::cout << "\n=== Trading Statistics ===" << std::endl;
    auto stats = trading_core->get_statistics();
    std::cout << stats << std::endl;
    
    trading_core->stop();
    fasttrade::cleanup();
    
    std::cout << "\nPortfolio management demo completed." << std::endl;
    return 0;
}
