#include "fasttrade/core/trading_core.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <shared_mutex>

namespace fasttrade::core {

TradingCore::TradingCore()
    : order_book_manager_(std::make_unique<OrderBookManager>()),
      clock_(std::make_unique<Clock>(ClockMode::REALTIME)),
      market_data_manager_(std::make_unique<MarketDataManager>()),
      running_(false),
      daily_pnl_(utils::Decimal::zero()),
      total_pnl_(utils::Decimal::zero()) {
}

TradingCore::~TradingCore() {
    stop();
}

TradingCore::TradingCore(TradingCore&& other) noexcept
    : order_book_manager_(std::move(other.order_book_manager_)),
      clock_(std::move(other.clock_)),
      market_data_manager_(std::move(other.market_data_manager_)),
      active_orders_(std::move(other.active_orders_)),
      positions_(std::move(other.positions_)),
      balances_(std::move(other.balances_)),
      trade_history_(std::move(other.trade_history_)),
      risk_limits_(other.risk_limits_),
      callbacks_(std::move(other.callbacks_)),
      event_queue_(std::move(other.event_queue_)),
      event_processor_(std::move(other.event_processor_)),
      running_(other.running_.load()),
      daily_pnl_(other.daily_pnl_),
      total_pnl_(other.total_pnl_) {
    other.running_ = false;
}

TradingCore& TradingCore::operator=(TradingCore&& other) noexcept {
    if (this != &other) {
        stop();
        
        order_book_manager_ = std::move(other.order_book_manager_);
        clock_ = std::move(other.clock_);
        market_data_manager_ = std::move(other.market_data_manager_);
        active_orders_ = std::move(other.active_orders_);
        positions_ = std::move(other.positions_);
        balances_ = std::move(other.balances_);
        trade_history_ = std::move(other.trade_history_);
        risk_limits_ = other.risk_limits_;
        callbacks_ = std::move(other.callbacks_);
        event_queue_ = std::move(other.event_queue_);
        event_processor_ = std::move(other.event_processor_);
        running_ = other.running_.load();
        daily_pnl_ = other.daily_pnl_;
        total_pnl_ = other.total_pnl_;
        
        other.running_ = false;
    }
    return *this;
}

void TradingCore::initialize(ClockMode clock_mode) {
    clock_ = std::make_unique<Clock>(clock_mode);
    order_book_manager_ = std::make_unique<OrderBookManager>();
    market_data_manager_ = std::make_unique<MarketDataManager>();
    
    // Initialize default risk limits if not set
    if (risk_limits_.max_position_size.is_zero()) {
        risk_limits_.max_position_size = utils::Decimal("1000.0");
        risk_limits_.max_order_size = utils::Decimal("100.0");
        risk_limits_.max_daily_loss = utils::Decimal("10000.0");
        risk_limits_.max_orders_per_second = 100;
        risk_limits_.enable_position_limits = true;
        risk_limits_.enable_order_limits = true;
        risk_limits_.enable_loss_limits = true;
    }
}

void TradingCore::start() {
    if (running_.load()) {
        return; // Already running
    }
    
    running_ = true;
    clock_->start();
    
    // Start event processing thread
    event_processor_ = std::thread(&TradingCore::process_events, this);
}

void TradingCore::stop() {
    if (!running_.load()) {
        return; // Already stopped
    }
    
    running_ = false;
    
    if (clock_) {
        clock_->stop();
    }
    
    if (event_processor_.joinable()) {
        event_processor_.join();
    }
}

void TradingCore::process_events() {
    while (running_.load()) {
        std::function<void()> event;
        
        {
            std::lock_guard<std::mutex> lock(event_mutex_);
            if (!event_queue_.empty()) {
                event = event_queue_.front();
                event_queue_.pop();
            }
        }
        
        if (event) {
            try {
                event();
            } catch (...) {
                // Log error in production code
            }
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

bool TradingCore::submit_order(const LimitOrder& order) {
    if (!validate_order(order)) {
        return false;
    }
    
    // Check risk limits
    if (!check_risk_limits(order)) {
        if (callbacks_.on_order_rejected) {
            std::lock_guard<std::mutex> lock(event_mutex_);
            event_queue_.push([this, order]() {
                callbacks_.on_order_rejected(order);
            });
        }
        return false;
    }
    
    // Add to active orders
    {
        std::unique_lock<std::shared_mutex> lock(state_mutex_);
        auto order_copy = std::make_unique<LimitOrder>(order);
        order_copy->set_status(OrderStatus::OPEN);
        active_orders_[order.client_order_id()] = std::move(order_copy);
    }
    
    // In a real implementation, this would send the order to an exchange
    // For now, we'll just mark it as accepted
    
    return true;
}

bool TradingCore::cancel_order(const std::string& client_order_id) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    auto it = active_orders_.find(client_order_id);
    if (it != active_orders_.end()) {
        it->second->cancel();
        
        if (callbacks_.on_order_cancelled) {
            auto order_copy = *it->second;
            std::lock_guard<std::mutex> event_lock(event_mutex_);
            event_queue_.push([this, order_copy]() {
                callbacks_.on_order_cancelled(order_copy);
            });
        }
        
        active_orders_.erase(it);
        return true;
    }
    
    return false;
}

bool TradingCore::modify_order(const std::string& client_order_id, 
                              const utils::Decimal& new_price,
                              const utils::Decimal& new_quantity) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    auto it = active_orders_.find(client_order_id);
    if (it != active_orders_.end()) {
        if (!new_price.is_zero()) {
            it->second->set_price(new_price);
        }
        
        // Note: Modifying quantity is more complex in real trading systems
        // as it may require canceling and re-submitting the order
        
        return true;
    }
    
    return false;
}

std::vector<LimitOrder> TradingCore::get_active_orders() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    std::vector<LimitOrder> orders;
    orders.reserve(active_orders_.size());
    
    for (const auto& [id, order] : active_orders_) {
        orders.push_back(*order);
    }
    
    return orders;
}

std::vector<LimitOrder> TradingCore::get_active_orders(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    std::vector<LimitOrder> orders;
    
    for (const auto& [id, order] : active_orders_) {
        if (order->trading_pair() == symbol) {
            orders.push_back(*order);
        }
    }
    
    return orders;
}

OrderBook& TradingCore::get_order_book(const std::string& symbol) {
    return order_book_manager_->get_order_book(symbol);
}

void TradingCore::subscribe_market_data(const std::string& symbol) {
    // Ensure order book exists
    order_book_manager_->get_order_book(symbol);
    
    // Subscribe to market data if manager is initialized
    if (market_data_manager_) {
        market_data_manager_->subscribe_market_data(symbol);
    }
}

void TradingCore::unsubscribe_market_data(const std::string& symbol) {
    if (market_data_manager_) {
        market_data_manager_->unsubscribe_market_data(symbol);
    }
    // this would close market data connections
    order_book_manager_->remove_order_book(symbol);
}

void TradingCore::subscribe_market_data(const std::string& symbol, const std::vector<MarketDataManager::Exchange>& exchanges) {
    // Ensure order book exists
    order_book_manager_->get_order_book(symbol);
    
    // Subscribe to market data on specific exchanges
    if (market_data_manager_) {
        for (auto exchange : exchanges) {
            market_data_manager_->subscribe_market_data(symbol, exchange);
        }
    }
}

bool TradingCore::initialize_market_data(const std::vector<MarketDataManager::Exchange>& exchanges) {
    if (!market_data_manager_) {
        market_data_manager_ = std::make_unique<MarketDataManager>();
    }
    
    // Set up callbacks for market data events
    market_data_manager_->set_market_tick_callback(
        [this](const MarketTick& tick, MarketDataManager::Exchange exchange) {
            // Update order book with market tick data
            auto& order_book = order_book_manager_->get_order_book(tick.symbol);
            if (tick.is_bid) {
                order_book.update_bid(tick.price, tick.quantity, tick.timestamp);
            } else {
                order_book.update_ask(tick.price, tick.quantity, tick.timestamp);
            }
            
            // Notify callbacks if set
            if (callbacks_.on_market_data) {
                std::lock_guard<std::mutex> lock(event_mutex_);
                event_queue_.push([this, tick, exchange]() {
                    callbacks_.on_market_data(tick.symbol, tick.price, tick.quantity, tick.is_bid);
                });
            }
        }
    );
    
    market_data_manager_->set_trade_tick_callback(
        [this](const TradeTick& tick, MarketDataManager::Exchange exchange) {
            // Handle trade tick data
            if (callbacks_.on_trade) {
                std::lock_guard<std::mutex> lock(event_mutex_);
                event_queue_.push([this, tick, exchange]() {
                    callbacks_.on_trade(tick.symbol, tick.price, tick.quantity, tick.side == "buy");
                });
            }
        }
    );
    
    market_data_manager_->set_error_callback(
        [this](const std::string& error, MarketDataManager::Exchange exchange) {
            std::cerr << "Market data error on exchange " << static_cast<int>(exchange) << ": " << error << std::endl;
        }
    );
    
    return market_data_manager_->initialize(exchanges);
}

bool TradingCore::is_market_data_connected() const {
    return market_data_manager_ && market_data_manager_->is_connected();
}

std::vector<std::string> TradingCore::get_subscribed_symbols() const {
    if (market_data_manager_) {
        return market_data_manager_->get_subscribed_symbols();
    }
    return {};
}

Position TradingCore::get_position(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        return it->second;
    }
    
    return Position(); // Empty position
}

std::map<std::string, Position> TradingCore::get_all_positions() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return positions_;
}

Balance TradingCore::get_balance(const std::string& currency) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    auto it = balances_.find(currency);
    if (it != balances_.end()) {
        return it->second;
    }
    
    return Balance(); // Empty balance
}

std::map<std::string, Balance> TradingCore::get_all_balances() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return balances_;
}

utils::Decimal TradingCore::get_portfolio_value(const std::string& base_currency) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    utils::Decimal total_value;
    
    // Add cash balances
    for (const auto& [currency, balance] : balances_) {
        if (currency == base_currency) {
            total_value += balance.total;
        } else {
            // In a real implementation, this would convert using current exchange rates
            // For now, we'll assume 1:1 conversion (simplified)
            total_value += balance.total;
        }
    }
    
    // Add position values
    for (const auto& [symbol, position] : positions_) {
        // In a real implementation, this would use current market prices
        utils::Decimal position_value = position.quantity * position.average_price;
        total_value += position_value;
    }
    
    return total_value;
}

utils::Decimal TradingCore::get_unrealized_pnl() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    utils::Decimal total_unrealized;
    for (const auto& [symbol, position] : positions_) {
        total_unrealized += position.unrealized_pnl;
    }
    
    return total_unrealized;
}

void TradingCore::set_risk_limits(const RiskLimits& limits) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    risk_limits_ = limits;
}

bool TradingCore::check_risk_limits(const LimitOrder& order) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    // Check order size limit
    if (risk_limits_.enable_order_limits) {
        if (order.quantity() > risk_limits_.max_order_size) {
            return false;
        }
    }
    
    // Check position size limit
    if (risk_limits_.enable_position_limits) {
        auto it = positions_.find(order.trading_pair());
        utils::Decimal current_position;
        if (it != positions_.end()) {
            current_position = it->second.quantity;
        }
        
        utils::Decimal new_position = current_position;
        if (order.is_buy()) {
            new_position += order.quantity();
        } else {
            new_position -= order.quantity();
        }
        
        if (new_position.abs() > risk_limits_.max_position_size) {
            return false;
        }
    }
    
    // Check daily loss limit
    if (risk_limits_.enable_loss_limits) {
        if (daily_pnl_ < -risk_limits_.max_daily_loss) {
            return false;
        }
    }
    
    return true;
}

void TradingCore::set_callbacks(const TradingCallbacks& callbacks) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    callbacks_ = callbacks;
}

std::vector<Trade> TradingCore::get_trade_history(size_t limit) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    if (limit == 0 || limit >= trade_history_.size()) {
        return trade_history_;
    }
    
    return std::vector<Trade>(trade_history_.begin(), trade_history_.begin() + limit);
}

std::vector<Trade> TradingCore::get_trade_history(const std::string& symbol, size_t limit) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    std::vector<Trade> filtered_trades;
    for (const auto& trade : trade_history_) {
        if (trade.symbol == symbol) {
            filtered_trades.push_back(trade);
            if (limit > 0 && filtered_trades.size() >= limit) {
                break;
            }
        }
    }
    
    return filtered_trades;
}

Timestamp TradingCore::now() const {
    if (clock_) {
        return clock_->now();
    }
    return std::chrono::high_resolution_clock::now();
}

std::string TradingCore::get_statistics() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"active_orders\": " << active_orders_.size() << ",\n";
    oss << "  \"positions\": " << positions_.size() << ",\n";
    oss << "  \"total_trades\": " << trade_history_.size() << ",\n";
    oss << "  \"realized_pnl\": " << total_pnl_.to_string() << ",\n";
    oss << "  \"unrealized_pnl\": " << get_unrealized_pnl().to_string() << ",\n";
    oss << "  \"daily_pnl\": " << daily_pnl_.to_string() << ",\n";
    oss << "  \"running\": " << (running_.load() ? "true" : "false") << "\n";
    oss << "}";
    
    return oss.str();
}

void TradingCore::reset() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    active_orders_.clear();
    positions_.clear();
    balances_.clear();
    trade_history_.clear();
    daily_pnl_ = utils::Decimal::zero();
    total_pnl_ = utils::Decimal::zero();
    
    if (order_book_manager_) {
        order_book_manager_->clear_all();
    }
}

std::string TradingCore::export_state() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"positions\": [\n";
    
    bool first = true;
    for (const auto& [symbol, position] : positions_) {
        if (!first) oss << ",\n";
        first = false;
        
        oss << "    {\n";
        oss << "      \"symbol\": \"" << position.symbol << "\",\n";
        oss << "      \"quantity\": " << position.quantity.to_string() << ",\n";
        oss << "      \"average_price\": " << position.average_price.to_string() << ",\n";
        oss << "      \"realized_pnl\": " << position.realized_pnl.to_string() << "\n";
        oss << "    }";
    }
    
    oss << "\n  ],\n";
    oss << "  \"balances\": [\n";
    
    first = true;
    for (const auto& [currency, balance] : balances_) {
        if (!first) oss << ",\n";
        first = false;
        
        oss << "    {\n";
        oss << "      \"currency\": \"" << balance.currency << "\",\n";
        oss << "      \"total\": " << balance.total.to_string() << ",\n";
        oss << "      \"available\": " << balance.available.to_string() << "\n";
        oss << "    }";
    }
    
    oss << "\n  ],\n";
    oss << "  \"total_pnl\": " << total_pnl_.to_string() << ",\n";
    oss << "  \"daily_pnl\": " << daily_pnl_.to_string() << "\n";
    oss << "}";
    
    return oss.str();
}

bool TradingCore::import_state(const std::string& json) {
    // JSON parsing would be implemented here with a proper JSON library
    // For now, return false to indicate not implemented
    return false;
}

bool TradingCore::validate_order(const LimitOrder& order) {
    if (order.client_order_id().empty()) {
        return false;
    }
    if (order.trading_pair().empty()) {
        return false;
    }
    if (order.quantity().is_zero() || order.quantity().is_negative()) {
        return false;
    }
    if (order.type() == OrderType::LIMIT && (order.price().is_zero() || order.price().is_negative())) {
        return false;
    }
    
    return true;
}

void TradingCore::update_position(const Trade& trade) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    auto& position = positions_[trade.symbol];
    position.symbol = trade.symbol;
    
    if (trade.side == OrderSide::BUY) {
        // Update average price for buy
        utils::Decimal total_cost = position.quantity * position.average_price + trade.quantity * trade.price;
        position.quantity += trade.quantity;
        if (!position.quantity.is_zero()) {
            position.average_price = total_cost / position.quantity;
        }
    } else {
        // Selling - realize P&L
        utils::Decimal realized = trade.quantity * (trade.price - position.average_price);
        position.realized_pnl += realized;
        total_pnl_ += realized;
        daily_pnl_ += realized;
        
        position.quantity -= trade.quantity;
    }
    
    position.last_update = GlobalClock::now();
    
    // Notify callback
    if (callbacks_.on_position_update) {
        std::lock_guard<std::mutex> event_lock(event_mutex_);
        event_queue_.push([this, position]() {
            callbacks_.on_position_update(position);
        });
    }
}

void TradingCore::update_balance(const std::string& currency, const utils::Decimal& delta) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    auto& balance = balances_[currency];
    balance.currency = currency;
    balance.total += delta;
    balance.available += delta; // Simplified - in reality, available might be different
    balance.last_update = GlobalClock::now();
    
    // Notify callback
    if (callbacks_.on_balance_update) {
        std::lock_guard<std::mutex> event_lock(event_mutex_);
        event_queue_.push([this, balance]() {
            callbacks_.on_balance_update(balance);
        });
    }
}

void TradingCore::calculate_pnl() {
    // This would be implemented to calculate unrealized P&L based on current market prices
    // For now, it's a placeholder
}

// TradingCoreBuilder implementation
TradingCoreBuilder& TradingCoreBuilder::with_clock_mode(ClockMode mode) {
    clock_mode_ = mode;
    return *this;
}

TradingCoreBuilder& TradingCoreBuilder::with_risk_limits(const RiskLimits& limits) {
    risk_limits_ = limits;
    return *this;
}

TradingCoreBuilder& TradingCoreBuilder::with_callbacks(const TradingCallbacks& callbacks) {
    callbacks_ = callbacks;
    return *this;
}

std::unique_ptr<TradingCore> TradingCoreBuilder::build() {
    auto core = std::make_unique<TradingCore>();
    core->initialize(clock_mode_);
    core->set_risk_limits(risk_limits_);
    core->set_callbacks(callbacks_);
    return core;
}

} // namespace fasttrade::core
