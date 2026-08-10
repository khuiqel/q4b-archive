cmake_minimum_required(VERSION 3.20...3.31)
project(q4b)
set(PROJECT_GUI_NAME ${PROJECT_NAME}-gui)
set(PROJECT_TESTS_NAME ${PROJECT_NAME}-tests)

# Note: when changing these defaults, you have to delete CMakeCache.txt
option(Q4B_ENABLE_LZ4    "Enable LZ4 compression and decompression" ON)
option(Q4B_ENABLE_ZSTD   "Enable Zstd compression and decompression" ON)
option(Q4B_ENABLE_BROTLI "Enable Brotli compression and decompression" ON)
set(Q4B_SCHEME_LIBS)
option(Q4B_ADVANCED_TESTS "Enable advanced tests" OFF)

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

# Global CMake configuration
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Enable SSE4.2
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(COMPILER_MARCH_FLAGS "/arch:SSE4.2")
	# ARM: /arch:armv8.1 (8.0 is the default)
else()
	set(COMPILER_MARCH_FLAGS "-march=x86-64-v2")
	# ARM: -march=armv8.1-a
endif()

set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} ${COMPILER_MARCH_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_MARCH_FLAGS}")

# SDL configuration
set(SDL_AUDIO    OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO    ON  CACHE BOOL "" FORCE)
set(SDL_GPU      OFF CACHE BOOL "" FORCE) # Needed by ImGui (for GPU-accelerated backends)
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

add_subdirectory(Externals/SDL EXCLUDE_FROM_ALL)

if(Q4B_ENABLE_LZ4)
	# nothing to configure

	add_subdirectory(Externals/lz4/build/cmake EXCLUDE_FROM_ALL)
	list(APPEND Q4B_SCHEME_LIBS lz4_static)
	add_compile_definitions(Q4B_ENABLE_LZ4)
endif()

if(Q4B_ENABLE_ZSTD)
	set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
	set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE) # Don't need read support of old files
	set(ZSTD_MULTITHREAD ON CACHE BOOL "" FORCE) # TODO: Even if this is disabled, Zstd's CMake file still reports multithreading is enabled... should ZSTD_MULTITHREAD_SUPPORT be set instead?

	add_subdirectory(Externals/zstd/build/cmake EXCLUDE_FROM_ALL)
	list(APPEND Q4B_SCHEME_LIBS libzstd_static)
	add_compile_definitions(Q4B_ENABLE_ZSTD)
endif()

if(Q4B_ENABLE_BROTLI)
	set(BROTLI_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(BROTLI_BUILD_FOR_PACKAGE OFF CACHE BOOL "" FORCE)
	include_directories(PRIVATE ${BROTLI_INCLUDE_DIRS}) # Brotli doesn't add its includes automatically

	add_subdirectory(Externals/brotli EXCLUDE_FROM_ALL)
	list(APPEND Q4B_SCHEME_LIBS ${BROTLI_LIBRARIES})
	add_compile_definitions(Q4B_ENABLE_BROTLI)
endif()

# These don't work because the packages don't provide the CMake files to find their libs/dirs, or something:
#[[
find_package(SDL3 3.4.0 REQUIRED COMPONENTS video render OPTIONAL_COMPONENTS gpu joystick)
find_package(lz4 1.8.0 REQUIRED)
find_package(zstd 1.5.0 REQUIRED)
find_package(brotli 1.1.0 REQUIRED)
#]]

add_executable(${PROJECT_NAME}
	_source/q4b.cpp
	_source/q4b_helpers.cpp
	_source/compression_schemes.cpp
	_source/main_cli.cpp

	Externals/xxHash/xxhash.c
)

add_executable(${PROJECT_GUI_NAME}
	_source/q4b.cpp
	_source/q4b_helpers.cpp
	_source/compression_schemes.cpp
	_source/gui/compression_info.cpp
	_source/gui_data.cpp
	_source/main_gui.cpp

	Externals/xxHash/xxhash.c

	Externals/imgui/imgui.cpp
	Externals/imgui/imgui_demo.cpp
	Externals/imgui/imgui_draw.cpp
	Externals/imgui/imgui_tables.cpp
	Externals/imgui/imgui_widgets.cpp
	Externals/imgui/backends/imgui_impl_sdl3.cpp
	Externals/imgui/backends/imgui_impl_sdlrenderer3.cpp
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

target_include_directories(${PROJECT_NAME}     PRIVATE Externals/CLI11/include)
target_include_directories(${PROJECT_GUI_NAME} PRIVATE Externals/imgui Externals/imgui/backends)
include_directories(PRIVATE Externals/xxHash)

target_link_libraries(${PROJECT_NAME}     PRIVATE ${Q4B_SCHEME_LIBS})
target_link_libraries(${PROJECT_GUI_NAME} PRIVATE ${Q4B_SCHEME_LIBS})
target_link_libraries(${PROJECT_GUI_NAME} PRIVATE SDL3::SDL3)


# Tests!
enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(Externals/googletest EXCLUDE_FROM_ALL)

add_executable(${PROJECT_TESTS_NAME} EXCLUDE_FROM_ALL
	tests/archive.cpp

	_source/q4b.cpp
	_source/q4b_helpers.cpp
	_source/compression_schemes.cpp

	Externals/xxHash/xxhash.c
)

if (MSVC)
	set_target_properties(${PROJECT_TESTS_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

target_link_libraries(${PROJECT_TESTS_NAME} PRIVATE ${Q4B_SCHEME_LIBS})
target_link_libraries(${PROJECT_TESTS_NAME} PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_TESTS_NAME})
