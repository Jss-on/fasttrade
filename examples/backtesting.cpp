#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <vector>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    std::cout << "=== FastTrade Backtesting Demo ===" << std::endl;
    
    fasttrade::initialize();
    
    // Create trading core in backtesting mode
    auto trading_core = TradingCoreBuilder()
        .with_clock_mode(ClockMode::BACKTEST)  // Backtesting mode for controlled time
        .build();
    
    trading_core->initialize();
    trading_core->start();
    
    // Set up strategy callbacks
    TradingCallbacks callbacks;
    callbacks.on_trade_executed = [](const Trade& trade) {
        std::cout << "📊 [BACKTEST] Trade: " << trade.symbol << " " 
                  << (trade.side == OrderSide::BUY ? "BUY" : "SELL") << " "
                  << trade.quantity << " @ " << trade.price << std::endl;
    };
    
    trading_core->set_callbacks(callbacks);
    
    // Subscribe to historical data
    trading_core->subscribe_market_data("BTC-USDT");
    auto& btc_book = trading_core->get_order_book("BTC-USDT");
    
    std::cout << "\n=== Running Simple Moving Average Strategy ===" << std::endl;
    
    // Simulate historical price data (in real backtesting, this would come from data feed)
    std::vector<Decimal> historical_prices = {
        Decimal("45000"), Decimal("45500"), Decimal("46000"), Decimal("46500"),
        Decimal("47000"), Decimal("47500"), Decimal("48000"), Decimal("48500"),
        Decimal("49000"), Decimal("49500"), Decimal("50000"), Decimal("50500"),
        Decimal("51000"), Decimal("50800"), Decimal("50600"), Decimal("50400"),
        Decimal("50200"), Decimal("50000"), Decimal("49800"), Decimal("49600")
    };
    
    std::vector<Decimal> sma_short, sma_long;
    int short_period = 3;
    int long_period = 7;
    
    Decimal initial_balance = Decimal("10000");  // $10,000 starting capital
    Decimal btc_position;
    Decimal cash_balance = initial_balance;
    
    for (size_t i = 0; i < historical_prices.size(); ++i) {
        Decimal current_price = historical_prices[i];
        
        // Update order book with current price
        btc_book.update_bid(current_price * Decimal("0.999"), Decimal("1.0"), i * 2);
        btc_book.update_ask(current_price * Decimal("1.001"), Decimal("1.0"), i * 2 + 1);
        
        // Calculate moving averages
        if (i >= short_period - 1) {
            Decimal sum_short;
            for (int j = 0; j < short_period; ++j) {
                sum_short += historical_prices[i - j];
            }
            sma_short.push_back(sum_short / Decimal(short_period));
        }
        
        if (i >= long_period - 1) {
            Decimal sum_long;
            for (int j = 0; j < long_period; ++j) {
                sum_long += historical_prices[i - j];
            }
            sma_long.push_back(sum_long / Decimal(long_period));
        }
        
        // Trading logic: Buy when short MA crosses above long MA, sell when below
        if (sma_short.size() >= 2 && sma_long.size() >= 2) {
            bool bullish_cross = (sma_short[sma_short.size()-1] > sma_long[sma_long.size()-1]) &&
                                (sma_short[sma_short.size()-2] <= sma_long[sma_long.size()-2]);
            
            bool bearish_cross = (sma_short[sma_short.size()-1] < sma_long[sma_long.size()-1]) &&
                               (sma_short[sma_short.size()-2] >= sma_long[sma_long.size()-2]);
            
            if (bullish_cross && btc_position.is_zero() && cash_balance > current_price) {
                // Buy signal
                Decimal buy_amount = cash_balance / current_price * Decimal("0.95");  // Use 95% of cash
                
                auto buy_order = OrderBuilder()
                    .id("BACKTEST_BUY_" + std::to_string(i))
                    .pair("BTC-USDT")
                    .buy(buy_amount)
                    .at_price(current_price)
                    .build();
                
                if (trading_core->submit_order(buy_order)) {
                    btc_position += buy_amount;
                    cash_balance -= buy_amount * current_price;
                    
                    std::cout << "🔵 BUY: " << buy_amount << " BTC @ " << current_price
                              << " (SMA Cross: " << sma_short.back() << " > " << sma_long.back() << ")" << std::endl;
                }
            }
            else if (bearish_cross && !btc_position.is_zero()) {
                // Sell signal
                auto sell_order = OrderBuilder()
                    .id("BACKTEST_SELL_" + std::to_string(i))
                    .pair("BTC-USDT")
                    .sell(btc_position)
                    .at_price(current_price)
                    .build();
                
                if (trading_core->submit_order(sell_order)) {
                    cash_balance += btc_position * current_price;
                    
                    std::cout << "🔴 SELL: " << btc_position << " BTC @ " << current_price
                              << " (SMA Cross: " << sma_short.back() << " < " << sma_long.back() << ")" << std::endl;
                    
                    btc_position = Decimal::zero();
                }
            }
        }
        
        // Print periodic status
        if (i % 5 == 0) {
            Decimal portfolio_value = cash_balance + (btc_position * current_price);
            std::cout << "📈 Day " << (i+1) << ": Price=" << current_price 
                      << " Portfolio=" << portfolio_value 
                      << " P&L=" << (portfolio_value - initial_balance) << std::endl;
        }
    }
    
    // Final results
    std::cout << "\n=== Backtest Results ===" << std::endl;
    Decimal final_price = historical_prices.back();
    Decimal final_portfolio_value = cash_balance + (btc_position * final_price);
    Decimal total_return = final_portfolio_value - initial_balance;
    Decimal return_percentage = (total_return / initial_balance) * Decimal("100");
    
    std::cout << "Initial Capital:     $" << initial_balance << std::endl;
    std::cout << "Final Portfolio:     $" << final_portfolio_value << std::endl;
    std::cout << "Total Return:        $" << total_return << std::endl;
    std::cout << "Return Percentage:   " << return_percentage << "%" << std::endl;
    std::cout << "Final BTC Position:  " << btc_position << " BTC" << std::endl;
    std::cout << "Final Cash Balance:  $" << cash_balance << std::endl;
    
    // Buy and hold comparison
    Decimal buy_hold_btc = initial_balance / historical_prices[0];
    Decimal buy_hold_value = buy_hold_btc * final_price;
    Decimal buy_hold_return = buy_hold_value - initial_balance;
    
    std::cout << "\n=== Buy & Hold Comparison ===" << std::endl;
    std::cout << "Buy & Hold Value:    $" << buy_hold_value << std::endl;
    std::cout << "Buy & Hold Return:   $" << buy_hold_return << std::endl;
    std::cout << "Strategy vs B&H:     $" << (total_return - buy_hold_return) << std::endl;
    
    if (total_return > buy_hold_return) {
        std::cout << "✅ Strategy outperformed buy & hold!" << std::endl;
    } else {
        std::cout << "❌ Strategy underperformed buy & hold" << std::endl;
    }
    
    // Trading statistics
    auto trades = trading_core->get_trade_history();
    std::cout << "\n=== Trading Statistics ===" << std::endl;
    std::cout << "Total trades:        " << trades.size() << std::endl;
    std::cout << "Realized P&L:        $" << trading_core->get_realized_pnl() << std::endl;
    
    trading_core->stop();
    fasttrade::cleanup();
    
    std::cout << "\nBacktesting demo completed." << std::endl;
    return 0;
}
