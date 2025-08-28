add_executable(client
    main.cpp
    Game.cpp
    Tilemap.cpp
    NetworkClient.cpp
    ../vendor/imgui/imgui.cpp
    ../vendor/imgui/imgui_draw.cpp
    ../vendor/imgui/imgui_tables.cpp
    ../vendor/imgui/imgui_widgets.cpp
    ../vendor/imgui-sfml/imgui-SFML.cpp
)
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../shared
    ../vendor/imgui
    ../vendor/imgui-sfml
)
target_link_libraries(client PRIVATE
    Boost::system
    sfml-system
    sfml-window
    sfml-graphics
    OpenGL::GL
)
