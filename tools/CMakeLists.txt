add_executable(itch_dump itch_dump.cpp)

target_link_libraries(itch_dump
	PRIVATE
		titan_itch
)
