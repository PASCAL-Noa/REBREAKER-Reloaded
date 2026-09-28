file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "include/*.hpp" "include/*.h")

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Engine PUBLIC Core Render Audio)