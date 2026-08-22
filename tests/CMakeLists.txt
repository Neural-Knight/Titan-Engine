add_executable(titan_tests
	unit/test_reference_matcher.cpp
)

target_link_libraries(titan_tests
	PRIVATE
		titan_reference
		GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(titan_tests)
