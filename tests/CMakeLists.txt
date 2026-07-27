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
