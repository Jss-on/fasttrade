#include <fasttrade/fasttrade.hpp>
#include <iostream>
#include <vector>
#include <random>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    std::cout << "=== FastTrade Order Book Demo ===" << std::endl;
    
    fasttrade::initialize();
    
    // Create order book manager
    OrderBookManager manager;
    auto& book = manager.get_order_book("BTC-USDT");
    
    // Set up update callback
    book.register_update_callback([](const std::string& symbol) {
        std::cout << "📊 Order book updated for " << symbol << std::endl;
    });
    
    std::cout << "Building order book with sample data..." << std::endl;
    
    // Add sample bid levels
    std::vector<std::tuple<Decimal, Decimal, int64_t>> bids = {
        {Decimal("49900"), Decimal("1.5"), 1001},
        {Decimal("49850"), Decimal("2.3"), 1002},
        {Decimal("49800"), Decimal("1.8"), 1003},
        {Decimal("49750"), Decimal("3.2"), 1004},
        {Decimal("49700"), Decimal("0.9"), 1005}
    };
    
    // Add sample ask levels
    std::vector<std::tuple<Decimal, Decimal, int64_t>> asks = {
        {Decimal("50000"), Decimal("1.2"), 2001},
        {Decimal("50050"), Decimal("0.8"), 2002},
        {Decimal("50100"), Decimal("2.1"), 2003},
        {Decimal("50150"), Decimal("1.5"), 2004},
        {Decimal("50200"), Decimal("2.8"), 2005}
    };
    
    // Apply updates in batch
    book.apply_updates(bids, asks, 3000);
    
    // Display order book
    std::cout << "\n=== Order Book Snapshot ===" << std::endl;
    auto ask_levels = book.get_asks(10);
    auto bid_levels = book.get_bids(10);
    
    std::cout << "ASKS (ascending price):" << std::endl;
    for (auto it = ask_levels.rbegin(); it != ask_levels.rend(); ++it) {
        std::cout << "  " << it->price << " | " << it->amount 
                  << " | " << it->price * it->amount << " USDT" << std::endl;
    }
    
    std::cout << "  ──────────────────────────────────" << std::endl;
    std::cout << "  Mid Price: " << book.mid_price() << std::endl;
    std::cout << "  Spread: " << book.spread() << " (" 
              << (book.spread() / book.mid_price() * Decimal("100")) << "%)" << std::endl;
    std::cout << "  ──────────────────────────────────" << std::endl;
    
    std::cout << "BIDS (descending price):" << std::endl;
    for (const auto& bid : bid_levels) {
        std::cout << "  " << bid.price << " | " << bid.amount 
                  << " | " << bid.price * bid.amount << " USDT" << std::endl;
    }
    
    // Demonstrate market impact analysis
    std::cout << "\n=== Market Impact Analysis ===" << std::endl;
    
    std::vector<Decimal> order_sizes = {
        Decimal("0.1"), Decimal("0.5"), Decimal("1.0"), 
        Decimal("2.0"), Decimal("5.0")
    };
    
    for (const auto& size : order_sizes) {
        auto buy_impact = book.get_impact_price(true, size);
        auto sell_impact = book.get_impact_price(false, size);
        
        std::cout << "Order size " << size << " BTC:" << std::endl;
        std::cout << "  Buy impact price:  " << buy_impact << std::endl;
        std::cout << "  Sell impact price: " << sell_impact << std::endl;
        std::cout << "  Buy slippage:  " << ((buy_impact - book.best_ask()) / book.best_ask() * Decimal("100")) << "%" << std::endl;
        std::cout << "  Sell slippage: " << ((book.best_bid() - sell_impact) / book.best_bid() * Decimal("100")) << "%" << std::endl;
        std::cout << std::endl;
    }
    
    // Simulate real-time updates
    std::cout << "=== Simulating Real-time Updates ===" << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_change(-0.01, 0.01);  // ±1% price movement
    std::uniform_real_distribution<> size_change(0.1, 2.0);     // Random size changes
    
    for (int i = 0; i < 10; ++i) {
        // Random bid update
        auto base_bid = book.best_bid();
        auto new_bid_price = base_bid * (Decimal("1.0") + Decimal(price_change(gen)));
        auto new_bid_size = Decimal(size_change(gen));
        
        // Random ask update
        auto base_ask = book.best_ask();
        auto new_ask_price = base_ask * (Decimal("1.0") + Decimal(price_change(gen)));
        auto new_ask_size = Decimal(size_change(gen));
        
        book.update_bid(new_bid_price, new_bid_size, 4000 + i);
        book.update_ask(new_ask_price, new_ask_size, 5000 + i);
        
        std::cout << "Update " << (i+1) << ": Best bid/ask = " 
                  << book.best_bid() << " / " << book.best_ask() 
                  << ", Spread = " << book.spread() << std::endl;
    }
    
    // Volume analysis
    std::cout << "\n=== Volume Analysis ===" << std::endl;
    auto volume_50k = book.get_volume_at_price(true, Decimal("50000"));
    auto volume_49k = book.get_volume_at_price(false, Decimal("49000"));
    
    std::cout << "Volume available at 50,000 USDT and below: " << volume_50k << " BTC" << std::endl;
    std::cout << "Volume available at 49,000 USDT and above: " << volume_49k << " BTC" << std::endl;
    
    // Export order book state
    std::cout << "\n=== Order Book JSON Export ===" << std::endl;
    auto json_snapshot = book.to_json(5);
    std::cout << json_snapshot << std::endl;
    
    // Validate order book integrity
    if (book.is_valid()) {
        std::cout << "\n✅ Order book integrity check passed" << std::endl;
    } else {
        std::cout << "\n❌ Order book integrity check failed" << std::endl;
    }
    
    fasttrade::cleanup();
    
    std::cout << "\nOrder book demo completed." << std::endl;
    return 0;
}
