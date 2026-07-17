cmake_minimum_required(VERSION 3.21)

project(cieto_embed_app LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CIETO_ROOT "" CACHE PATH "Path to the Cieto install prefix")

if(CIETO_ROOT STREQUAL "")
    message(FATAL_ERROR "Set CIETO_ROOT to the Cieto install prefix, for example -DCIETO_ROOT=/path/to/install")
endif()

add_library(cieto STATIC IMPORTED)

set_target_properties(cieto PROPERTIES
    IMPORTED_LOCATION "${CIETO_ROOT}/${CMAKE_INSTALL_LIBDIR}/libcieto.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CIETO_ROOT}/${CMAKE_INSTALL_INCLUDEDIR}"
)

add_executable(embed_app main.c)
target_link_libraries(embed_app PRIVATE cieto)

if(UNIX OR MINGW)
    target_link_libraries(embed_app PRIVATE m)
endif()
