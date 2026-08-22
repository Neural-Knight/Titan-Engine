# Module 8: throughput via Google Benchmark, latency via titan_bench's harness.
set(TITAN_BENCH_TARGETS bench_match bench_cancel bench_multi_symbol bench_snapshot)

add_executable(bench_match bench_match.cpp)
add_executable(bench_cancel bench_cancel.cpp)
add_executable(bench_multi_symbol bench_multi_symbol.cpp)
add_executable(bench_snapshot bench_snapshot.cpp)

foreach(target ${TITAN_BENCH_TARGETS})
	target_link_libraries(${target}
		PRIVATE
			titan_exchange
			titan_market_data
			titan_bench
			benchmark::benchmark
	)
	target_compile_definitions(${target}
		PRIVATE
			TITAN_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
	)
	titan_apply_release_flags(${target})
endforeach()

# Module 12: same steady_limit_flow/burst_crossing scenarios, OptimizedMatcher instead.
add_executable(bench_match_optimized bench_match_optimized.cpp)

target_link_libraries(bench_match_optimized
	PRIVATE
		titan_exchange
		titan_book
		titan_market_data
		titan_bench
		benchmark::benchmark
)

target_compile_definitions(bench_match_optimized
	PRIVATE
		TITAN_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
)
titan_apply_release_flags(bench_match_optimized)

# Module 13: allocation counting via global operator new/delete overrides,
# scoped to this one executable -- does not affect titan_tests.
add_executable(bench_allocations bench_allocations.cpp)

target_link_libraries(bench_allocations
	PRIVATE
		titan_exchange
		titan_bench
)

target_compile_definitions(bench_allocations
	PRIVATE
		TITAN_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
)
titan_apply_release_flags(bench_allocations)

# Module 14: single-threaded direct replay vs StagedProcessor's SPSC pipeline.
add_executable(bench_pipeline bench_pipeline.cpp)

target_link_libraries(bench_pipeline
	PRIVATE
		titan_pipeline
		titan_platform
		titan_bench
)

target_compile_definitions(bench_pipeline
	PRIVATE
		TITAN_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
)
titan_apply_release_flags(bench_pipeline)
