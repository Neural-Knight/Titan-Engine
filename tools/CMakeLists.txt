add_executable(itch_dump itch_dump.cpp)

target_link_libraries(itch_dump
	PRIVATE
		titan_itch
)

add_executable(itch_replay itch_replay.cpp)

target_link_libraries(itch_replay
	PRIVATE
		titan_itch
)
