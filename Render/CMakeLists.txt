file(GLOB_RECURSE RENDER_SOURCES CONFIG_DEPENDS "source/*.cpp")
file(GLOB_RECURSE RENDER_HEADERS CONFIG_DEPENDS "include/*.hpp")

add_library(Render STATIC ${RENDER_SOURCES} ${RENDER_HEADERS})

target_include_directories(Render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Render PUBLIC
        Engine
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
        SFML::Network
)