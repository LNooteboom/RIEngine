target_sources(${ENGINE_NAME}
	PRIVATE
	main.c
	events.c
	audio.c
	ecs.c
	assets.c
	basics.c
	mem.c
)

add_subdirectory(gfx)
add_subdirectory(physics)
add_subdirectory(ichigo)