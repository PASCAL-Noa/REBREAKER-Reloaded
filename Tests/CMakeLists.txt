find_package(doctest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

add_executable(RebreakerTests 
    ${TEST_SOURCES} 
    "${CMAKE_SOURCE_DIR}/Game/source/RogueliteDraft.cpp"
    "${CMAKE_SOURCE_DIR}/Game/source/ProceduralEndlessGenerator.cpp"
    "${CMAKE_SOURCE_DIR}/Game/source/BrickFactory.cpp"
    "${CMAKE_SOURCE_DIR}/Game/source/TetrisMatrix.cpp"
)

target_include_directories(RebreakerTests PRIVATE "${CMAKE_SOURCE_DIR}/Game/include")

target_link_libraries(RebreakerTests PRIVATE
    doctest::doctest
    Engine
    Audio
    Core
)

add_test(NAME UnitTests COMMAND RebreakerTests)
set_tests_properties(UnitTests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
