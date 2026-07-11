cmake_minimum_required(VERSION 3.21)

project(pico_embed_app LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(PICO_ROOT "" CACHE PATH "Path to the PiCo install prefix")

if(PICO_ROOT STREQUAL "")
    message(FATAL_ERROR "Set PICO_ROOT to the PiCo install prefix, for example -DPICO_ROOT=/path/to/install")
endif()

add_library(pico STATIC IMPORTED)

set_target_properties(pico PROPERTIES
    IMPORTED_LOCATION "${PICO_ROOT}/${CMAKE_INSTALL_LIBDIR}/libpico.a"
    INTERFACE_INCLUDE_DIRECTORIES "${PICO_ROOT}/${CMAKE_INSTALL_INCLUDEDIR}"
)

add_executable(embed_app main.c)
target_link_libraries(embed_app PRIVATE pico)

if(UNIX OR MINGW)
    target_link_libraries(embed_app PRIVATE m)
endif()
