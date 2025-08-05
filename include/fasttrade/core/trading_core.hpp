#pragma once

#include "order_book.hpp"
#include "limit_order.hpp"
#include "clock.hpp"
#include "../utils/decimal.hpp"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>

namespace fasttrade::core {

/**
 * @brief Portfolio position information
 */
struct Position {
    std::string symbol;
    utils::Decimal quantity;
    utils::Decimal average_price;
    utils::Decimal unrealized_pnl;
    utils::Decimal realized_pnl;
    Timestamp last_update;

    Position() = default;
    Position(const std::string& sym, const utils::Decimal& qty, const utils::Decimal& price)
        : symbol(sym), quantity(qty), average_price(price), 
          unrealized_pnl(utils::Decimal::zero()), realized_pnl(utils::Decimal::zero()),
          last_update(GlobalClock::now()) {}
};

/**
 * @brief Account balance information
 */
struct Balance {
    std::string currency;
    utils::Decimal total;
    utils::Decimal available;
    utils::Decimal locked;
    Timestamp last_update;

    Balance() = default;
    Balance(const std::string& cur, const utils::Decimal& tot, const utils::Decimal& avail)
        : currency(cur), total(tot), available(avail), 
          locked(tot - avail), last_update(GlobalClock::now()) {}
};

/**
 * @brief Trade execution information
 */
struct Trade {
    std::string trade_id;
    std::string client_order_id;
    std::string exchange_order_id;
    std::string symbol;
    OrderSide side;
    utils::Decimal price;
    utils::Decimal quantity;
    utils::Decimal fee;
    std::string fee_currency;
    Timestamp timestamp;

    Trade() = default;
};

/**
 * @brief Risk management parameters
 */
struct RiskLimits {
    utils::Decimal max_position_size;      ///< Maximum position size per symbol
    utils::Decimal max_order_size;         ///< Maximum single order size
    utils::Decimal max_daily_loss;         ///< Maximum daily loss limit
    utils::Decimal max_drawdown;           ///< Maximum drawdown limit
    int max_orders_per_second;             ///< Order rate limiting
    bool enable_position_limits = true;
    bool enable_order_limits = true;
    bool enable_loss_limits = true;
};

/**
 * @brief Trading event callbacks
 */
struct TradingCallbacks {
    std::function<void(const LimitOrder&)> on_order_filled;
    std::function<void(const LimitOrder&)> on_order_cancelled;
    std::function<void(const LimitOrder&)> on_order_rejected;
    std::function<void(const Trade&)> on_trade_executed;
    std::function<void(const std::string&, const std::string&)> on_error;
    std::function<void(const Position&)> on_position_update;
    std::function<void(const Balance&)> on_balance_update;
};

/**
 * @brief High-performance trading core engine
 * 
 * This is the main trading engine that orchestrates order management,
 * portfolio tracking, risk management, and market data processing
 * with a clean, easy-to-use interface.
 */
class TradingCore {
private:
    // Core components
    std::unique_ptr<OrderBookManager> order_book_manager_;
    std::unique_ptr<Clock> clock_;
    
    // State management
    std::map<std::string, std::unique_ptr<LimitOrder>> active_orders_;
    std::map<std::string, Position> positions_;
    std::map<std::string, Balance> balances_;
    std::vector<Trade> trade_history_;
    
    // Risk management
    RiskLimits risk_limits_;
    utils::Decimal daily_pnl_;
    utils::Decimal total_pnl_;
    
    // Event handling
    TradingCallbacks callbacks_;
    std::queue<std::function<void()>> event_queue_;
    std::thread event_processor_;
    std::atomic<bool> running_;
    
    // Thread safety
    mutable std::shared_mutex state_mutex_;
    std::mutex event_mutex_;
    
    // Internal methods
    void process_events();
    bool validate_order(const LimitOrder& order);
    void update_position(const Trade& trade);
    void update_balance(const std::string& currency, const utils::Decimal& delta);
    void calculate_pnl();

public:
    /**
     * @brief Construct a new Trading Core
     */
    TradingCore();
    
    /**
     * @brief Destructor
     */
    ~TradingCore();

    // Non-copyable, movable
    TradingCore(const TradingCore&) = delete;
    TradingCore& operator=(const TradingCore&) = delete;
    TradingCore(TradingCore&& other) noexcept;
    TradingCore& operator=(TradingCore&& other) noexcept;

    /**
     * @brief Initialize the trading core
     * @param clock_mode Clock mode to use
     */
    void initialize(ClockMode clock_mode = ClockMode::REALTIME);

    /**
     * @brief Start the trading core
     */
    void start();

    /**
     * @brief Stop the trading core
     */
    void stop();

    /**
     * @brief Check if the trading core is running
     * @return True if running
     */
    bool is_running() const { return running_.load(); }

    // Order Management
    /**
     * @brief Submit a new order
     * @param order Order to submit
     * @return True if order was accepted
     */
    bool submit_order(const LimitOrder& order);

    /**
     * @brief Cancel an existing order
     * @param client_order_id Client order ID to cancel
     * @return True if cancel request was sent
     */
    bool cancel_order(const std::string& client_order_id);

    /**
     * @brief Modify an existing order
     * @param client_order_id Order to modify
     * @param new_price New price (empty to keep current)
     * @param new_quantity New quantity (empty to keep current)
     * @return True if modify request was sent
     */
    bool modify_order(const std::string& client_order_id, 
                     const utils::Decimal& new_price = utils::Decimal(),
                     const utils::Decimal& new_quantity = utils::Decimal());

    /**
     * @brief Get all active orders
     * @return Vector of active orders
     */
    std::vector<LimitOrder> get_active_orders() const;

    /**
     * @brief Get active orders for a specific symbol
     * @param symbol Trading symbol
     * @return Vector of orders for the symbol
     */
    std::vector<LimitOrder> get_active_orders(const std::string& symbol) const;

    // Market Data
    /**
     * @brief Get order book for a symbol
     * @param symbol Trading symbol
     * @return Reference to order book
     */
    OrderBook& get_order_book(const std::string& symbol);

    /**
     * @brief Subscribe to market data for a symbol
     * @param symbol Trading symbol
     */
    void subscribe_market_data(const std::string& symbol);

    /**
     * @brief Unsubscribe from market data for a symbol
     * @param symbol Trading symbol
     */
    void unsubscribe_market_data(const std::string& symbol);

    // Portfolio Management
    /**
     * @brief Get current position for a symbol
     * @param symbol Trading symbol
     * @return Position (zero if no position)
     */
    Position get_position(const std::string& symbol) const;

    /**
     * @brief Get all positions
     * @return Map of symbol to position
     */
    std::map<std::string, Position> get_all_positions() const;

    /**
     * @brief Get balance for a currency
     * @param currency Currency symbol
     * @return Balance (zero if no balance)
     */
    Balance get_balance(const std::string& currency) const;

    /**
     * @brief Get all balances
     * @return Map of currency to balance
     */
    std::map<std::string, Balance> get_all_balances() const;

    /**
     * @brief Get total portfolio value in base currency
     * @param base_currency Base currency for valuation
     * @return Total portfolio value
     */
    utils::Decimal get_portfolio_value(const std::string& base_currency = "USDT") const;

    /**
     * @brief Get realized P&L
     * @return Realized profit and loss
     */
    utils::Decimal get_realized_pnl() const { return total_pnl_; }

    /**
     * @brief Get unrealized P&L
     * @return Unrealized profit and loss
     */
    utils::Decimal get_unrealized_pnl() const;

    /**
     * @brief Get daily P&L
     * @return Daily profit and loss
     */
    utils::Decimal get_daily_pnl() const { return daily_pnl_; }

    // Risk Management
    /**
     * @brief Set risk management limits
     * @param limits Risk limit parameters
     */
    void set_risk_limits(const RiskLimits& limits);

    /**
     * @brief Get current risk limits
     * @return Current risk limits
     */
    const RiskLimits& get_risk_limits() const { return risk_limits_; }

    /**
     * @brief Check if an order passes risk checks
     * @param order Order to check
     * @return True if order passes risk checks
     */
    bool check_risk_limits(const LimitOrder& order) const;

    // Event Handling
    /**
     * @brief Set trading event callbacks
     * @param callbacks Callback functions
     */
    void set_callbacks(const TradingCallbacks& callbacks);

    // Trading History
    /**
     * @brief Get trade history
     * @param limit Maximum number of trades (0 = all)
     * @return Vector of trades
     */
    std::vector<Trade> get_trade_history(size_t limit = 0) const;

    /**
     * @brief Get trade history for a symbol
     * @param symbol Trading symbol
     * @param limit Maximum number of trades (0 = all)
     * @return Vector of trades for the symbol
     */
    std::vector<Trade> get_trade_history(const std::string& symbol, size_t limit = 0) const;

    // Utility Methods
    /**
     * @brief Get current timestamp from the trading clock
     * @return Current timestamp
     */
    Timestamp now() const;

    /**
     * @brief Get trading statistics
     * @return JSON string with trading statistics
     */
    std::string get_statistics() const;

    /**
     * @brief Reset all state (for backtesting)
     */
    void reset();

    /**
     * @brief Export state to JSON (for persistence)
     * @return JSON string representation
     */
    std::string export_state() const;

    /**
     * @brief Import state from JSON
     * @param json JSON string representation
     * @return True if import was successful
     */
    bool import_state(const std::string& json);
};

/**
 * @brief Factory class for easy TradingCore creation
 */
class TradingCoreBuilder {
private:
    ClockMode clock_mode_ = ClockMode::REALTIME;
    RiskLimits risk_limits_;
    TradingCallbacks callbacks_;

public:
    /**
     * @brief Set clock mode
     * @param mode Clock mode
     * @return Reference to builder
     */
    TradingCoreBuilder& with_clock_mode(ClockMode mode);

    /**
     * @brief Set risk limits
     * @param limits Risk limit parameters
     * @return Reference to builder
     */
    TradingCoreBuilder& with_risk_limits(const RiskLimits& limits);

    /**
     * @brief Set callbacks
     * @param callbacks Event callbacks
     * @return Reference to builder
     */
    TradingCoreBuilder& with_callbacks(const TradingCallbacks& callbacks);

    /**
     * @brief Build the trading core
     * @return Unique pointer to TradingCore
     */
    std::unique_ptr<TradingCore> build();
};

} // namespace fasttrade::core
