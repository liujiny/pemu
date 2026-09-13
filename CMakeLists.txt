cmake_minimum_required(VERSION 3.10)
#set(CMAKE_VERBOSE_MAKEFILE ON)

SET(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
SET(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG "@")

####################
# BUILD OPTIONS
####################
set(OPTION_EMU OFF CACHE STRING "Emulator to build (pfbneo, pnes, psnes)")
set(OPTION_BUILTIN_LIBCONFIG OFF CACHE BOOL "Build libconfig from sources")
set(OPTION_MPV_PLAYER ON CACHE BOOL "Add support for video snaps (mpv)")
set(OPTION_UPDATE_CONFIG OFF CACHE BOOL "Force config file updating")
option(PS5_GPU_OPENGL "Use ps5-opengl Mesa/Gallium backend on PS5" ON)
option(PS5_NATIVE_APP "Build the PS5 native folder-app variant" OFF)
set(PS5_OPENGL_PREFIX "" CACHE PATH "ps5-opengl SDK prefix")
set(PS5_SDL_PREFIX "" CACHE PATH "PS5 SDL2 bridge prefix")
set(PS5_NATIVE_APP_TEMPLATE "" CACHE PATH "Pinned ps5-native-app-boilerplate checkout")
set(PS5_NATIVE_TITLE_ID "PPSA99998" CACHE STRING "Development title ID for the PS5 native app")

if (PLATFORM_PS5 AND PS5_NATIVE_APP)
    if (NOT PS5_NATIVE_APP_TEMPLATE OR NOT EXISTS "${PS5_NATIVE_APP_TEMPLATE}")
        message(FATAL_ERROR "PS5_NATIVE_APP_TEMPLATE must point to the pinned native-app boilerplate checkout")
    endif ()
    if (NOT PS5_NATIVE_TITLE_ID MATCHES "^PPSA[0-9A-Z][0-9A-Z][0-9A-Z][0-9A-Z][0-9A-Z]$")
        message(FATAL_ERROR "PS5_NATIVE_TITLE_ID must have the PPSAxxxxx form")
    endif ()
    set(PS5_NATIVE_APP_TEMPLATE "${PS5_NATIVE_APP_TEMPLATE}" CACHE PATH "Pinned ps5-native-app-boilerplate checkout" FORCE)
    message(STATUS "[PS5 NATIVE] template=${PS5_NATIVE_APP_TEMPLATE}")
    message(STATUS "[PS5 NATIVE] title_id=${PS5_NATIVE_TITLE_ID}")
    set(OPTION_BUILTIN_LIBCONFIG ON CACHE BOOL "Build libconfig from sources" FORCE)
endif ()

if (PLATFORM_PS5 AND PS5_GPU_OPENGL)
    set(C2D_PS5_MESA_GL ON CACHE BOOL "Use the ps5-opengl GL 3.3 path" FORCE)
endif ()

# add libcross2d library. If you want to port pfba to another platform,
# you should (may) only need to port libcross2d library.
add_subdirectory(external/libcross2d)
####################
# BUILD OPTIONS
####################

# linux/windows screen handling options
if (PLATFORM_LINUX OR PLATFORM_WINDOWS)
    set(OPTION_RESOLUTION ON CACHE BOOL "Add screen resolution option")
    set(OPTION_FULLSCREEN ON CACHE BOOL "Add switch to fullscreen option")
endif ()

# add sscrap (screenscrapper scrapper!) library
add_subdirectory(external/sscrap)

# required for cmake toolchain setup
project(pemu)
set(PROJECT_AUTHOR cpasjuste)
set(VERSION_MAJOR 7)
set(VERSION_MINOR 2)

# pemu (ui)
add_subdirectory(src/skeleton)

# cores
add_subdirectory(src/cores/pfbneo)
if (NOT (PLATFORM_PS5 AND PS5_NATIVE_APP))
    add_subdirectory(src/cores/pgen)
    add_subdirectory(src/cores/pnes)
    add_subdirectory(src/cores/psnes)
    add_subdirectory(src/cores/pgba)
endif ()
