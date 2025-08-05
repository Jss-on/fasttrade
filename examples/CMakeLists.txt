# Examples for fasttrade framework

# Basic usage example
add_executable(basic_trading_example basic_trading.cpp)
target_link_libraries(basic_trading_example fasttrade)

# Order book example
add_executable(order_book_example order_book_demo.cpp)
target_link_libraries(order_book_example fasttrade)

# Portfolio management example
add_executable(portfolio_example portfolio_management.cpp)
target_link_libraries(portfolio_example fasttrade)

# Backtesting example
add_executable(backtest_example backtesting.cpp)
target_link_libraries(backtest_example fasttrade)

# High-frequency trading example
add_executable(hft_example high_frequency_trading.cpp)
target_link_libraries(hft_example fasttrade)
