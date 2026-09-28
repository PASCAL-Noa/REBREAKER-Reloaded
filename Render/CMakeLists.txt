file(GLOB_RECURSE RENDER_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE RENDER_HEADERS CONFIGURE_DEPENDS "include/*.hpp" "include/*.h")

add_library(Render STATIC ${RENDER_SOURCES} ${RENDER_HEADERS})

target_include_directories(Render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(Render PRIVATE "${CMAKE_SOURCE_DIR}/Engine/include")

target_link_libraries(Render PUBLIC
        Core
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
        SFML::Network
)