add_executable(client3D
    main.cpp
    ../vendor/glad/src/glad.c
)

find_package(glm REQUIRED)

target_include_directories(client3D PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../vendor/glad/include
    /usr/include # For GLM
    ../vendor # For stb_image and tinyobjloader
)

find_package(glfw3 3.3 REQUIRED)

target_link_libraries(client3D PRIVATE
    glfw
    OpenGL::GL
)