#include "fasttrade/core/clock.hpp"
#include <chrono>

namespace fasttrade::core {

// Static member for GlobalClock
std::unique_ptr<Clock> GlobalClock::instance_ = nullptr;

Clock::Clock(ClockMode mode) 
    : mode_(mode), running_(false) {
    start_time_ = std::chrono::high_resolution_clock::now();
    current_time_ = start_time_;
}

Clock::~Clock() {
    stop();
}

Clock::Clock(Clock&& other) noexcept 
    : mode_(other.mode_), 
      start_time_(other.start_time_),
      current_time_(other.current_time_),
      running_(other.running_.load()),
      events_(std::move(other.events_)),
      timer_thread_(std::move(other.timer_thread_)) {
    other.running_ = false;
}

Clock& Clock::operator=(Clock&& other) noexcept {
    if (this != &other) {
        stop(); // Stop current timer if running
        
        mode_ = other.mode_;
        start_time_ = other.start_time_;
        current_time_ = other.current_time_;
        running_ = other.running_.load();
        events_ = std::move(other.events_);
        timer_thread_ = std::move(other.timer_thread_);
        
        other.running_ = false;
    }
    return *this;
}

void Clock::start() {
    if (running_.load()) {
        return; // Already running
    }
    
    running_ = true;
    
    if (mode_ == ClockMode::REALTIME) {
        timer_thread_ = std::thread(&Clock::timer_loop, this);
    }
}

void Clock::stop() {
    if (!running_.load()) {
        return; // Already stopped
    }
    
    running_ = false;
    
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void Clock::timer_loop() {
    while (running_.load()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        
        // Process scheduled events
        auto it = events_.begin();
        while (it != events_.end()) {
            auto& event = *it;
            if (current_time >= event->scheduled_time) {
                // Execute callback
                try {
                    event->callback();
                } catch (...) {
                    // Ignore callback exceptions to prevent timer thread crash
                }
                
                if (event->recurring) {
                    // Reschedule recurring event
                    event->scheduled_time = current_time + event->interval;
                    ++it;
                } else {
                    // Remove one-time event
                    it = events_.erase(it);
                }
            } else {
                ++it;
            }
        }
        
        // Sleep for a short interval to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

Timestamp Clock::now() const {
    if (mode_ == ClockMode::REALTIME) {
        return std::chrono::high_resolution_clock::now();
    } else {
        // For backtesting/simulation, return controlled time
        return current_time_;
    }
}

int64_t Clock::now_ns() const {
    return to_nanoseconds(now());
}

int64_t Clock::now_ms() const {
    return to_milliseconds(now());
}

void Clock::schedule_once(Duration delay, ScheduledCallback callback) {
    auto event = std::make_unique<ScheduledEvent>();
    event->scheduled_time = now() + delay;
    event->callback = std::move(callback);
    event->recurring = false;
    
    events_.push_back(std::move(event));
}

void Clock::schedule_recurring(Duration interval, ScheduledCallback callback) {
    auto event = std::make_unique<ScheduledEvent>();
    event->scheduled_time = now() + interval;
    event->callback = std::move(callback);
    event->recurring = true;
    event->interval = interval;
    
    events_.push_back(std::move(event));
}

void Clock::set_time(Timestamp time) {
    if (mode_ != ClockMode::BACKTEST && mode_ != ClockMode::SIMULATION) {
        return; // Only allowed in controlled time modes
    }
    current_time_ = time;
}

void Clock::advance_time(Duration duration) {
    if (mode_ != ClockMode::BACKTEST && mode_ != ClockMode::SIMULATION) {
        return; // Only allowed in controlled time modes
    }
    current_time_ += duration;
}

// Static utility functions
Timestamp Clock::from_milliseconds(int64_t ms) {
    return Timestamp(std::chrono::milliseconds(ms));
}

Timestamp Clock::from_nanoseconds(int64_t ns) {
    return Timestamp(std::chrono::nanoseconds(ns));
}

int64_t Clock::to_milliseconds(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()).count();
}

int64_t Clock::to_nanoseconds(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        ts.time_since_epoch()).count();
}

Duration Clock::milliseconds(int64_t ms) {
    return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(ms));
}

Duration Clock::microseconds(int64_t us) {
    return std::chrono::duration_cast<Duration>(std::chrono::microseconds(us));
}

Duration Clock::nanoseconds(int64_t ns) {
    return Duration(ns);
}

// GlobalClock implementation
void GlobalClock::initialize(ClockMode mode) {
    instance_ = std::make_unique<Clock>(mode);
    instance_->start();
}

Clock& GlobalClock::instance() {
    if (!instance_) {
        initialize(); // Initialize with default mode if not already done
    }
    return *instance_;
}

void GlobalClock::cleanup() {
    if (instance_) {
        instance_->stop();
        instance_.reset();
    }
}

} // namespace fasttrade::core
