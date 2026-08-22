cmake_minimum_required(VERSION 3.20)

project(titan_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include(FetchContent)

FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# titan_reference: correctness-oracle matcher. Do not optimize.
add_library(titan_reference STATIC
	reference/order_book.cpp
)

target_include_directories(titan_reference
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

# titan_exchange: OrderManager + Validator, layered on top of IMatcher.
add_library(titan_exchange STATIC
	src/exchange/validator.cpp
	src/exchange/order_manager.cpp
	src/exchange/instrument_registry.cpp
)

target_link_libraries(titan_exchange
	PUBLIC
		titan_reference
)

# titan_book: cross-cutting book invariants, checked against both layers.
add_library(titan_book STATIC
	src/book/invariants.cpp
)

target_link_libraries(titan_book
	PUBLIC
		titan_exchange
)

# titan_market_data: derives ExecutionReports from InstrumentRegistry's event log.
add_library(titan_market_data STATIC
	src/market_data/publisher.cpp
)

target_include_directories(titan_market_data
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(tests)
add_subdirectory(benchmarks)
