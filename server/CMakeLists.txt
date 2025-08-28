add_executable(server
    server.cpp
    # GameMap is no longer needed for the 3D game
)

# We do not need find_package for the header-only GLM library

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../shared
    # We just need to tell it where to find the headers,
    # which is a standard system path.
    /usr/include
)

target_link_libraries(server PRIVATE
    Boost::system
    # The -lglm link is removed from here
)