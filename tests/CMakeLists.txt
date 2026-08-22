add_executable(titan_tests
	unit/test_reference_matcher.cpp
	unit/test_validator.cpp
	unit/test_order_lifecycle.cpp
	unit/test_market_orders.cpp
	unit/test_ioc_orders.cpp
	unit/test_cancel_replace.cpp
	unit/test_stp.cpp
	unit/test_order_entry_routing.cpp
	unit/test_multi_symbol.cpp
	unit/test_execution_reports.cpp
	unit/test_book_snapshots.cpp
	unit/test_latency_histogram.cpp
	unit/test_scenario_loader.cpp
	unit/test_itch_parser.cpp
	integration/test_event_dispatch.cpp
	integration/test_itch_book_builder.cpp
	integration/test_itch_checkpoints.cpp
	integration/test_itch_adapter.cpp
	integration/test_itch_engine_parity.cpp
	integration/test_itch_engine_parity_optimized.cpp
	parity/test_reference_vs_optimized.cpp
	fuzz/fuzz_order_book.cpp
)

target_include_directories(titan_tests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/fuzz
)

target_compile_definitions(titan_tests
	PRIVATE
		TITAN_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../benchmarks/scenarios"
		TITAN_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(titan_tests
	PRIVATE
		titan_book
		titan_market_data
		titan_bench
		titan_itch
		titan_replay
		GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(titan_tests)
