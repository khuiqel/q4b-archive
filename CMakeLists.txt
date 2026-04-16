cmake_minimum_required(VERSION 3.20...3.31)
project(q4b)
set(PROJECT_GUI_NAME ${PROJECT_NAME}-gui)

# Note: when changing these defaults, you have to delete CMakeCache.txt
option(USE_SYSTEM_SDL "Use the system SDL3 package" OFF)
option(USE_SYSTEM_ZSTD "Use the system Zstd package" OFF)
option(USE_SYSTEM_LZ4 "Use the system LZ4 package" OFF)

set(CMAKE_CXX_STANDARD 23) # C++23 requires CMake 3.20
set(CMAKE_CXX_STANDARD_REQUIRED False)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT CMAKE_BUILD_TYPE)
	#options: Debug, Release, RelWithDebInfo, MinSizeRel
	set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# TODO
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# TODO
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	add_compile_options(/MP)
else()
	message(WARNING "Unknown compiler, assuming it's fine...")
endif()

# Enable SSE4.2
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(COMPILER_MARCH_FLAGS "/arch:SSE4.2")
else()
	set(COMPILER_MARCH_FLAGS "-march=x86-64-v2")
endif()

set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} ${COMPILER_MARCH_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_MARCH_FLAGS}")

# SDL configuration
set(SDL_AUDIO    OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO    ON  CACHE BOOL "" FORCE)
set(SDL_GPU      ON  CACHE BOOL "" FORCE) # Needed by ImGui
set(SDL_RENDER   ON  CACHE BOOL "" FORCE)
set(SDL_CAMERA   OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE) # Needed by ImGui if enabling gamepad navigation
set(SDL_HAPTIC   OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI   OFF CACHE BOOL "" FORCE)
set(SDL_POWER    OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR   OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG   OFF CACHE BOOL "" FORCE) # Could be useful
set(SDL_TRAY     OFF CACHE BOOL "" FORCE) # Could be useful
# more
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_UNIX_CONSOLE_BUILD OFF CACHE BOOL "" FORCE)

# Zstd configuration
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD ON CACHE BOOL "" FORCE) # Just in case

if (WIN32)
	find_package(Vulkan REQUIRED)
	include_directories(${Vulkan_INCLUDE_DIRS})
else()
	# TODO: does MSYS2 need this?
	find_package(Vulkan REQUIRED)
endif()
add_subdirectory(Externals/SDL EXCLUDE_FROM_ALL)
add_subdirectory(Externals/zstd/build/cmake)
add_subdirectory(Externals/lz4/build/cmake)

# if(USE_SYSTEM_SDL)
# 	find_package(SDL3 REQUIRED COMPONENTS video)
# else()
# 	# TODO
# 	find_package(SDL3 REQUIRED COMPONENTS video)
# 	# add_subdirectory(Externals/SDL)
# endif()

# if(USE_SYSTEM_ZSTD)
# 	find_package(zstd REQUIRED)
# else()
# 	add_subdirectory(Externals/zstd/build/cmake)
# endif()

include_directories(SDL     PRIVATE Externals/SDL/include)
include_directories(imgui   PRIVATE Externals/imgui Externals/imgui/backends)
include_directories(xxHash  PRIVATE Externals/xxHash)

add_executable(${PROJECT_NAME}
	_source/q4b.cpp
	_source/main_cli.cpp

	Externals/xxHash/xxhash.c
)

add_executable(${PROJECT_GUI_NAME}
	_source/q4b.cpp
	_source/gui_data.cpp
	_source/main_gui.cpp

	Externals/xxHash/xxhash.c

	Externals/imgui/imgui.cpp
	Externals/imgui/imgui_demo.cpp
	Externals/imgui/imgui_draw.cpp
	Externals/imgui/imgui_tables.cpp
	Externals/imgui/imgui_widgets.cpp
	Externals/imgui/backends/imgui_impl_sdl3.cpp
	Externals/imgui/backends/imgui_impl_vulkan.cpp
)

#TODO: is MSVC+Clang a special case?
if (MSVC)
	# Application icon:
	target_sources(${PROJECT_GUI_NAME} PRIVATE ${PROJECT_NAME}.rc)
	# Startup project:
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_GUI_NAME})
	# Set the running directory to the source root:
	set_target_properties(${PROJECT_NAME}     PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	set_target_properties(${PROJECT_GUI_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	# Disable the console window:
	# TODO?: set_target_properties(${PROJECT_GUI_NAME} PROPERTIES WIN32_EXECUTABLE ON)
	# target_link_options(${PROJECT_GUI_NAME} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

target_link_libraries(${PROJECT_NAME}     PRIVATE libzstd_static lz4_static)
target_link_libraries(${PROJECT_GUI_NAME} PRIVATE libzstd_static lz4_static)
target_link_libraries(${PROJECT_GUI_NAME} PRIVATE SDL3::SDL3 ${Vulkan_LIBRARIES})
