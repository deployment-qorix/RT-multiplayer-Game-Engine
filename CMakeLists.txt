# --- Main CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)
project(GameEngine)

# Copy the assets folder to the build folder
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find All Required Libraries ---
find_package(Boost REQUIRED COMPONENTS system)
find_package(SFML 2.6 COMPONENTS system window graphics REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# --- Add All Game Subdirectories ---
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(client3d)