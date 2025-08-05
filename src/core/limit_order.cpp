#include "fasttrade/core/limit_order.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <numeric>

namespace fasttrade::core {

LimitOrder::LimitOrder(std::string client_order_id,
                       std::string trading_pair,
                       OrderSide side,
                       const utils::Decimal& price,
                       const utils::Decimal& quantity)
    : client_order_id_(std::move(client_order_id)),
      trading_pair_(std::move(trading_pair)),
      side_(side),
      type_(OrderType::LIMIT),
      price_(price),
      quantity_(quantity),
      filled_quantity_(utils::Decimal::zero()),
      creation_time_(GlobalClock::now()),
      last_update_time_(creation_time_),
      status_(OrderStatus::PENDING) {
    
    // Parse base and quote currencies from trading pair
    size_t dash_pos = trading_pair_.find('-');
    if (dash_pos != std::string::npos) {
        base_currency_ = trading_pair_.substr(0, dash_pos);
        quote_currency_ = trading_pair_.substr(dash_pos + 1);
    } else {
        // Fallback parsing
        base_currency_ = trading_pair_;
        quote_currency_ = "USDT";
    }
}

LimitOrder::LimitOrder(std::string client_order_id,
                       std::string trading_pair,
                       OrderSide side,
                       OrderType type,
                       std::string base_currency,
                       std::string quote_currency,
                       const utils::Decimal& price,
                       const utils::Decimal& quantity,
                       const utils::Decimal& filled_quantity,
                       OrderStatus status,
                       std::string position)
    : client_order_id_(std::move(client_order_id)),
      trading_pair_(std::move(trading_pair)),
      side_(side),
      type_(type),
      base_currency_(std::move(base_currency)),
      quote_currency_(std::move(quote_currency)),
      price_(price),
      quantity_(quantity),
      filled_quantity_(filled_quantity),
      creation_time_(GlobalClock::now()),
      last_update_time_(creation_time_),
      status_(status),
      position_(std::move(position)) {
}

bool LimitOrder::operator<(const LimitOrder& other) const {
    // Primary sort by price
    if (price_ != other.price_) {
        if (side_ == OrderSide::BUY) {
            return price_ > other.price_; // Higher price first for bids
        } else {
            return price_ < other.price_; // Lower price first for asks
        }
    }
    
    // Secondary sort by time (earlier orders first)
    return creation_time_ < other.creation_time_;
}

bool LimitOrder::operator==(const LimitOrder& other) const {
    return client_order_id_ == other.client_order_id_;
}

void LimitOrder::set_status(OrderStatus status) {
    status_ = status;
    last_update_time_ = GlobalClock::now();
}

void LimitOrder::set_filled_quantity(const utils::Decimal& filled) {
    filled_quantity_ = filled;
    last_update_time_ = GlobalClock::now();
    
    // Update status based on fill level
    if (filled_quantity_ >= quantity_) {
        status_ = OrderStatus::FILLED;
    } else if (filled_quantity_ > utils::Decimal::zero()) {
        status_ = OrderStatus::PARTIAL;
    }
}

void LimitOrder::set_exchange_order_id(const std::string& id) {
    exchange_order_id_ = id;
    last_update_time_ = GlobalClock::now();
}

void LimitOrder::set_price(const utils::Decimal& price) {
    price_ = price;
    last_update_time_ = GlobalClock::now();
}

utils::Decimal LimitOrder::fill_percentage() const {
    if (quantity_.is_zero()) {
        return utils::Decimal::zero();
    }
    return (filled_quantity_ / quantity_) * utils::Decimal("100");
}

void LimitOrder::apply_fill(const utils::Decimal& fill_quantity, const utils::Decimal& fill_price) {
    filled_quantity_ += fill_quantity;
    last_update_time_ = GlobalClock::now();
    
    // Update average fill price if needed
    // For simplicity, we'll use the order price as the fill price
    
    // Update status
    if (filled_quantity_ >= quantity_) {
        status_ = OrderStatus::FILLED;
    } else {
        status_ = OrderStatus::PARTIAL;
    }
}

void LimitOrder::cancel() {
    status_ = OrderStatus::CANCELLED;
    last_update_time_ = GlobalClock::now();
}

std::string LimitOrder::to_string() const {
    std::ostringstream oss;
    oss << "LimitOrder("
        << "id=" << client_order_id_
        << ", pair=" << trading_pair_
        << ", side=" << fasttrade::core::to_string(side_)
        << ", type=" << fasttrade::core::to_string(type_)
        << ", price=" << price_
        << ", quantity=" << quantity_
        << ", filled=" << filled_quantity_
        << ", status=" << fasttrade::core::to_string(status_)
        << ")";
    return oss.str();
}

nlohmann::json LimitOrder::to_json() const {
    nlohmann::json j;
    
    j["client_order_id"] = client_order_id_;
    j["trading_pair"] = trading_pair_;
    j["side"] = fasttrade::core::to_string(side_);
    j["type"] = fasttrade::core::to_string(type_);
    j["base_currency"] = base_currency_;
    j["quote_currency"] = quote_currency_;
    j["price"] = price_.to_string();
    j["quantity"] = quantity_.to_string();
    j["filled_quantity"] = filled_quantity_.to_string();
    j["creation_time"] = Clock::to_milliseconds(creation_time_);
    j["last_update_time"] = Clock::to_milliseconds(last_update_time_);
    j["status"] = fasttrade::core::to_string(status_);
    j["position"] = position_;
    j["exchange_order_id"] = exchange_order_id_;
    
    // Add executions
    j["executions"] = nlohmann::json::array();
    for (const auto& exec : executions_) {
        j["executions"].push_back(exec.to_json());
    }
    
    // Optional fields
    if (rejection_reason_.has_value()) {
        j["rejection_reason"] = rejection_reason_.value();
    }
    
    if (expiry_time_.has_value()) {
        j["expiry_time"] = Clock::to_milliseconds(expiry_time_.value());
    }
    
    // Computed fields
    j["remaining_quantity"] = remaining_quantity().to_string();
    j["fill_percentage"] = fill_percentage().to_string();
    j["age_ms"] = age_ms();
    j["is_active"] = is_active();
    j["average_execution_price"] = get_average_execution_price().to_string();
    j["total_fees"] = get_total_fees().to_string();
    
    return j;
}

std::string LimitOrder::to_json_string() const {
    return to_json().dump(4); // Pretty print with 4-space indentation
}

LimitOrder LimitOrder::from_json(const nlohmann::json& json_obj) {
    try {
        LimitOrder order;
        
        // Required fields
        order.client_order_id_ = json_obj.at("client_order_id").get<std::string>();
        order.trading_pair_ = json_obj.at("trading_pair").get<std::string>();
        order.side_ = order_side_from_string(json_obj.at("side").get<std::string>());
        order.type_ = order_type_from_string(json_obj.at("type").get<std::string>());
        order.base_currency_ = json_obj.at("base_currency").get<std::string>();
        order.quote_currency_ = json_obj.at("quote_currency").get<std::string>();
        order.price_ = utils::Decimal::from_string(json_obj.at("price").get<std::string>());
        order.quantity_ = utils::Decimal::from_string(json_obj.at("quantity").get<std::string>());
        order.filled_quantity_ = utils::Decimal::from_string(json_obj.at("filled_quantity").get<std::string>());
        order.status_ = order_status_from_string(json_obj.at("status").get<std::string>());
        
        // Timestamps
        order.creation_time_ = Clock::from_milliseconds(json_obj.at("creation_time").get<int64_t>());
        order.last_update_time_ = Clock::from_milliseconds(json_obj.at("last_update_time").get<int64_t>());
        
        // Optional fields
        if (json_obj.contains("position")) {
            order.position_ = json_obj["position"].get<std::string>();
        }
        
        if (json_obj.contains("exchange_order_id")) {
            order.exchange_order_id_ = json_obj["exchange_order_id"].get<std::string>();
        }
        
        if (json_obj.contains("rejection_reason")) {
            order.rejection_reason_ = json_obj["rejection_reason"].get<std::string>();
        }
        
        if (json_obj.contains("expiry_time")) {
            order.expiry_time_ = Clock::from_milliseconds(json_obj["expiry_time"].get<int64_t>());
        }
        
        // Load executions
        if (json_obj.contains("executions")) {
            for (const auto& exec_json : json_obj["executions"]) {
                order.executions_.push_back(ExecutionDetail::from_json(exec_json));
            }
        }
        
        return order;
        
    } catch (const nlohmann::json::exception& e) {
        throw std::invalid_argument("Invalid JSON for LimitOrder: " + std::string(e.what()));
    }
}

LimitOrder LimitOrder::from_json_string(const std::string& json_str) {
    try {
        auto json_obj = nlohmann::json::parse(json_str);
        return from_json(json_obj);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::invalid_argument("Failed to parse JSON string: " + std::string(e.what()));
    }
}

bool LimitOrder::is_valid() const {
    // Basic validation checks
    if (client_order_id_.empty()) return false;
    if (trading_pair_.empty()) return false;
    if (quantity_.is_zero() || quantity_.is_negative()) return false;
    if (type_ == OrderType::LIMIT && (price_.is_zero() || price_.is_negative())) return false;
    if (filled_quantity_.is_negative()) return false;
    if (filled_quantity_ > quantity_) return false;
    
    // Check execution consistency
    utils::Decimal total_executed;
    for (const auto& exec : executions_) {
        total_executed += exec.quantity;
    }
    
    // Allow small rounding differences
    utils::Decimal diff = (total_executed - filled_quantity_).abs();
    if (diff > utils::Decimal("0.00000001")) return false;
    
    return true;
}

int64_t LimitOrder::age_ms() const {
    auto now = GlobalClock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - creation_time_).count();
}

int64_t LimitOrder::time_since_last_update_ms() const {
    auto now = GlobalClock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_update_time_).count();
}

void LimitOrder::add_execution(const std::string& execution_id,
                              const utils::Decimal& quantity,
                              const utils::Decimal& price,
                              const utils::Decimal& fee_amount,
                              const std::string& fee_currency) {
    executions_.emplace_back(execution_id, quantity, price, fee_amount, fee_currency);
    
    // Update filled quantity
    filled_quantity_ += quantity;
    last_update_time_ = GlobalClock::now();
    
    // Update status based on fill level
    if (filled_quantity_ >= quantity_) {
        status_ = OrderStatus::FILLED;
    } else if (filled_quantity_ > utils::Decimal::zero()) {
        status_ = OrderStatus::PARTIAL;
    }
}

utils::Decimal LimitOrder::get_executed_value() const {
    utils::Decimal total_value;
    for (const auto& exec : executions_) {
        total_value += exec.quantity * exec.price;
    }
    return total_value;
}

utils::Decimal LimitOrder::get_average_execution_price() const {
    if (filled_quantity_.is_zero()) {
        return utils::Decimal::zero();
    }
    
    return get_executed_value() / filled_quantity_;
}

utils::Decimal LimitOrder::get_total_fees() const {
    utils::Decimal total_fees;
    for (const auto& exec : executions_) {
        // For simplicity, assume all fees are in quote currency
        // In a real implementation, you'd convert fees to a common currency
        total_fees += exec.fee_amount;
    }
    return total_fees;
}

// ExecutionDetail implementation
nlohmann::json ExecutionDetail::to_json() const {
    nlohmann::json j;
    j["execution_id"] = execution_id;
    j["quantity"] = quantity.to_string();
    j["price"] = price.to_string();
    j["fee_amount"] = fee_amount.to_string();
    j["fee_currency"] = fee_currency;
    j["timestamp"] = Clock::to_milliseconds(timestamp);
    j["value"] = (quantity * price).to_string();
    return j;
}

ExecutionDetail ExecutionDetail::from_json(const nlohmann::json& j) {
    ExecutionDetail exec;
    exec.execution_id = j.at("execution_id").get<std::string>();
    exec.quantity = utils::Decimal::from_string(j.at("quantity").get<std::string>());
    exec.price = utils::Decimal::from_string(j.at("price").get<std::string>());
    exec.fee_amount = utils::Decimal::from_string(j.at("fee_amount").get<std::string>());
    exec.fee_currency = j.at("fee_currency").get<std::string>();
    exec.timestamp = Clock::from_milliseconds(j.at("timestamp").get<int64_t>());
    return exec;
}

// OrderBuilder implementation
OrderBuilder& OrderBuilder::id(const std::string& client_order_id) {
    client_order_id_ = client_order_id;
    return *this;
}

OrderBuilder& OrderBuilder::pair(const std::string& trading_pair) {
    trading_pair_ = trading_pair;
    return *this;
}

OrderBuilder& OrderBuilder::buy(const utils::Decimal& quantity) {
    side_ = OrderSide::BUY;
    quantity_ = quantity;
    return *this;
}

OrderBuilder& OrderBuilder::sell(const utils::Decimal& quantity) {
    side_ = OrderSide::SELL;
    quantity_ = quantity;
    return *this;
}

OrderBuilder& OrderBuilder::at_price(const utils::Decimal& price) {
    price_ = price;
    type_ = OrderType::LIMIT;
    return *this;
}

OrderBuilder& OrderBuilder::market_order() {
    type_ = OrderType::MARKET;
    return *this;
}

OrderBuilder& OrderBuilder::limit_order() {
    type_ = OrderType::LIMIT;
    return *this;
}

OrderBuilder& OrderBuilder::position(const std::string& position) {
    position_ = position;
    return *this;
}

LimitOrder OrderBuilder::build() {
    if (client_order_id_.empty()) {
        throw std::invalid_argument("Order ID is required");
    }
    if (trading_pair_.empty()) {
        throw std::invalid_argument("Trading pair is required");
    }
    if (quantity_.is_zero()) {
        throw std::invalid_argument("Quantity must be greater than zero");
    }
    if (type_ == OrderType::LIMIT && price_.is_zero()) {
        throw std::invalid_argument("Price is required for limit orders");
    }
    
    return LimitOrder(client_order_id_, trading_pair_, side_, price_, quantity_);
}

// Helper functions for string conversions
std::string to_string(OrderSide side) {
    switch (side) {
        case OrderSide::BUY: return "BUY";
        case OrderSide::SELL: return "SELL";
        default: return "UNKNOWN";
    }
}

std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::OPEN: return "OPEN";
        case OrderStatus::PARTIAL: return "PARTIAL";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

std::string to_string(OrderType type) {
    switch (type) {
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        case OrderType::STOP_MARKET: return "STOP_MARKET";
        default: return "UNKNOWN";
    }
}

OrderSide order_side_from_string(const std::string& str) {
    if (str == "BUY") return OrderSide::BUY;
    if (str == "SELL") return OrderSide::SELL;
    throw std::invalid_argument("Invalid order side: " + str);
}

OrderStatus order_status_from_string(const std::string& str) {
    if (str == "PENDING") return OrderStatus::PENDING;
    if (str == "OPEN") return OrderStatus::OPEN;
    if (str == "PARTIAL") return OrderStatus::PARTIAL;
    if (str == "FILLED") return OrderStatus::FILLED;
    if (str == "CANCELLED") return OrderStatus::CANCELLED;
    if (str == "REJECTED") return OrderStatus::REJECTED;
    if (str == "EXPIRED") return OrderStatus::EXPIRED;
    throw std::invalid_argument("Invalid order status: " + str);
}

OrderType order_type_from_string(const std::string& str) {
    if (str == "LIMIT") return OrderType::LIMIT;
    if (str == "MARKET") return OrderType::MARKET;
    if (str == "STOP_LIMIT") return OrderType::STOP_LIMIT;
    if (str == "STOP_MARKET") return OrderType::STOP_MARKET;
    throw std::invalid_argument("Invalid order type: " + str);
}

} // namespace fasttrade::core
