cmake_minimum_required(VERSION 3.21)

project(PiCo VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(FATAL_ERROR
        "PiCo requires GCC because it uses GNU C extensions. "
        "Configure CMake with a GCC-based toolchain or preset."
    )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS "core/*.c")
file(GLOB_RECURSE VM_SRC CONFIGURE_DEPENDS "vm/*.c")
file(GLOB_RECURSE COMPILER_SRC CONFIGURE_DEPENDS "compiler/*.c")
file(GLOB_RECURSE STD_SRC CONFIGURE_DEPENDS "std/*.c")
file(GLOB_RECURSE CMD_SRC CONFIGURE_DEPENDS "cmd/*.c")

set(PICO_INTERNAL_INCLUDE_DIRS
    cmd
    core
    vm
    compiler
    std
    vendor/xxhash
)

set(PICO_API_SRC
    api/pico.c
)

set(PICO_VENDOR_SRC
    vendor/xxhash/xxhash.c
)

set(PICO_RUNTIME_SRC
    ${PICO_API_SRC}
    ${PICO_VENDOR_SRC}
    ${CORE_SRC}
    ${VM_SRC}
    ${COMPILER_SRC}
    ${STD_SRC}
)

add_compile_options(
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Release>:-O3>
)

add_library(picort STATIC
    ${PICO_RUNTIME_SRC}
)

target_include_directories(picort
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${PICO_INTERNAL_INCLUDE_DIRS}
)

set_target_properties(picort PROPERTIES
    OUTPUT_NAME pico
)

target_link_libraries(picort
    PRIVATE
        m
)

add_executable(pico
    ${CMD_SRC}
)

target_include_directories(pico
    PRIVATE
        ${PICO_INTERNAL_INCLUDE_DIRS}
)

target_link_libraries(pico
    PRIVATE
        picort
)

if(NOT WIN32)
    add_library(linenoise_lib STATIC
        vendor/linenoise/linenoise.c
    )

    target_include_directories(linenoise_lib
        PUBLIC
            vendor/linenoise
    )

    target_compile_definitions(pico
        PRIVATE
            USE_LINENOISE
    )

    target_link_libraries(pico
        PRIVATE
            linenoise_lib
    )
endif()

add_executable(pico_embed_basic
    examples/embedding/basic.c
)

target_link_libraries(pico_embed_basic
    PRIVATE
        picort
)

add_executable(pico_embed_exit_policy
    tests/embedding_exit_policy.c
)

target_link_libraries(pico_embed_exit_policy
    PRIVATE
        picort
)

add_executable(pico_embed_native_function
    tests/embedding_native_function.c
)

target_link_libraries(pico_embed_native_function
    PRIVATE
        picort
)

add_executable(pico_embed_output_callback
    tests/embedding_output_callback.c
)

target_link_libraries(pico_embed_output_callback
    PRIVATE
        picort
)

add_executable(pico_embed_call_script
    tests/embedding_call_script.c
)

target_link_libraries(pico_embed_call_script
    PRIVATE
        picort
)

include(CTest)

if(BUILD_TESTING)
    if(WIN32)
        find_program(
            PICO_BASH_EXECUTABLE
            NAMES bash.exe
            PATHS
                "$ENV{ProgramFiles}/Git/bin"
                "$ENV{ProgramFiles}/Git/usr/bin"
                "C:/Program Files/Git/bin"
                "C:/Program Files/Git/usr/bin"
                "C:/msys64/usr/bin"
            NO_DEFAULT_PATH
        )
    else()
        find_program(
            PICO_BASH_EXECUTABLE
            NAMES bash
        )
    endif()

    if(NOT PICO_BASH_EXECUTABLE)
        message(FATAL_ERROR
            "Bash was not found. On Windows, install Git for Windows or MSYS2."
        )
    endif()

    add_test(
        NAME pico_script_tests
        COMMAND
            "${PICO_BASH_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/run_tests.sh"
            "$<TARGET_FILE:pico>"
            "60"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests"
    )

    add_test(
        NAME pico_embedding_basic
        COMMAND $<TARGET_FILE:pico_embed_basic>
    )

    add_test(
        NAME pico_embedding_exit_policy
        COMMAND $<TARGET_FILE:pico_embed_exit_policy>
    )

    add_test(
        NAME pico_embedding_native_function
        COMMAND $<TARGET_FILE:pico_embed_native_function>
    )

    add_test(
        NAME pico_embedding_call_script
        COMMAND $<TARGET_FILE:pico_embed_call_script>
    )

    add_test(
        NAME pico_embedding_output_callback
        COMMAND $<TARGET_FILE:pico_embed_output_callback>
    )
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        pico
        pico_embed_basic
        pico_embed_exit_policy
        pico_embed_native_function
        pico_embed_call_script
        pico_embed_output_callback
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS pico picort
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/pico.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES README.md manual.md LICENSE gpl-3.0.txt
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(DIRECTORY examples/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
    FILES_MATCHING
        PATTERN "*.pcs"
        PATTERN "*.c"
        PATTERN "README.md"
)

message(STATUS "Project 'PiCo' configured to use C Compiler: ${CMAKE_C_COMPILER}")