# Test Bank Core
add_executable(test_bank test_bank.c)
target_link_libraries(test_bank PRIVATE common)

# Test Logger
add_executable(test_logger test_logger.c)
target_link_libraries(test_logger PRIVATE common)
