add_executable(titan_tests
	unit/test_reference_matcher.cpp
	unit/test_validator.cpp
	unit/test_order_lifecycle.cpp
	unit/test_market_orders.cpp
	unit/test_ioc_orders.cpp
	unit/test_cancel_replace.cpp
	unit/test_stp.cpp
	fuzz/fuzz_order_book.cpp
)

target_include_directories(titan_tests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/fuzz
)

target_link_libraries(titan_tests
	PRIVATE
		titan_book
		GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(titan_tests)
