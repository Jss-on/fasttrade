#pragma once

/**
 * @file fasttrade.hpp
 * @brief Main header file for the fasttrade C++ trading framework
 * @author fasttrade team
 * @version 1.0.0
 * 
 * This is the main header that includes all essential fasttrade components.
 * Simply #include <fasttrade/fasttrade.hpp> to get started.
 */

// Core components
#include "core/order_book.hpp"
#include "core/trading_core.hpp"
#include "core/limit_order.hpp"
#include "core/clock.hpp"

// Utilities
#include "utils/decimal.hpp"

namespace fasttrade {

/**
 * @brief Initialize the fasttrade framework
 * 
 * Call this once at the start of your application to set up
 * internal resources and optimize performance.
 */
void initialize();

/**
 * @brief Clean up fasttrade framework
 * 
 * Call this when shutting down to properly clean up resources.
 */
void cleanup();

/**
 * @brief Get the version of the fasttrade framework
 * @return Version string
 */
const char* version();

} // namespace fasttrade
