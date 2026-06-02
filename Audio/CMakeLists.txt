file(GLOB_RECURSE AUDIO_SOURCES CONFIG_DEPENDS "source/*.cpp")
file(GLOB_RECURSE AUDIO_HEADERS CONFIG_DEPENDS "include/*.hpp" "include/*.h")

add_library(Audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS})

target_include_directories(Audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Audio PUBLIC
        Render
        SFML::Audio
        SFML::System
)