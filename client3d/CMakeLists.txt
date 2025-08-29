# Set the build type to Debug
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Add the executable target for the 3D client
add_executable(client3d
    main.cpp
    NetworkClient.cpp
    ../vendor/glad/src/glad.c
    ../vendor/imgui/imgui.cpp
    ../vendor/imgui/imgui_draw.cpp
    ../vendor/imgui/imgui_tables.cpp
    ../vendor/imgui/imgui_widgets.cpp
    ../vendor/imgui/backends/imgui_impl_glfw.cpp
    ../vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# Tell CMake where to find header files
target_include_directories(client3d PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../vendor/glad/include
    ../vendor
    ../vendor/imgui
    ../vendor/imgui/backends
)

# Link against the necessary libraries
target_link_libraries(client3d PRIVATE
    ${OPENGL_LIBRARIES}
    glfw 
    Boost::system
    sfml-audio
)

# --- CORRECTED ASSET COPYING ---
# This single command correctly finds your assets, shaders, and models
# and copies them to the directory where your executable is built.
add_custom_command(
    TARGET client3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" #<-- Source
            "$<TARGET_FILE_DIR:client3d>/assets" #<-- Destination
    COMMAND ${CMAKE_COMMAND} -E copy 
            "${CMAKE_CURRENT_SOURCE_DIR}/1.model_loading.vs"
            "$<TARGET_FILE_DIR:client3d>"
    COMMAND ${CMAKE_COMMAND} -E copy 
            "${CMAKE_CURRENT_SOURCE_DIR}/1.model_loading.fs"
            "$<TARGET_FILE_DIR:client3d>"
    COMMAND ${CMAKE_COMMAND} -E copy 
            "${CMAKE_CURRENT_SOURCE_DIR}/debug.vs"
            "$<TARGET_FILE_DIR:client3d>"
    COMMAND ${CMAKE_COMMAND} -E copy 
            "${CMAKE_CURRENT_SOURCE_DIR}/debug.fs"
            "$<TARGET_FILE_DIR:client3d>"
    COMMENT "Copying assets and shaders to build directory..."
)