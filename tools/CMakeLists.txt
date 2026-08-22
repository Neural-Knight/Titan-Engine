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

add_executable(replay_engine replay_engine.cpp)

target_link_libraries(replay_engine
	PRIVATE
		titan_replay
)

add_executable(itch_listen itch_listen.cpp)

target_link_libraries(itch_listen
	PRIVATE
		titan_feed_tcp
)
