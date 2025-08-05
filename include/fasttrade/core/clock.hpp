#pragma once

#include <chrono>
#include <functional>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace fasttrade::core {

/**
 * @brief High-precision timestamp type for trading operations
 */
using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;
using Duration = std::chrono::nanoseconds;

/**
 * @brief Clock modes for different trading scenarios
 */
enum class ClockMode {
    REALTIME,    ///< Real-time trading mode
    BACKTEST,    ///< Backtesting mode with controllable time
    SIMULATION   ///< Simulation mode with accelerated time
};

/**
 * @brief Callback function type for scheduled events
 */
using ScheduledCallback = std::function<void()>;

/**
 * @brief High-performance clock for trading operations
 * 
 * This class provides precise timing functionality for trading systems,
 * supporting both real-time and backtesting scenarios with nanosecond precision.
 */
class Clock {
private:
    struct ScheduledEvent {
        Timestamp scheduled_time;
        ScheduledCallback callback;
        bool recurring;
        Duration interval;
    };

    ClockMode mode_;
    Timestamp start_time_;
    Timestamp current_time_;
    std::atomic<bool> running_;
    std::vector<std::unique_ptr<ScheduledEvent>> events_;
    std::thread timer_thread_;
    
    void timer_loop();

public:
    /**
     * @brief Construct a new Clock object
     * @param mode The clock mode (default: REALTIME)
     */
    explicit Clock(ClockMode mode = ClockMode::REALTIME);
    
    /**
     * @brief Destructor
     */
    ~Clock();

    // Non-copyable, movable
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;
    Clock(Clock&& other) noexcept;
    Clock& operator=(Clock&& other) noexcept;

    /**
     * @brief Start the clock
     */
    void start();

    /**
     * @brief Stop the clock
     */
    void stop();

    /**
     * @brief Get current timestamp
     * @return Current timestamp
     */
    Timestamp now() const;

    /**
     * @brief Get current timestamp as nanoseconds since epoch
     * @return Nanoseconds since epoch
     */
    int64_t now_ns() const;

    /**
     * @brief Get current timestamp as milliseconds since epoch
     * @return Milliseconds since epoch
     */
    int64_t now_ms() const;

    /**
     * @brief Schedule a one-time callback
     * @param delay Delay before execution
     * @param callback Function to execute
     */
    void schedule_once(Duration delay, ScheduledCallback callback);

    /**
     * @brief Schedule a recurring callback
     * @param interval Interval between executions
     * @param callback Function to execute
     */
    void schedule_recurring(Duration interval, ScheduledCallback callback);

    /**
     * @brief Set the current time (for backtesting mode)
     * @param time New current time
     */
    void set_time(Timestamp time);

    /**
     * @brief Advance time by specified duration (for backtesting mode)
     * @param duration Duration to advance
     */
    void advance_time(Duration duration);

    /**
     * @brief Get the clock mode
     * @return Current clock mode
     */
    ClockMode mode() const { return mode_; }

    /**
     * @brief Check if the clock is running
     * @return True if running, false otherwise
     */
    bool is_running() const { return running_.load(); }

    // Utility functions for common time operations
    static Timestamp from_milliseconds(int64_t ms);
    static Timestamp from_nanoseconds(int64_t ns);
    static int64_t to_milliseconds(Timestamp ts);
    static int64_t to_nanoseconds(Timestamp ts);
    static Duration milliseconds(int64_t ms);
    static Duration microseconds(int64_t us);
    static Duration nanoseconds(int64_t ns);
};

/**
 * @brief Global clock instance for easy access
 * 
 * This provides a convenient way to access timing functionality
 * throughout the application without passing clock instances around.
 */
class GlobalClock {
private:
    static std::unique_ptr<Clock> instance_;
    
public:
    /**
     * @brief Initialize the global clock
     * @param mode Clock mode to use
     */
    static void initialize(ClockMode mode = ClockMode::REALTIME);

    /**
     * @brief Get the global clock instance
     * @return Reference to the global clock
     */
    static Clock& instance();

    /**
     * @brief Clean up the global clock
     */
    static void cleanup();

    // Convenient static methods that delegate to the global instance
    static Timestamp now() { return instance().now(); }
    static int64_t now_ns() { return instance().now_ns(); }
    static int64_t now_ms() { return instance().now_ms(); }
};

} // namespace fasttrade::core
