target_sources(${ENGINE_NAME} PRIVATE
	collision.c
	physics.c
)

if(NOT ${NO_JOLT})
	target_sources(${ENGINE_NAME} PRIVATE
		jolt.cpp
		body.cpp
		character.cpp
		raycast.cpp
	)
endif()
