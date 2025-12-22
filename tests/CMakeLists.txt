# Test Bank Core
add_executable(test_bank test_bank.c)
target_link_libraries(test_bank PRIVATE common pthread rt)

# Test Logger
add_executable(test_logger test_logger.c)
target_link_libraries(test_logger PRIVATE common pthread rt)

# Test Monitor (Stress Test)
add_executable(test_monitor test_monitor.c)
target_link_libraries(test_monitor PRIVATE common pthread rt m)