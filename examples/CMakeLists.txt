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

# Market data example
add_executable(market_data_example market_data_example.cpp)
target_link_libraries(market_data_example fasttrade)

# WebSocket connector tests
add_executable(test_binance_connector test_binance_connector.cpp)
target_link_libraries(test_binance_connector fasttrade)

add_executable(test_bybit_connector test_bybit_connector.cpp)
target_link_libraries(test_bybit_connector fasttrade)

add_executable(test_okx_connector test_okx_connector.cpp)
target_link_libraries(test_okx_connector fasttrade)

add_executable(run_all_tests run_all_tests.cpp)
target_link_libraries(run_all_tests fasttrade)
