file(GLOB_RECURSE AUDIO_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE AUDIO_HEADERS CONFIGURE_DEPENDS "include/*.hpp" "include/*.h")

add_library(Audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS})

target_include_directories(Audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Audio PUBLIC
        Core
        SFML::Audio
        SFML::System
)