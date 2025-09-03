set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(client3d
    main.cpp
    NetworkClient.cpp # This line includes the missing file
    ../vendor/glad/src/glad.c
    ../vendor/imgui/imgui.cpp
    ../vendor/imgui/imgui_draw.cpp
    ../vendor/imgui/imgui_tables.cpp
    ../vendor/imgui/imgui_widgets.cpp
    ../vendor/imgui/backends/imgui_impl_glfw.cpp
    ../vendor/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(client3d PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    include 
    ../vendor/glad/include 
    ../vendor 
    ../vendor/imgui 
    ../vendor/imgui/backends
    ../shared
)

target_link_libraries(client3d PRIVATE 
    ${OPENGL_LIBRARIES} 
    glfw 
    Boost::system 
    sfml-audio
)

add_custom_command(TARGET client3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:client3d>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:client3d>/shaders"
    COMMENT "Copying assets and shaders..."
)