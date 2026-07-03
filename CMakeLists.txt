cmake_minimum_required(VERSION 3.21)

project(PiCo VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

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
file(GLOB_RECURSE STD_SRC CONFIGURE_DEPENDS "std/*.c")
file(GLOB_RECURSE CMD_SRC "cmd/*.c")

include_directories(
    cmd
    core
    vm
    compiler
    std
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

add_compile_options(
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Release>:-O3>
)

set(PICO_API_SRC
    api/pico.c
)

set(PICO_RUNTIME_SRC
    ${PICO_API_SRC}
    ${CORE_SRC}
    ${VM_SRC}
    ${COMPILER_SRC}
    ${STD_SRC}
)

add_library(picort STATIC
    ${PICO_RUNTIME_SRC}
)

target_include_directories(picort
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(picort PROPERTIES
    OUTPUT_NAME pico
)

target_link_libraries(picort
    PRIVATE
        xxhash_lib
        m
)

add_executable(pico
    ${CMD_SRC}
)

add_executable(pico_embed_basic
    examples/embedding/basic.c
)

target_link_libraries(pico_embed_basic
    PRIVATE
        picort
)

target_link_libraries(pico
    PRIVATE
        picort
)

if(NOT WIN32)
    target_link_libraries(pico
        PRIVATE
            linenoise_lib
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(
        NAME pico_script_tests
        COMMAND ${CMAKE_COMMAND} -E env
                "PICO_EXEC=$<TARGET_FILE:pico>"
                "TIMEOUT_SEC=60"
                bash run_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    )

    add_test(
        NAME pico_embedding_basic
        COMMAND $<TARGET_FILE:pico_embed_basic>
    )
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS pico pico_embed_basic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS pico
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES README.md manual.md LICENSE gpl-3.0.txt
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(DIRECTORY examples/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
    FILES_MATCHING PATTERN "*.pcs"
)

message(STATUS "Project 'PiCo' configured to use C Compiler: ${CMAKE_C_COMPILER}")
