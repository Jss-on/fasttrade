# FastTrade 🚀

**High-Performance C++ Trading Framework**

FastTrade is a modern, high-performance C++ trading framework designed for speed, reliability, and ease of use. Born from the need to optimize Python-based trading systems, FastTrade provides sub-millisecond execution with an intuitive, developer-friendly API.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## ✨ Key Features

### 🏎️ **Ultra-High Performance**
- **Sub-millisecond latency** for order processing
- **Lock-free data structures** for real-time market data
- **Memory-efficient** order book implementation
- **Multi-threaded** architecture with minimal contention

### 🎯 **Easy-to-Use Interface**
```cpp
// Simple order creation with fluent interface
auto order = OrderBuilder()
    .id("BTC_BUY_001")
    .pair("BTC-USDT")
    .buy(Decimal("0.1"))
    .at_price(Decimal("50000"))
    .build();

// One-line trading core setup
auto core = TradingCoreBuilder()
    .with_clock_mode(ClockMode::REALTIME)
    .with_risk_limits(risk_limits)
    .build();
```

### 🛡️ **Built-in Risk Management**
- Position size limits
- Order rate limiting  
- Daily loss limits
- Real-time portfolio tracking

### 📊 **Advanced Market Data**
- High-precision decimal arithmetic
- Real-time order book processing
- Market impact analysis
- Multi-symbol support

### 🔄 **Multiple Trading Modes**
- **Real-time trading** with live market data
- **Backtesting** with historical data
- **Simulation** with accelerated time

## 🏗️ Architecture

FastTrade is built with a modular, layered architecture:

```
┌─────────────────────────────────────┐
│           Trading Core              │  ← Main orchestrator
├─────────────────────────────────────┤
│  Order Book  │ Limit Orders │ Clock │  ← Core components
├─────────────────────────────────────┤
│           Utilities                 │  ← Decimal, helpers
└─────────────────────────────────────┘
```

### Core Components

- **📊 OrderBook**: Thread-safe, high-performance order book with price-time priority
- **📋 LimitOrder**: Complete order lifecycle management with all major order types
- **⏱️ Clock**: High-precision timing system supporting real-time and backtesting modes
- **🎯 TradingCore**: Main engine orchestrating portfolio management, risk controls, and execution
- **🔢 Decimal**: High-precision arithmetic avoiding floating-point errors

## 🚀 Quick Start

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- Threading support

### Build Instructions

FastTrade uses **CMake Presets** for streamlined building and testing across different configurations.

```bash
# Clone the repository
git clone https://github.com/Jss-on/fasttrade.git
cd fasttrade
```

#### 🔧 **Available Build Presets**

| Preset | Description | Use Case |
|--------|-------------|----------|
| `default` | Balanced build with debug info | Development & testing |
| `debug` | Full debug build with warnings | Debugging & development |
| `release` | Optimized production build | Deployment |
| `testing` | Debug build with coverage | Unit testing & CI |
| `performance` | Maximum optimization | Benchmarking |
| `python` | Build with Python bindings | Python integration |

#### 🚀 **Quick Build (Recommended)**

```bash
# Configure with default preset
cmake --preset=default

# Build with default preset
cmake --build --preset=default

# Run examples
./build/default/examples/basic_trading_example
```

#### 🐛 **Development Build**

```bash
# For development with full debugging
cmake --preset=debug
cmake --build --preset=debug
```

#### 🚀 **Production Build**

```bash
# For optimized production deployment
cmake --preset=release
cmake --build --preset=release
```

#### 🧪 **Testing Build**

```bash
# For running tests with coverage
cmake --preset=testing
cmake --build --preset=testing
ctest --preset=testing
```

#### ⚡ **Performance Build**

```bash
# For maximum performance benchmarking
cmake --preset=performance
cmake --build --preset=performance
```

### Your First Trading Program

```cpp
#include <fasttrade/fasttrade.hpp>

using namespace fasttrade::core;
using namespace fasttrade::utils;

int main() {
    // Initialize the framework
    fasttrade::initialize();
    
    // Create and configure trading core
    auto trading_core = TradingCoreBuilder()
        .with_clock_mode(ClockMode::REALTIME)
        .build();
    
    // Set up callbacks
    TradingCallbacks callbacks;
    callbacks.on_trade_executed = [](const Trade& trade) {
        std::cout << "Trade: " << trade.symbol << " " 
                  << trade.quantity << " @ " << trade.price << std::endl;
    };
    trading_core->set_callbacks(callbacks);
    
    // Start trading
    trading_core->initialize();
    trading_core->start();
    
    // Subscribe to market data
    trading_core->subscribe_market_data("BTC-USDT");
    
    // Create and submit an order
    auto order = OrderBuilder()
        .id("MY_FIRST_ORDER")
        .pair("BTC-USDT")
        .buy(Decimal("0.01"))
        .at_price(Decimal("50000"))
        .build();
    
    trading_core->submit_order(order);
    
    // Clean up
    trading_core->stop();
    fasttrade::cleanup();
    return 0;
}
```

## 📚 Examples

FastTrade comes with comprehensive examples demonstrating various use cases:

### 1. **Basic Trading** (`examples/basic_trading.cpp`)
Demonstrates fundamental operations:
- Order creation and submission
- Market data subscription
- Order book inspection
- Event handling

### 2. **Order Book Management** (`examples/order_book_demo.cpp`)
Shows advanced order book features:
- Real-time order book updates
- Market impact analysis
- Volume analysis
- Order book snapshots

### 3. **Portfolio Management** (`examples/portfolio_management.cpp`)
Complete portfolio tracking:
- Position management
- P&L calculation
- Risk management
- Balance tracking

### 4. **Backtesting** (`examples/backtesting.cpp`)
Historical strategy testing:
- Moving average crossover strategy
- Performance analysis
- Buy & hold comparison
- Strategy metrics

### 5. **High-Frequency Trading** (`examples/high_frequency_trading.cpp`)
Ultra-fast trading simulation:
- Market making strategy
- Sub-millisecond execution
- Performance monitoring
- Latency analysis

### Running Examples

```bash
# Build examples
cd build
make

# Run basic trading example
./examples/basic_trading_example

# Run order book demo
./examples/order_book_example

# Run portfolio management demo
./examples/portfolio_example

# Run backtesting example
./examples/backtest_example

# Run HFT simulation
./examples/hft_example
```

## 🎯 Key APIs

### Order Management

```cpp
// Fluent order builder
auto order = OrderBuilder()
    .id("ORDER_123")
    .pair("BTC-USDT")
    .buy(Decimal("1.0"))        // or .sell()
    .at_price(Decimal("50000"))  // or .market_order()
    .build();

// Order operations
trading_core->submit_order(order);
trading_core->cancel_order("ORDER_123");
trading_core->modify_order("ORDER_123", new_price, new_quantity);

// Query orders
auto active_orders = trading_core->get_active_orders();
auto btc_orders = trading_core->get_active_orders("BTC-USDT");
```

### Market Data

```cpp
// Subscribe to symbols
trading_core->subscribe_market_data("BTC-USDT");
trading_core->subscribe_market_data("ETH-USDT");

// Access order book
auto& book = trading_core->get_order_book("BTC-USDT");

// Market data queries
auto best_bid = book.best_bid();
auto best_ask = book.best_ask();
auto mid_price = book.mid_price();
auto spread = book.spread();

// Get market depth
auto bids = book.get_bids(10);  // Top 10 bid levels
auto asks = book.get_asks(10);  // Top 10 ask levels

// Market impact analysis
auto impact_price = book.get_impact_price(true, Decimal("1.0"));
auto available_volume = book.get_volume_at_price(true, Decimal("50000"));
```

### Portfolio Management

```cpp
// Position queries
auto btc_position = trading_core->get_position("BTC-USDT");
auto all_positions = trading_core->get_all_positions();

// Balance queries
auto usdt_balance = trading_core->get_balance("USDT");
auto all_balances = trading_core->get_all_balances();

// P&L tracking
auto realized_pnl = trading_core->get_realized_pnl();
auto unrealized_pnl = trading_core->get_unrealized_pnl();
auto daily_pnl = trading_core->get_daily_pnl();

// Portfolio valuation
auto total_value = trading_core->get_portfolio_value("USDT");
```

### Risk Management

```cpp
// Configure risk limits
RiskLimits limits;
limits.max_position_size = Decimal("10.0");    // Max 10 BTC
limits.max_order_size = Decimal("1.0");        // Max 1 BTC per order
limits.max_daily_loss = Decimal("1000.0");     // Max $1000 daily loss
limits.max_orders_per_second = 10;

trading_core->set_risk_limits(limits);

// Risk checks
if (trading_core->check_risk_limits(order)) {
    // Order passes risk checks
    trading_core->submit_order(order);
}
```

### High-Precision Decimals

```cpp
// Create decimals
Decimal price("50000.123456");     // From string
Decimal quantity(1.5);             // From double
Decimal amount = Decimal("100");    // From integer

// Arithmetic operations
Decimal total = price * quantity;
Decimal average = (price1 + price2) / Decimal("2");

// Comparisons
if (price > Decimal("50000")) {
    // Price above threshold
}

// Conversions
double price_double = price.to_double();
std::string price_str = price.to_string();
```

## ⚙️ Configuration

### Build Options

```cmake
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_EXAMPLES "Build examples" ON)
```

### Trading Core Configuration

```cpp
// Clock modes
ClockMode::REALTIME     // Real-time trading
ClockMode::BACKTEST     // Backtesting with controllable time
ClockMode::SIMULATION   // Simulation with accelerated time

// Risk management
RiskLimits limits;
limits.enable_position_limits = true;
limits.enable_order_limits = true;
limits.enable_loss_limits = true;
```

## 📈 Performance

FastTrade is designed for maximum performance:

### Benchmarks
- **Order processing**: < 1μs average latency
- **Order book updates**: < 500ns average latency  
- **Memory usage**: < 100MB for typical workloads
- **Throughput**: > 1M orders/second

### Optimization Features
- Lock-free data structures for hot paths
- Memory pool allocation for frequent objects
- SIMD optimizations for numerical calculations
- Template metaprogramming for compile-time optimizations

## 🧪 Testing

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
make

# Run test suite
./tests/fasttrade_tests

# Run specific test categories
./tests/fasttrade_tests --gtest_filter="OrderBook*"
./tests/fasttrade_tests --gtest_filter="TradingCore*"
```

## 🐍 Python Bindings

FastTrade provides Python bindings for rapid prototyping:

```python
import pyfasttrade as ft

# Create trading core
core = ft.TradingCore(ft.ClockMode.REALTIME)
core.initialize()
core.start()

# Create order
order = ft.OrderBuilder() \
    .id("PYTHON_ORDER") \
    .pair("BTC-USDT") \
    .buy(ft.Decimal("0.1")) \
    .at_price(ft.Decimal("50000")) \
    .build()

# Submit order
core.submit_order(order)
```

## 🔧 Integration

### Exchange Connectors
FastTrade can be extended with exchange-specific connectors:

```cpp
class BinanceConnector : public ExchangeConnector {
    void connect() override;
    void subscribe_market_data(const std::string& symbol) override;
    void submit_order(const LimitOrder& order) override;
};
```

### Custom Strategies
Implement trading strategies by extending the base class:

```cpp
class MomentumStrategy : public StrategyBase {
    void on_market_data_update(const OrderBookUpdate& update) override;
    void on_trade_executed(const Trade& trade) override;
};
```

## 🛠️ Development

### Project Structure
```
fasttrade/
├── include/fasttrade/          # Public headers
│   ├── core/                   # Core components
│   └── utils/                  # Utilities
├── src/                        # Implementation files
├── examples/                   # Usage examples
├── tests/                      # Test suite
├── python/                     # Python bindings
└── CMakeLists.txt             # Build configuration
```

### Contributing
1. Fork the repository
2. Create a feature branch
3. Make your changes with tests
4. Submit a pull request

## 📋 Roadmap

- [ ] **Exchange Integrations**: Binance, Coinbase Pro, FTX connectors
- [ ] **Advanced Strategies**: Statistical arbitrage, mean reversion
- [ ] **Machine Learning**: Integration with ML frameworks
- [ ] **Web Interface**: Real-time dashboard and monitoring
- [ ] **Database Integration**: Historical data storage and retrieval
- [ ] **Distributed Computing**: Multi-node deployment support

## 🤝 Why FastTrade?

### **vs. Python Trading Frameworks**
- **100x faster** order processing
- **Native threading** without GIL limitations
- **Memory efficient** with predictable performance
- **Type safety** at compile time

### **vs. Other C++ Frameworks**
- **Modern C++20** with clean, intuitive API
- **Easy integration** with existing systems
- **Comprehensive examples** and documentation
- **Active development** with regular updates

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Inspired by the Hummingbot project
- Built with modern C++ best practices
- Designed for the trading community

---

**Ready to trade at the speed of light?** 🚀

[Get Started](#-quick-start) | [View Examples](examples/) | [API Documentation](docs/) | [Join Community](https://discord.gg/fasttrade)