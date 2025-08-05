#pragma once

#include "clock.hpp"
#include "../utils/decimal.hpp"
#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <functional>

namespace fasttrade::core {

/**
 * @brief Order book entry representing a price level
 */
struct OrderBookEntry {
    utils::Decimal price;
    utils::Decimal amount;
    int64_t update_id;
    Timestamp timestamp;

    OrderBookEntry() = default;
    OrderBookEntry(const utils::Decimal& p, const utils::Decimal& a, int64_t id)
        : price(p), amount(a), update_id(id), timestamp(GlobalClock::now()) {}

    // Comparison for price-time priority
    bool operator<(const OrderBookEntry& other) const {
        if (price != other.price) {
            return price < other.price;  // For asks (ascending price)
        }
        return timestamp < other.timestamp;  // Time priority
    }

    bool operator==(const OrderBookEntry& other) const {
        return price == other.price && update_id == other.update_id;
    }
};

/**
 * @brief Bid comparator for descending price order
 */
struct BidComparator {
    bool operator()(const OrderBookEntry& a, const OrderBookEntry& b) const {
        if (a.price != b.price) {
            return a.price > b.price;  // Descending price for bids
        }
        return a.timestamp < b.timestamp;  // Time priority
    }
};

/**
 * @brief Ask comparator for ascending price order
 */
struct AskComparator {
    bool operator()(const OrderBookEntry& a, const OrderBookEntry& b) const {
        if (a.price != b.price) {
            return a.price < b.price;  // Ascending price for asks
        }
        return a.timestamp < b.timestamp;  // Time priority
    }
};

/**
 * @brief Order book side (bids or asks)
 */
template<typename Comparator>
class OrderBookSide {
private:
    std::set<OrderBookEntry, Comparator> entries_;
    mutable std::mutex mutex_;

public:
    using iterator = typename std::set<OrderBookEntry, Comparator>::iterator;
    using const_iterator = typename std::set<OrderBookEntry, Comparator>::const_iterator;

    /**
     * @brief Add or update an entry
     * @param price Price level
     * @param amount New amount (0 means remove)
     * @param update_id Update sequence ID
     */
    void update(const utils::Decimal& price, const utils::Decimal& amount, int64_t update_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find existing entry at this price
        auto it = std::find_if(entries_.begin(), entries_.end(),
            [&price](const OrderBookEntry& entry) { return entry.price == price; });

        if (amount.is_zero()) {
            // Remove entry if amount is zero
            if (it != entries_.end()) {
                entries_.erase(it);
            }
        } else {
            // Update or add entry
            if (it != entries_.end()) {
                entries_.erase(it);
            }
            entries_.emplace(price, amount, update_id);
        }
    }

    /**
     * @brief Get the best price level
     * @return Iterator to best price, or end() if empty
     */
    const_iterator best() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.begin();
    }

    /**
     * @brief Get all price levels
     * @param limit Maximum number of levels (0 = all)
     * @return Vector of entries
     */
    std::vector<OrderBookEntry> get_levels(size_t limit = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<OrderBookEntry> result;
        
        size_t count = 0;
        for (const auto& entry : entries_) {
            result.push_back(entry);
            if (limit > 0 && ++count >= limit) break;
        }
        return result;
    }

    /**
     * @brief Get total volume at or better than price
     * @param price Price threshold
     * @return Total volume
     */
    utils::Decimal get_volume_at_or_better(const utils::Decimal& price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        utils::Decimal total;
        
        for (const auto& entry : entries_) {
            bool include = std::is_same_v<Comparator, BidComparator> ? 
                          (entry.price >= price) : (entry.price <= price);
            if (include) {
                total += entry.amount;
            } else {
                break;  // Entries are sorted, so we can break early
            }
        }
        return total;
    }

    /**
     * @brief Clear all entries
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    /**
     * @brief Get number of price levels
     * @return Number of levels
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    /**
     * @brief Check if empty
     * @return True if empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.empty();
    }

    // Iterator support
    const_iterator begin() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.begin();
    }

    const_iterator end() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.end();
    }
};

using BidSide = OrderBookSide<BidComparator>;
using AskSide = OrderBookSide<AskComparator>;

/**
 * @brief Order book update callback
 */
using OrderBookUpdateCallback = std::function<void(const std::string& symbol)>;

/**
 * @brief High-performance order book for real-time market data
 * 
 * This class provides fast order book operations with thread-safe access
 * and efficient price-level management optimized for trading systems.
 */
class OrderBook {
private:
    std::string symbol_;
    BidSide bids_;
    AskSide asks_;
    int64_t last_update_id_;
    Timestamp last_update_time_;
    std::vector<OrderBookUpdateCallback> update_callbacks_;
    mutable std::mutex callback_mutex_;

    void notify_update();

public:
    /**
     * @brief Construct an order book for a trading symbol
     * @param symbol Trading symbol (e.g., "BTC-USDT")
     */
    explicit OrderBook(const std::string& symbol);

    /**
     * @brief Update bid side
     * @param price Price level
     * @param amount New amount (0 means remove)
     * @param update_id Update sequence ID
     */
    void update_bid(const utils::Decimal& price, const utils::Decimal& amount, int64_t update_id);

    /**
     * @brief Update ask side
     * @param price Price level
     * @param amount New amount (0 means remove)
     * @param update_id Update sequence ID
     */
    void update_ask(const utils::Decimal& price, const utils::Decimal& amount, int64_t update_id);

    /**
     * @brief Apply multiple updates atomically
     * @param bids Vector of bid updates (price, amount, update_id)
     * @param asks Vector of ask updates (price, amount, update_id)
     * @param final_update_id Final sequence ID for this batch
     */
    void apply_updates(const std::vector<std::tuple<utils::Decimal, utils::Decimal, int64_t>>& bids,
                      const std::vector<std::tuple<utils::Decimal, utils::Decimal, int64_t>>& asks,
                      int64_t final_update_id);

    /**
     * @brief Get best bid price
     * @return Best bid price, or zero if no bids
     */
    utils::Decimal best_bid() const;

    /**
     * @brief Get best ask price
     * @return Best ask price, or zero if no asks
     */
    utils::Decimal best_ask() const;

    /**
     * @brief Get mid price
     * @return Mid price between best bid and ask
     */
    utils::Decimal mid_price() const;

    /**
     * @brief Get bid-ask spread
     * @return Spread (ask - bid)
     */
    utils::Decimal spread() const;

    /**
     * @brief Get bid levels
     * @param limit Maximum number of levels (0 = all)
     * @return Vector of bid entries
     */
    std::vector<OrderBookEntry> get_bids(size_t limit = 10) const;

    /**
     * @brief Get ask levels
     * @param limit Maximum number of levels (0 = all)
     * @return Vector of ask entries
     */
    std::vector<OrderBookEntry> get_asks(size_t limit = 10) const;

    /**
     * @brief Calculate impact price for a market order
     * @param side Order side (true = buy, false = sell)
     * @param quantity Order quantity
     * @return Average execution price
     */
    utils::Decimal get_impact_price(bool is_buy, const utils::Decimal& quantity) const;

    /**
     * @brief Get available volume at or better than price
     * @param is_buy True for buy side, false for sell side
     * @param price Price threshold
     * @return Available volume
     */
    utils::Decimal get_volume_at_price(bool is_buy, const utils::Decimal& price) const;

    /**
     * @brief Register callback for order book updates
     * @param callback Callback function
     */
    void register_update_callback(OrderBookUpdateCallback callback);

    /**
     * @brief Clear all price levels
     */
    void clear();

    /**
     * @brief Get trading symbol
     * @return Trading symbol
     */
    const std::string& symbol() const { return symbol_; }

    /**
     * @brief Get last update ID
     * @return Last update sequence ID
     */
    int64_t last_update_id() const { return last_update_id_; }

    /**
     * @brief Get last update time
     * @return Last update timestamp
     */
    Timestamp last_update_time() const { return last_update_time_; }

    /**
     * @brief Get order book snapshot as JSON
     * @param depth Number of price levels per side
     * @return JSON string representation
     */
    std::string to_json(size_t depth = 10) const;

    /**
     * @brief Validate order book integrity
     * @return True if order book is valid
     */
    bool is_valid() const;
};

/**
 * @brief Order book manager for multiple symbols
 * 
 * Manages multiple order books efficiently with shared resources.
 */
class OrderBookManager {
private:
    std::map<std::string, std::unique_ptr<OrderBook>> order_books_;
    mutable std::shared_mutex mutex_;

public:
    /**
     * @brief Get or create order book for symbol
     * @param symbol Trading symbol
     * @return Reference to order book
     */
    OrderBook& get_order_book(const std::string& symbol);

    /**
     * @brief Check if order book exists for symbol
     * @param symbol Trading symbol
     * @return True if exists
     */
    bool has_order_book(const std::string& symbol) const;

    /**
     * @brief Remove order book for symbol
     * @param symbol Trading symbol
     */
    void remove_order_book(const std::string& symbol);

    /**
     * @brief Get all symbols with order books
     * @return Vector of symbols
     */
    std::vector<std::string> get_symbols() const;

    /**
     * @brief Clear all order books
     */
    void clear_all();
};

} // namespace fasttrade::core
