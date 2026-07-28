add_executable(cell_buffer_test
    cell_buffer_test.cpp
)

target_link_libraries(cell_buffer_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME CellBuffer
    COMMAND cell_buffer_test
)

add_executable(split_pane_test
    split_pane_test.cpp
)

target_link_libraries(split_pane_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME SplitPane
    COMMAND split_pane_test
)

add_executable(render_test
    render_test.cpp
)

target_link_libraries(render_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME Render
    COMMAND render_test
)

add_executable(region_test
    region_test.cpp
)

target_link_libraries(region_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME Region
    COMMAND region_test
)

add_executable(input_test
    input_test.cpp
)

target_link_libraries(input_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME Input
    COMMAND input_test
)
