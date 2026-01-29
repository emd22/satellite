cmake_minimum_required(VERSION 3.23)

project(space CXX)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

file (GLOB_RECURSE SOURCES "Src/*.cpp" "Src/*.inl")
file (GLOB_RECURSE HEADERS "Src/*.hpp" "Src/*.h" "Src/*.inl")

add_definitions(
    -DVMA_RECORDING_ENABLED=1
    -DVMA_DEBUG_ALWAYS_DETECT_LEAKS=1

    -DVK_ENABLE_BETA_EXTENSIONS=1   # Used by PortabilitySubsetFeaturesKHR for macOS; From around 2020, but still under BETA_EXTENSIONS per vulkan.h.

	-D_USE_MATH_DEFINES
	-DFX_BASE_DIR="${CMAKE_SOURCE_DIR}"
    # -DFX_DEBUG_STATIC_ARRAY
)

if (APPLE)
	add_compile_options(
		-g
		# -fsanitize=address
	)

	add_link_options(
		# -fsanitize=address
	)
endif()

if(MSVC)
	add_compile_options("/MP" "/Oi" "/arch:AVX2")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/Src
)

link_directories(${CMAKE_SOURCE_DIR}/Lib)

add_executable(space ${SOURCES} ${HEADERS})

target_link_libraries(space raylib)
