if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please use:\n  cmake -B build\nThen delete CMakeCache.txt and CMakeFiles/ from this directory.")
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

# Force-strip sanitizers on Release builds, regardless of what
# my-defaults.cmake (or anything else) injected above.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME}_lib PRIVATE -fno-sanitize=all)
    target_link_options(${PROJECT_NAME}_lib PRIVATE -fno-sanitize=all)
endif()

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

# --- Documentation ---
include(GNUInstallDirs)

option(TRUX_BUILD_DOCS "Generate and install Doxygen API documentation" ON)

if(TRUX_BUILD_DOCS)
    find_package(Doxygen OPTIONAL_COMPONENTS dot)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_PROJECT_NAME ${PROJECT_NAME})
        set(DOXYGEN_PROJECT_NUMBER ${PROJECT_VERSION})
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${CMAKE_SOURCE_DIR}/README.md)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)

        # Builds into ${DOXYGEN_OUTPUT_DIRECTORY}/html; not part of the
        # default `all` target, so `make ${PROJECT_NAME}_docs` builds it
        # standalone, and `make install` builds + installs it below.
        doxygen_add_docs(
            ${PROJECT_NAME}_docs
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/README.md
            ${CMAKE_SOURCE_DIR}/TODO.md
            ${CMAKE_SOURCE_DIR}/docs/TRUX_ARCHITECTURE.md
            COMMENT "Generating Doxygen documentation for ${PROJECT_NAME}"
        )

        # `install` doesn't build targets on its own, so explicitly
        # build the docs target first, then install what it produced.
        install(
            CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target ${PROJECT_NAME}_docs)"
            COMPONENT docs
        )
        install(
            DIRECTORY ${DOXYGEN_OUTPUT_DIRECTORY}/html/
            DESTINATION ${CMAKE_INSTALL_DOCDIR}/html
            COMPONENT docs
        )
    else()
        message(STATUS "Doxygen not found: `make install` will skip API documentation. Install doxygen (and optionally graphviz) to enable it.")
    endif()
endif()
