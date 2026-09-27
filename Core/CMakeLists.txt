file(GLOB_RECURSE CORE_SOURCES CONFIG_DEPENDS "source/*.cpp")
file(GLOB_RECURSE CORE_HEADERS CONFIG_DEPENDS "include/*.hpp" "include/*.h" "include/*.inl")

add_library(Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(Core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Core PUBLIC
        SFML::Graphics
        SFML::Audio
        SFML::System
)
