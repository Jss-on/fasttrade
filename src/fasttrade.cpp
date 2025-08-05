#include "fasttrade/fasttrade.hpp"
#include "fasttrade/core/clock.hpp"
#include <iostream>

namespace fasttrade {

void initialize() {
    // Initialize the global clock
    core::GlobalClock::initialize(core::ClockMode::REALTIME);
    
    // Any other global initialization can go here
    // For example: logging setup, memory pools, etc.
}

void cleanup() {
    // Clean up global resources
    core::GlobalClock::cleanup();
    
    // Any other cleanup can go here
}

const char* version() {
    return "1.0.0";
}

} // namespace fasttrade
