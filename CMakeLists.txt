cmake_minimum_required(VERSION 3.16)
project(GameEngine LANGUAGES C CXX)   # ✅ Add C here

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(SFML 2.5 COMPONENTS audio REQUIRED)

# GLAD
add_library(glad STATIC vendor/glad/src/glad.c)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
target_include_directories(glad PUBLIC vendor/glad/include)

# ImGui
set(IMGUI_DIR vendor/imgui)
add_library(imgui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# ======================
# Client (client3d/)
# ======================
set(CLIENT_SRC
    client3d/main.cpp
    client3d/NetworkClient.cpp
)

add_executable(client ${CLIENT_SRC})

target_include_directories(client PUBLIC
    client3d
    shared
    vendor
)

target_link_libraries(client
    glad
    imgui
    glfw
    ${OPENGL_gl_LIBRARY}
    ${CMAKE_DL_LIBS}
    Boost::system
    Threads::Threads
    sfml-audio
)

# ======================
# Server (server/)
# ======================
set(SERVER_SRC
    server/server.cpp
    server/GameMap.cpp
)

add_executable(server ${SERVER_SRC})

target_include_directories(server PUBLIC
    server
    shared
    vendor
)

target_link_libraries(server
    Boost::system
    Threads::Threads
)

