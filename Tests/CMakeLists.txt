find_package(doctest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

add_executable(RebreakerTests ${TEST_SOURCES})

target_link_libraries(RebreakerTests PRIVATE
    doctest::doctest
    Engine
    Audio
    Engine
)


add_test(NAME UnitTests COMMAND RebreakerTests)
set_tests_properties(UnitTests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
