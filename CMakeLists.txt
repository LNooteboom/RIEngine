cmake_minimum_required (VERSION 3.13)
project (RIEngine C CXX)

# Variable definitions
set(RIEngine_VERSION_MAJOR 1)
set(RIEngine_VERSION_MINOR 0)

if (RELEASE)
	add_compile_definitions("RELEASE")
endif()

set(ENGINE_NAME riengine)
add_library(${ENGINE_NAME} STATIC)
target_include_directories(${ENGINE_NAME} PUBLIC include)

# Engine core
if (WINDOWS)
	file (COPY win/bin/SDL2.dll win/bin/SDL2_ttf.dll win/bin/SDL2_mixer.dll win/bin/libogg-0.dll win/bin/glew32.dll DESTINATION ${CMAKE_BINARY_DIR})

	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	set(CMAKE_SHARED_LIBRARY_PREFIX "")
	set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
	target_include_directories(${ENGINE_NAME} PUBLIC win/include)
	target_link_directories(${ENGINE_NAME} PUBLIC win/lib)
	
	SET(OPTIONS WIN32)
	set(CMAKE_C_FLAGS "/W3 /wd4244 /wd4200 /wd4201 /wd4152 /Fp:fast")
	
	set(RENDER_OPENGL OFF)
	set(RENDER_DX11 ON)
else()
	set(ENGINE_NAME ${OUTPUT_NAME})
	set(GAME_NAME ${OUTPUT_NAME})
	if (RELEASE)
		set(CMAKE_C_FLAGS "-Wall -Wextra -std=c99 -O2")
	else()
		set(CMAKE_C_FLAGS "-Wall -Wextra -std=c99 -Og -g")
	endif()

	target_link_libraries(${ENGINE_NAME}
		GLEW
		GL
		dl
		m
	)
	set(RENDER_OPENGL ON)
	set(RENDER_DX11 OFF)
endif()
target_link_libraries(${ENGINE_NAME}
	SDL2
	SDL2_mixer
	SDL2_ttf
)
target_link_libraries(${OUTPUT_NAME} ${ENGINE_NAME})

# Jolt
include (src/physics/jolt.cmake)
target_sources(${ENGINE_NAME} PRIVATE src/physics/jolt.cpp)
target_link_libraries(${ENGINE_NAME} Jolt)
add_compile_definitions("JOLT")

# Source files
add_subdirectory(src)

# Tools
add_executable(ichigo "tools/ichigo/ichigo.c" "tools/ichigo/lex.c" "tools/ichigo/parse.c" "tools/ichigo/semval.c" "tools/ichigo/instr.c")
add_executable(meshCreator "tools/meshCreator.c")
target_include_directories(meshCreator PUBLIC include)
if(WINDOWS)
else()
target_link_libraries(ichigo m)
target_link_libraries(meshCreator m)
endif()

# HLSL
set(FXC_VS fxc /nologo /Vi /T vs_4_0)
set(FXC_PS fxc /nologo /Vi /T ps_4_0)
set(HLSL_IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl")
set(HLSL_OUT_DIR "${CMAKE_BINARY_DIR}/dat/shaders")

add_custom_command(OUTPUT "${HLSL_OUT_DIR}/std_vs.cso" COMMAND ${FXC_VS} /E stdvs_main /Fo ${HLSL_OUT_DIR}/std_vs.cso ${HLSL_IN_DIR}/vertex.hlsl DEPENDS "${HLSL_IN_DIR}/vertex.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/anim_vs.cso" COMMAND ${FXC_VS} /E stdvs_anim_main /Fo ${HLSL_OUT_DIR}/anim_vs.cso ${HLSL_IN_DIR}/vertex.hlsl DEPENDS "${HLSL_IN_DIR}/vertex.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/distort_vs.cso" COMMAND ${FXC_VS} /E distort_main /Fo ${HLSL_OUT_DIR}/distort_vs.cso ${HLSL_IN_DIR}/vertex.hlsl DEPENDS "${HLSL_IN_DIR}/vertex.hlsl")

add_custom_command(OUTPUT "${HLSL_OUT_DIR}/std_ps_2d.cso" COMMAND ${FXC_PS} /E std2d_main /Fo ${HLSL_OUT_DIR}/std_ps_2d.cso ${HLSL_IN_DIR}/pixel.hlsl DEPENDS "${HLSL_IN_DIR}/pixel.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/std_ps_3d.cso" COMMAND ${FXC_PS} /E std3d_main /Fo ${HLSL_OUT_DIR}/std_ps_3d.cso ${HLSL_IN_DIR}/pixel.hlsl DEPENDS "${HLSL_IN_DIR}/pixel.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/std_ps_3d_clip.cso" COMMAND ${FXC_PS} /E std3d_clip_main /Fo ${HLSL_OUT_DIR}/std_ps_3d_clip.cso ${HLSL_IN_DIR}/pixel.hlsl DEPENDS "${HLSL_IN_DIR}/pixel.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/std_ps_post_3d.cso" COMMAND ${FXC_PS} /E post3d_main /Fo ${HLSL_OUT_DIR}/std_ps_post_3d.cso ${HLSL_IN_DIR}/pixel.hlsl DEPENDS "${HLSL_IN_DIR}/pixel.hlsl")
add_custom_command(OUTPUT "${HLSL_OUT_DIR}/water_ps.cso" COMMAND ${FXC_PS} /E water_ps /Fo ${HLSL_OUT_DIR}/water_ps.cso ${HLSL_IN_DIR}/pixel.hlsl DEPENDS "${HLSL_IN_DIR}/pixel.hlsl")
add_custom_target(hlsl_shaders ALL DEPENDS
	"${HLSL_OUT_DIR}/std_vs.cso"
	"${HLSL_OUT_DIR}/anim_vs.cso"
	"${HLSL_OUT_DIR}/distort_vs.cso"
	"${HLSL_OUT_DIR}/std_ps_2d.cso"
	"${HLSL_OUT_DIR}/std_ps_3d.cso"
	"${HLSL_OUT_DIR}/std_ps_3d_clip.cso"
	"${HLSL_OUT_DIR}/std_ps_post_3d.cso"
	"${HLSL_OUT_DIR}/water_ps.cso"
)