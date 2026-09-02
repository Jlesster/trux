if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please use:\n  cmake -B build\nThen delete CMakeCache.txt and CMakeFiles/ from this directory.")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME}_lib PRIVATE -fno-sanitize=all)
    target_link_options(${PROJECT_NAME}_lib PRIVATE -fno-sanitize=all)
endif()

cmake_minimum_required(VERSION 3.14)
project(trux LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(/home/kenshii/.cmake/my-defaults.cmake OPTIONAL)

# --- Dependencies ---
include(FetchContent)

# --- Sources ---
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
message(STATUS "Sources found: ${SOURCES}")

add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})
target_include_directories(${PROJECT_NAME}_lib PUBLIC include)

# --- Executables ---
add_executable(target_api examples/target_api.cpp)
target_link_libraries(target_api PRIVATE ${PROJECT_NAME}_lib)
add_executable(keymap_example examples/keymap_example.cpp)
target_link_libraries(keymap_example PRIVATE ${PROJECT_NAME}_lib)
add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE ${PROJECT_NAME}_lib)

# --- Testing ---
enable_testing()
add_subdirectory(tests)
