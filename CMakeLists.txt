cmake_minimum_required(VERSION 3.20)

project(titan_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Off by default: default build stays debug-equivalent (no -O flags).
option(TITAN_BENCHMARK_RELEASE "Build benchmarks + their libs with -O2 -DNDEBUG" OFF)

# Applies -O2 -DNDEBUG to one target, only when the option above is ON.
function(titan_apply_release_flags target)
	if(TITAN_BENCHMARK_RELEASE)
		target_compile_options(${target} PRIVATE -O2)
		target_compile_definitions(${target} PRIVATE NDEBUG)
	endif()
endfunction()

include(FetchContent)

FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	googlebenchmark
	URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googlebenchmark)

# titan_reference: correctness-oracle matcher. Do not optimize.
add_library(titan_reference STATIC
	reference/order_book.cpp
)

target_include_directories(titan_reference
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)
titan_apply_release_flags(titan_reference)

# titan_optimized: OptimizedMatcher, a drop-in IMatcher. No dependency on
# titan_exchange -- instrument_registry.cpp (in titan_exchange) depends on
# this, not the other way, so linking stays acyclic.
add_library(titan_optimized STATIC
	src/book/optimized_matcher.cpp
)

target_include_directories(titan_optimized
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)
titan_apply_release_flags(titan_optimized)

# titan_exchange: OrderManager + Validator, layered on top of IMatcher.
add_library(titan_exchange STATIC
	src/exchange/validator.cpp
	src/exchange/order_manager.cpp
	src/exchange/instrument_registry.cpp
)

target_link_libraries(titan_exchange
	PUBLIC
		titan_reference
		titan_optimized
)
titan_apply_release_flags(titan_exchange)

# titan_book: cross-cutting book invariants, checked against both layers.
add_library(titan_book STATIC
	src/book/invariants.cpp
)

target_link_libraries(titan_book
	PUBLIC
		titan_exchange
)
titan_apply_release_flags(titan_book)

# titan_market_data: derives ExecutionReports from InstrumentRegistry's event log.
add_library(titan_market_data STATIC
	src/market_data/publisher.cpp
)

target_include_directories(titan_market_data
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)
titan_apply_release_flags(titan_market_data)

# titan_bench: histogram + harness + scenario loader, used by benchmarks/.
add_library(titan_bench STATIC
	src/bench/latency_histogram.cpp
	src/bench/harness.cpp
	src/bench/scenario.cpp
)

target_include_directories(titan_bench
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)
titan_apply_release_flags(titan_bench)

# titan_itch: ITCH 5.0 framing/decoding + feed-side book reconstruction.
add_library(titan_itch STATIC
	src/feed/itch/parser.cpp
	src/feed/itch/decoder.cpp
	src/feed/itch/book_builder.cpp
)

target_include_directories(titan_itch
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

# titan_replay: replays decoded ITCH messages into InstrumentRegistry, with
# parity checking against ItchBookBuilder.
add_library(titan_replay STATIC
	src/replay/itch_adapter.cpp
	src/replay/parity_checker.cpp
	src/replay/event_replayer.cpp
)

target_include_directories(titan_replay
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(titan_replay
	PUBLIC
		titan_itch
		titan_exchange
)

find_package(Threads REQUIRED)

# titan_platform: thread pinning, Linux-only with no-op stubs elsewhere.
add_library(titan_platform STATIC
	src/platform/thread_affinity.cpp
)

target_include_directories(titan_platform
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(titan_platform
	PUBLIC
		Threads::Threads
)

# titan_pipeline: SPSC producer/consumer wrapper around InstrumentRegistry.
add_library(titan_pipeline STATIC
	src/pipeline/staged_processor.cpp
)

target_include_directories(titan_pipeline
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(titan_pipeline
	PUBLIC
		titan_exchange
		titan_platform
		Threads::Threads
)
titan_apply_release_flags(titan_pipeline)

add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(tools)
