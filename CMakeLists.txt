cmake_minimum_required(VERSION 3.10)

project(PiCo)

set(CMAKE_C_COMPILER "gcc")

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(FATAL_ERROR "FATAL: This project requires the GCC compiler due to specific GNU C extensions. "
                        "The attempt to set GCC as the compiler failed. Please ensure 'gcc' is installed and in your PATH.")
endif()

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB SOURCE_DIR "*.c")
file(GLOB MODULE_SOURCES "modules/*.c")

if(NOT WIN32)
    add_library(linenoise_lib STATIC vendor/linenoise/linenoise.c)
    target_include_directories(linenoise_lib PUBLIC vendor/linenoise)
endif()

add_library(xxhash_lib STATIC vendor/xxhash/xxhash.c)
target_include_directories(xxhash_lib PUBLIC vendor/xxhash)

add_compile_options(-g)

add_executable(pico ${SOURCE_DIR} ${MODULE_SOURCES})

if(WIN32)
    target_link_libraries(pico PRIVATE xxhash_lib m)
else()
    target_link_libraries(pico PRIVATE xxhash_lib linenoise_lib m)
endif()

target_compile_features(pico PRIVATE c_std_11)
target_compile_options(pico PRIVATE
    $<$<CONFIG:RELEASE>:-O3>
)

message(STATUS "Project 'PiCo' configured to use C Compiler: ${CMAKE_C_COMPILER}")
