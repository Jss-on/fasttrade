#include "fasttrade/core/order_book.hpp"
#include <sstream>
#include <algorithm>
#include <shared_mutex>

namespace fasttrade::core {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol), last_update_id_(0) {
    last_update_time_ = GlobalClock::now();
}

void OrderBook::update_bid(const utils::Decimal& price, const utils::Decimal& amount, int64_t update_id) {
    bids_.update(price, amount, update_id);
    last_update_id_ = update_id;
    last_update_time_ = GlobalClock::now();
    notify_update();
}

void OrderBook::update_ask(const utils::Decimal& price, const utils::Decimal& amount, int64_t update_id) {
    asks_.update(price, amount, update_id);
    last_update_id_ = update_id;
    last_update_time_ = GlobalClock::now();
    notify_update();
}

void OrderBook::apply_updates(const std::vector<std::tuple<utils::Decimal, utils::Decimal, int64_t>>& bids,
                             const std::vector<std::tuple<utils::Decimal, utils::Decimal, int64_t>>& asks,
                             int64_t final_update_id) {
    // Apply all bid updates
    for (const auto& [price, amount, update_id] : bids) {
        bids_.update(price, amount, update_id);
    }
    
    // Apply all ask updates
    for (const auto& [price, amount, update_id] : asks) {
        asks_.update(price, amount, update_id);
    }
    
    last_update_id_ = final_update_id;
    last_update_time_ = GlobalClock::now();
    notify_update();
}

utils::Decimal OrderBook::best_bid() const {
    auto best = bids_.best();
    if (best != bids_.end()) {
        return best->price;
    }
    return utils::Decimal::zero();
}

utils::Decimal OrderBook::best_ask() const {
    auto best = asks_.best();
    if (best != asks_.end()) {
        return best->price;
    }
    return utils::Decimal::zero();
}

utils::Decimal OrderBook::mid_price() const {
    auto bid = best_bid();
    auto ask = best_ask();
    
    if (bid.is_zero() || ask.is_zero()) {
        return utils::Decimal::zero();
    }
    
    return (bid + ask) / utils::Decimal("2");
}

utils::Decimal OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    
    if (bid.is_zero() || ask.is_zero()) {
        return utils::Decimal::zero();
    }
    
    return ask - bid;
}

std::vector<OrderBookEntry> OrderBook::get_bids(size_t limit) const {
    return bids_.get_levels(limit);
}

std::vector<OrderBookEntry> OrderBook::get_asks(size_t limit) const {
    return asks_.get_levels(limit);
}

utils::Decimal OrderBook::get_impact_price(bool is_buy, const utils::Decimal& quantity) const {
    if (quantity.is_zero()) {
        return utils::Decimal::zero();
    }
    
    utils::Decimal remaining_quantity = quantity;
    utils::Decimal total_cost;
    
    if (is_buy) {
        // Buying: consume asks starting from lowest price
        auto asks = get_asks(0); // Get all levels
        
        for (const auto& ask : asks) {
            if (remaining_quantity.is_zero()) {
                break;
            }
            
            utils::Decimal consume_quantity = std::min(ask.amount, remaining_quantity);
            total_cost += consume_quantity * ask.price;
            remaining_quantity -= consume_quantity;
        }
    } else {
        // Selling: consume bids starting from highest price
        auto bids = get_bids(0); // Get all levels
        
        for (const auto& bid : bids) {
            if (remaining_quantity.is_zero()) {
                break;
            }
            
            utils::Decimal consume_quantity = std::min(bid.amount, remaining_quantity);
            total_cost += consume_quantity * bid.price;
            remaining_quantity -= consume_quantity;
        }
    }
    
    if (remaining_quantity > utils::Decimal::zero()) {
        // Not enough liquidity
        return utils::Decimal::zero();
    }
    
    return total_cost / quantity; // Average price
}

utils::Decimal OrderBook::get_volume_at_price(bool is_buy, const utils::Decimal& price) const {
    if (is_buy) {
        return asks_.get_volume_at_or_better(price);
    } else {
        return bids_.get_volume_at_or_better(price);
    }
}

void OrderBook::register_update_callback(OrderBookUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callbacks_.push_back(std::move(callback));
}

void OrderBook::notify_update() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& callback : update_callbacks_) {
        try {
            callback(symbol_);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

void OrderBook::clear() {
    bids_.clear();
    asks_.clear();
    last_update_id_ = 0;
    last_update_time_ = GlobalClock::now();
}

std::string OrderBook::to_json(size_t depth) const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"symbol\":\"" << symbol_ << "\",";
    oss << "\"timestamp\":" << Clock::to_milliseconds(last_update_time_) << ",";
    oss << "\"lastUpdateId\":" << last_update_id_ << ",";
    
    // Bids
    oss << "\"bids\":[";
    auto bids = get_bids(depth);
    for (size_t i = 0; i < bids.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "[\"" << bids[i].price.to_string() << "\",\"" << bids[i].amount.to_string() << "\"]";
    }
    oss << "],";
    
    // Asks
    oss << "\"asks\":[";
    auto asks = get_asks(depth);
    for (size_t i = 0; i < asks.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "[\"" << asks[i].price.to_string() << "\",\"" << asks[i].amount.to_string() << "\"]";
    }
    oss << "]";
    
    oss << "}";
    return oss.str();
}

bool OrderBook::is_valid() const {
    auto best_bid_price = best_bid();
    auto best_ask_price = best_ask();
    
    // If both sides have orders, bid should be less than ask
    if (!best_bid_price.is_zero() && !best_ask_price.is_zero()) {
        return best_bid_price < best_ask_price;
    }
    
    return true; // Valid if one or both sides are empty
}

// OrderBookManager implementation
OrderBook& OrderBookManager::get_order_book(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        auto [inserted_it, success] = order_books_.emplace(
            symbol, std::make_unique<OrderBook>(symbol));
        return *inserted_it->second;
    }
    
    return *it->second;
}

bool OrderBookManager::has_order_book(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return order_books_.find(symbol) != order_books_.end();
}

void OrderBookManager::remove_order_book(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    order_books_.erase(symbol);
}

std::vector<std::string> OrderBookManager::get_symbols() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> symbols;
    symbols.reserve(order_books_.size());
    
    for (const auto& [symbol, book] : order_books_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

void OrderBookManager::clear_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    order_books_.clear();
}

} // namespace fasttrade::core
