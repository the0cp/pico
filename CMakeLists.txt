cmake_minimum_required(VERSION 3.10)

project(PiCo)

set(CMAKE_C_COMPILER "gcc")

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(FATAL_ERROR "FATAL: This project requires the GCC compiler due to specific GNU C extensions. "
                        "The attempt to set GCC as the compiler failed. Please ensure 'gcc' is installed and in your PATH.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

file(GLOB_RECURSE CORE_SRC "core/*.c")
file(GLOB_RECURSE VM_SRC "vm/*.c")
file(GLOB_RECURSE COMPILER_SRC "compiler/*.c")
file(GLOB_RECURSE STD_SRC "std/*.c")
file(GLOB_RECURSE CMD_SRC "cmd/*.c")

include_directories(
    cmd
    core
    vm
    compiler
    std
    std/fs
    std/sys
    std/text
    std/container
    vendor/xxhash
)

add_library(xxhash_lib STATIC vendor/xxhash/xxhash.c)
target_include_directories(xxhash_lib PUBLIC vendor/xxhash)

if(NOT WIN32)
    add_library(linenoise_lib STATIC vendor/linenoise/linenoise.c)
    target_include_directories(linenoise_lib PUBLIC vendor/linenoise)
    add_definitions(-DUSE_LINENOISE) 
else()
endif()

add_compile_options(-g)

add_executable(pico 
    ${CORE_SRC} 
    ${VM_SRC} 
    ${COMPILER_SRC} 
    ${STD_SRC} 
    ${CMD_SRC}
)

if(WIN32)
    target_link_libraries(pico PRIVATE xxhash_lib m)
else()
    target_link_libraries(pico PRIVATE xxhash_lib linenoise_lib m)
endif()

target_compile_options(pico PRIVATE
    $<$<CONFIG:RELEASE>:-O3>
)

message(STATUS "Project 'PiCo' configured to use C Compiler: ${CMAKE_C_COMPILER}")
