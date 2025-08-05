#pragma once

#include "clock.hpp"
#include "../utils/decimal.hpp"
#include <string>
#include <memory>

namespace fasttrade::core {

/**
 * @brief Order side enumeration
 */
enum class OrderSide {
    BUY,
    SELL
};

/**
 * @brief Order status enumeration
 */
enum class OrderStatus {
    PENDING,     ///< Order is created but not yet sent
    OPEN,        ///< Order is active in the market
    PARTIAL,     ///< Order is partially filled
    FILLED,      ///< Order is completely filled
    CANCELLED,   ///< Order was cancelled
    REJECTED,    ///< Order was rejected by the exchange
    EXPIRED      ///< Order expired
};

/**
 * @brief Order type enumeration
 */
enum class OrderType {
    LIMIT,       ///< Limit order
    MARKET,      ///< Market order
    STOP_LIMIT,  ///< Stop limit order
    STOP_MARKET  ///< Stop market order
};

/**
 * @brief High-performance limit order implementation
 * 
 * This class represents a trading order with all necessary fields
 * and operations optimized for high-frequency trading scenarios.
 */
class LimitOrder {
private:
    std::string client_order_id_;
    std::string trading_pair_;
    OrderSide side_;
    OrderType type_;
    std::string base_currency_;
    std::string quote_currency_;
    utils::Decimal price_;
    utils::Decimal quantity_;
    utils::Decimal filled_quantity_;
    Timestamp creation_time_;
    Timestamp last_update_time_;
    OrderStatus status_;
    std::string position_;
    std::string exchange_order_id_;

public:
    /**
     * @brief Construct a new Limit Order
     */
    LimitOrder() = default;

    /**
     * @brief Construct a limit order with essential parameters
     * @param client_order_id Unique client-side order ID
     * @param trading_pair Trading pair (e.g., "BTC-USDT")
     * @param side Order side (BUY/SELL)
     * @param price Order price
     * @param quantity Order quantity
     */
    LimitOrder(std::string client_order_id,
               std::string trading_pair,
               OrderSide side,
               const utils::Decimal& price,
               const utils::Decimal& quantity);

    /**
     * @brief Construct a limit order with full parameters
     */
    LimitOrder(std::string client_order_id,
               std::string trading_pair,
               OrderSide side,
               OrderType type,
               std::string base_currency,
               std::string quote_currency,
               const utils::Decimal& price,
               const utils::Decimal& quantity,
               const utils::Decimal& filled_quantity = utils::Decimal::zero(),
               OrderStatus status = OrderStatus::PENDING,
               std::string position = "");

    // Copy/Move constructors
    LimitOrder(const LimitOrder& other) = default;
    LimitOrder(LimitOrder&& other) noexcept = default;
    LimitOrder& operator=(const LimitOrder& other) = default;
    LimitOrder& operator=(LimitOrder&& other) noexcept = default;

    // Comparison operators for ordering (needed for containers)
    bool operator<(const LimitOrder& other) const;
    bool operator==(const LimitOrder& other) const;

    // Getters
    const std::string& client_order_id() const { return client_order_id_; }
    const std::string& trading_pair() const { return trading_pair_; }
    OrderSide side() const { return side_; }
    OrderType type() const { return type_; }
    const std::string& base_currency() const { return base_currency_; }
    const std::string& quote_currency() const { return quote_currency_; }
    const utils::Decimal& price() const { return price_; }
    const utils::Decimal& quantity() const { return quantity_; }
    const utils::Decimal& filled_quantity() const { return filled_quantity_; }
    Timestamp creation_time() const { return creation_time_; }
    Timestamp last_update_time() const { return last_update_time_; }
    OrderStatus status() const { return status_; }
    const std::string& position() const { return position_; }
    const std::string& exchange_order_id() const { return exchange_order_id_; }

    // Setters
    void set_status(OrderStatus status);
    void set_filled_quantity(const utils::Decimal& filled);
    void set_exchange_order_id(const std::string& id);
    void set_price(const utils::Decimal& price);

    // Utility methods
    utils::Decimal remaining_quantity() const { return quantity_ - filled_quantity_; }
    utils::Decimal fill_percentage() const;
    bool is_buy() const { return side_ == OrderSide::BUY; }
    bool is_sell() const { return side_ == OrderSide::SELL; }
    bool is_filled() const { return status_ == OrderStatus::FILLED; }
    bool is_active() const { return status_ == OrderStatus::OPEN || status_ == OrderStatus::PARTIAL; }
    bool is_cancelled() const { return status_ == OrderStatus::CANCELLED; }

    /**
     * @brief Apply a partial fill to the order
     * @param fill_quantity Quantity that was filled
     * @param fill_price Price at which the fill occurred
     */
    void apply_fill(const utils::Decimal& fill_quantity, const utils::Decimal& fill_price);

    /**
     * @brief Cancel the order
     */
    void cancel();

    /**
     * @brief Get a string representation of the order
     * @return String representation
     */
    std::string to_string() const;

    /**
     * @brief Convert order to JSON string for serialization
     * @return JSON string
     */
    std::string to_json() const;

    /**
     * @brief Create order from JSON string
     * @param json JSON string representation
     * @return LimitOrder object
     */
    static LimitOrder from_json(const std::string& json);
};

/**
 * @brief Order builder for easy order construction
 * 
 * Provides a fluent interface for building orders with optional parameters.
 */
class OrderBuilder {
private:
    std::string client_order_id_;
    std::string trading_pair_;
    OrderSide side_;
    OrderType type_ = OrderType::LIMIT;
    utils::Decimal price_;
    utils::Decimal quantity_;
    std::string position_;

public:
    OrderBuilder& id(const std::string& client_order_id);
    OrderBuilder& pair(const std::string& trading_pair);
    OrderBuilder& buy(const utils::Decimal& quantity);
    OrderBuilder& sell(const utils::Decimal& quantity);
    OrderBuilder& at_price(const utils::Decimal& price);
    OrderBuilder& market_order();
    OrderBuilder& limit_order();
    OrderBuilder& position(const std::string& position);

    /**
     * @brief Build the order
     * @return Constructed LimitOrder
     */
    LimitOrder build();
};

// Helper functions for string conversions
std::string to_string(OrderSide side);
std::string to_string(OrderStatus status);
std::string to_string(OrderType type);
OrderSide order_side_from_string(const std::string& str);
OrderStatus order_status_from_string(const std::string& str);
OrderType order_type_from_string(const std::string& str);

} // namespace fasttrade::core
