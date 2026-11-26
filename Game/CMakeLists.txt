file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS "include/*.hpp" "include/*.h")

add_executable(RebreakerReload ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories(RebreakerReload PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(RebreakerReload PUBLIC Engine Render Audio Core)
if(WIN32)
    target_link_libraries(RebreakerReload PUBLIC dbghelp)
endif()

function(add_auto_copy NAME SOURCE_DIR DEST_DIR)
    file(GLOB_RECURSE FILES_TO_WATCH "${SOURCE_DIR}/*")
    list(FILTER FILES_TO_WATCH EXCLUDE REGEX "levels/custom/.*")

    add_custom_target(${NAME}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}/levels/custom"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_DIR}/audio" "${DEST_DIR}/audio"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_DIR}/font" "${DEST_DIR}/font"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_DIR}/shaders" "${DEST_DIR}/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_DIR}/sprite" "${DEST_DIR}/sprite"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOURCE_DIR}/levels/level01.txt"
                "${SOURCE_DIR}/levels/level02.txt"
                "${SOURCE_DIR}/levels/level03.txt"
                "${SOURCE_DIR}/levels/level04.txt"
                "${SOURCE_DIR}/levels/level05.txt"
                "${SOURCE_DIR}/levels/level06.txt"
                "${SOURCE_DIR}/levels/level07.txt"
                "${SOURCE_DIR}/levels/preview_disco.txt"
                "${SOURCE_DIR}/levels/preview_fireball.txt"
                "${SOURCE_DIR}/levels/preview_ghost.txt"
                "${SOURCE_DIR}/levels/preview_plasma.txt"
                "${SOURCE_DIR}/levels/preview_poweruprain.txt"
                "${SOURCE_DIR}/levels/preview_storm.txt"
                "${DEST_DIR}/levels"
            DEPENDS ${FILES_TO_WATCH}
    )

    add_dependencies(RebreakerReload ${NAME})
endfunction()

add_auto_copy(copy_resources "${CMAKE_SOURCE_DIR}/Resources" "${CMAKE_BINARY_DIR}/bin/Resources")