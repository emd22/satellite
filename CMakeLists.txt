cmake_minimum_required(VERSION 3.23)

project(space CXX)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

file (GLOB_RECURSE SOURCES "Src/*.cpp" "Src/*.c" "Src/*.mm" "Src/*.inl")
file (GLOB_RECURSE HEADERS "Src/*.hpp" "Src/*.h" "Src/*.inl")

add_definitions(
	-D_USE_MATH_DEFINES
	-DASSET_BASE_DIR="."
    # -DFX_DEBUG_STATIC_ARRAY
)

if (APPLE)
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
if(MSVC)
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
