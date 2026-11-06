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

add_executable(widget_test
    widget_test.cpp
)

target_link_libraries(widget_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME Widget
    COMMAND widget_test
)

add_executable(focus_test
    focus_test.cpp
)

target_link_libraries(focus_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME focus
    COMMAND focus_test
)

add_executable(event_parser_test
    event_parser_test.cpp
)

target_link_libraries(event_parser_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME event_parser
    COMMAND event_parser_test
)

add_executable(async_test
    async_test.cpp
)

target_link_libraries(async_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME async
    COMMAND async_test
)

add_executable(piece_table_test
    piece_table_test.cpp
)

target_link_libraries(piece_table_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME PieceTable
    COMMAND piece_table_test
)

add_executable(text_area_test
    text_area_test.cpp
)

target_link_libraries(text_area_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME TextArea
    COMMAND text_area_test
)

add_executable(color_test
    color_test.cpp
)

target_link_libraries(color_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME Color
    COMMAND color_test
)


add_executable(component_color_test
    component_color_test.cpp
)

target_link_libraries(component_color_test
    PRIVATE
    ${PROJECT_NAME}_lib
)

add_test(
    NAME ComponentColor
    COMMAND component_color_test
)
