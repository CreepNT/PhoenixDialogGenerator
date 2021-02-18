cmake_minimum_required(VERSION 2.8.11)
project(PhoenixDialogGenerator)
include_directories(include)
include_directories(include/imgui)

if(MSVC)
	# Without this, MSVC won't report the correct standards version via the
	# __cplusplus macro. See:
	#  https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
	set(CMAKE_CXX_FLAGS "/Zc:__cplusplus")
	add_definitions(-DWIN32)
endif()

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>)

link_directories(
	libs/
)

include_directories(
	include/
	libs/imgui
	libs/glfw/include/
)
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLAD=1)

add_executable(${PROJECT_NAME}
	src/main.cpp
	src/icon.h
	src/ogl_imgui.cpp
	PhoenixDialogGenerator.rc

libs/glad.c
libs/imgui/imgui.cpp
libs/imgui/imgui_demo.cpp
libs/imgui/imgui_draw.cpp
libs/imgui/imgui_tables.cpp
libs/imgui/imgui_widgets.cpp
libs/imgui/backends/imgui_impl_glfw.cpp
libs/imgui/backends/imgui_impl_opengl3.cpp
)

if (MSVC)
	#Only Windows has precompiled library
target_link_libraries(${PROJECT_NAME} glfw3)
else()
add_subdirectory(libs/glfw)
target_link_libraries(${PROJECT_NAME} glfw)
endif()