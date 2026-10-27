cmake_minimum_required(VERSION 3.23)

project(space C CXX)

if (NOT EMSCRIPTEN)
	set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

file (GLOB_RECURSE SOURCES "Src/*.cpp" "Src/*.c" "Src/*.mm" "Src/*.inl")
file (GLOB_RECURSE HEADERS "Src/*.hpp" "Src/*.h" "Src/*.inl")

add_definitions(
	-D_USE_MATH_DEFINES
	-DASSET_BASE_DIR="."
    # -DFX_DEBUG_STATIC_ARRAY
)

if (APPLE AND NOT EMSCRIPTEN)
	add_compile_options(
		-O2
		# -fsanitize=address
	)

	add_link_options(
		# -fsanitize=address
	)
endif()

if(MSVC)
	add_compile_options("")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Src/ThirdParty
    ${CMAKE_SOURCE_DIR}/Src/ThirdParty/imgui
    ${CMAKE_SOURCE_DIR}/Src/ThirdParty/imgui/backends
)


if (EMSCRIPTEN)
	# Build raylib from source for the web
	include(FetchContent)

	set(PLATFORM "Web" CACHE STRING "" FORCE)
	set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
	set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

	FetchContent_Declare(
		raylib
		URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
	FetchContent_MakeAvailable(raylib)

	add_compile_options(-O2)
	add_definitions(-DGRAPHICS_API_OPENGL_ES3)
elseif(MSVC)
	link_directories(${CMAKE_SOURCE_DIR}/Lib/Windows)
else()
	link_directories(${CMAKE_SOURCE_DIR}/Lib)
endif()

add_executable(space ${SOURCES} ${HEADERS})

set (LIBS raylib )

if (MSVC)
    set (LIBS ${LIBS} winmm)

endif()

target_link_libraries(space ${LIBS})

# Web build
if (EMSCRIPTEN)
	set_target_properties(space PROPERTIES SUFFIX ".html")

	target_link_options(space PRIVATE
		-sUSE_GLFW=3
		-sMIN_WEBGL_VERSION=2
		-sMAX_WEBGL_VERSION=2
		-sALLOW_MEMORY_GROWTH=1
		-sINITIAL_MEMORY=256MB
		-sSTACK_SIZE=1MB
		"SHELL:--shell-file ${CMAKE_SOURCE_DIR}/Web/shell.html"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Earth.glb@Earth.glb"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Skybox.glb@Skybox.glb"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Sun.glb@Sun.glb"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/font.ttf@font.ttf"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Shaders@Shaders"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Datasets/2026-04-09.tle@Datasets/2026-04-09.tle"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Datasets/satcat.csv@Datasets/satcat.csv"
	)

	set_property(TARGET space APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/Web/shell.html)
endif()
