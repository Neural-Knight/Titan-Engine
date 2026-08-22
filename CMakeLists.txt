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

# titan_reference: the correctness-oracle matching engine (ReferenceMatcher),
# implementing IMatcher. Do not optimize this target — see reference/order_book.hpp.
add_library(titan_reference STATIC
	reference/order_book.cpp
)

target_include_directories(titan_reference
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

# titan_exchange: OrderManager + Validator — validation and lifecycle
# tracking layered on top of IMatcher. Links titan_reference so callers
# that depend on titan_exchange get the reference matcher transitively.
add_library(titan_exchange STATIC
	src/exchange/validator.cpp
	src/exchange/order_manager.cpp
)

target_link_libraries(titan_exchange
	PUBLIC
		titan_reference
)

add_subdirectory(tests)
add_subdirectory(benchmarks)
