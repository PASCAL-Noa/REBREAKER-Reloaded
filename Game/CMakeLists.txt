file(GLOB_RECURSE GAME_SOURCES CONFIG_DEPENDS "source/*.cpp")
file(GLOB_RECURSE GAME_HEADERS CONFIG_DEPENDS "include/*.hpp" "include/*.h")

add_executable(RebreakerReload ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories(RebreakerReload PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(RebreakerReload PUBLIC Engine Render Audio)

function(add_auto_copy NAME SOURCE_DIR DEST_DIR)
    file(GLOB_RECURSE FILES_TO_WATCH "${SOURCE_DIR}/*")

    add_custom_target(${NAME}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_DIR} ${DEST_DIR}
            DEPENDS ${FILES_TO_WATCH}
    )

    add_dependencies(RebreakerReload ${NAME})
endfunction()

add_auto_copy(copy_resources "${CMAKE_SOURCE_DIR}/Resources" "${CMAKE_BINARY_DIR}/bin/Resources")