cmake_minimum_required(VERSION 3.31)
project(RebreakerReload)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Protect hardware & prevent memory exhaustion by capping Ninja build parallelism
set_property(GLOBAL PROPERTY JOB_POOLS compile_job_pool=4 link_job_pool=2)
set(CMAKE_JOB_POOL_COMPILE compile_job_pool)
set(CMAKE_JOB_POOL_LINK link_job_pool)

option(TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(MSVC)
    add_compile_options(/W4 /bigobj /utf-8)
    if(TREAT_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
    if(TREAT_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

find_package(SFML COMPONENTS Graphics Window System Audio Network CONFIG REQUIRED)

add_subdirectory(Core)
add_subdirectory(Render)
add_subdirectory(Audio)
add_subdirectory(Engine)
add_subdirectory(Game)

enable_testing()
add_subdirectory(Tests)
