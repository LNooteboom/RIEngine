target_sources(${ENGINE_NAME} PRIVATE
	texture.c
	drawvm.c
	draw.cpp
	anim.c
	ttf.c
	frustum.cpp
)
if (RENDER_OPENGL)
target_sources(${ENGINE_NAME} PRIVATE
	opengl.cpp
)
elseif (RENDER_DX11)
target_sources(${ENGINE_NAME} PRIVATE
	dx11.cpp
)
endif()
