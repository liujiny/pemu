cmake_minimum_required(VERSION 3.10)

project(pfbneo)

option(OPTION_LIGHT "Disable consoles and some arcade drivers" OFF)

# handle m68k
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/deps)
ADD_CUSTOM_COMMAND(OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/deps/m68kops.h ${CMAKE_CURRENT_BINARY_DIR}/deps/m68kops.c
        COMMAND gcc ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/cpu/m68k/m68kmake.c -o ${CMAKE_CURRENT_BINARY_DIR}/deps/m68kmake
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/deps/m68kmake ${CMAKE_CURRENT_BINARY_DIR}/deps/ ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/cpu/m68k/m68k_in.c)

##############
# burn & devices & snd
##############
set(BURN_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/devices
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/snd
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/dr_libs
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/stb_vorbis
)
file(GLOB SRC_BURN
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/devices/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/snd/*.c*
)

##############
# burner
##############
set(BURNER_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl
)
file(GLOB SRC_BURNER
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/neocdlist.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/stringset.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/ips_manager.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/romdata.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/localise_gamelist.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sdl/replay.cpp
)
list(REMOVE_ITEM SRC_BURNER
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/conc.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/cong.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/dat.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/gamc.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/gami.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/image.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/ioapi.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/sshot.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/unzip.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/zipfn.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/luaengine.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burner/luasav.cpp
)

##############
# drivers (全量包含所有街机与家用机驱动)
##############
set(DRV_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/atari
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/capcom
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/cave
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/cps3
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/dataeast
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/galaxian
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/irem
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/konami
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/midway
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pst90s
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pre90s
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/neogeo
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pgm
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pgm2
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/psikyo
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/sega
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/taito
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/toaplan
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/channelf
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/coleco
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/megadrive
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pce
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/nes
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/sg1000
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/sms
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/msx
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/spectrum
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/gba
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/snes
)

foreach (DIR ${DRV_DIRS})
    file(GLOB DRV ${DIR}/*.cpp ${DIR}/*.c)
    list(APPEND SRC_DRV ${DRV})
endforeach ()

list(REMOVE_ITEM SRC_DRV
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/capcom/ctv_make.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pgm/pgm_sprite_create.cpp
)

##############
# interfaces
##############
set(INTF_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/input
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/cd
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/cd/sdl
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/audio
)
file(GLOB SRC_INTF
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/*.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/cd/*.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/cd/sdl/*.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/audio/*.cpp
)

# The pemu frontend supplies its own video path on Vita.  Keep FBNeo's
# desktop/SDL video frontend for the other platforms that already used it.
if (NOT PLATFORM_VITA)
    file(GLOB SRC_INTF_VIDEO
            ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/video/vid_interface.cpp
            ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/video/vid_support.cpp
            ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/video/vid_softfx.cpp
            ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/video/scalers/*.cpp
    )
    list(APPEND SRC_INTF ${SRC_INTF_VIDEO})
endif ()

list(REMOVE_ITEM SRC_INTF
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/audio/aud_dsp.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/intf/audio/aud_interface.cpp
)

##############
##############
# cpu (智能递归收录所有子芯片源码，自动排除内联片段与JIT)
##############
file(GLOB_RECURSE SRC_CPU
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/cpu/*.cpp
)
# 单独追加真正需要独立编译的 C 文件（其余 .c 均为被 #include 的内联子表，严禁独立编译）
list(APPEND SRC_CPU ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/cpu/m68k/m68kcpu.c)

# 排除反汇编器等不需要的调试工具
list(FILTER SRC_CPU EXCLUDE REGEX ".*(dasm|x64|tms34010_intf|hmcs40op).*")

##############
# libchdr (CHD 光盘镜像解压支持)
##############
set(CHDR_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/libchdr
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/libchdr/include
)
file(GLOB SRC_CHDR
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/libchdr/*.c
)
list(FILTER SRC_CHDR EXCLUDE REGEX ".*(lzma|zstd).*")

##############
# 7z
##############
set(7Z_DIRS
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z
)
file(GLOB SRC_7Z
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zDec.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zArcIn.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zStream.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/LzmaDec.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/Lzma2Dec.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/Bra.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/Bcj2.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zBuf.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/Delta.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/Bra86.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/BraIA64.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zCrc.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/7zCrcOpt.c
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/libs/lib7z/CpuArch.c
)

##############
# pfbneo
##############
set(PFBA_DIRS sources sources/fbneo)
file(GLOB SRC_PFBA sources/*.c* sources/fbneo/*.c*)
list(APPEND SRC_PFBA ../main.cpp)


##############
# 自动递归收集 FBNeo 下所有包含头文件的子目录 (Recursive Include Dirs)
##############
file(GLOB_RECURSE ALL_HEADERS "${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/*.h")
set(FBNEO_INC_DIRS)
foreach(HDR ${ALL_HEADERS})
    get_filename_component(HDR_DIR ${HDR} DIRECTORY)
    if(NOT HDR_DIR MATCHES "dep/qtcreator" AND NOT HDR_DIR MATCHES "dep/vc" AND NOT HDR_DIR MATCHES "burner/win32")
        list(APPEND FBNEO_INC_DIRS ${HDR_DIR})
    endif()
endforeach()
list(APPEND FBNEO_INC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/cores/FBNeo/src/dep/libs/stb_vorbis")
    list(REMOVE_DUPLICATES FBNEO_INC_DIRS)

# includes
set(INC ${PFBA_DIRS}
        ${FBNEO_INC_DIRS}
        ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/generated
        ${CMAKE_CURRENT_BINARY_DIR}/deps
)


# flags
set(FLAGS
        -D__PFBA__
        -D__PEMU_VERSION_MAJOR__=${VERSION_MAJOR}
        -D__PEMU_VERSION_MINOR__=${VERSION_MINOR}
        -DLSB_FIRST
        -DCHDR_SYSTEM_ZLIB
        -Dregister=
        -DINCLUDE_7Z_SUPPORT -DMINIZIP_LIB
        -DBUILD_SDL2 -DBUILD_M68K
        -fno-operator-names
        -Wno-strict-aliasing -Wno-long-long
        -Wno-write-strings -Wno-sign-compare -Wno-uninitialized -Wno-unused
        -Wno-conversion -Wno-attributes -Wno-unused-parameter -Wno-unused-value
        -Wno-parentheses -Wno-sequence-point -Wno-format
        -Wno-misleading-indentation -Wno-unused-result
)

set(CMAKE_CXX_STANDARD 17)

#################
# PSP2 (PS Vita)
#################
if (PLATFORM_VITA)
    enable_language(ASM)
    set(TITLE_ID "PFBN00001" CACHE STRING "" FORCE)

    # Restore the original pemu Vita CPU path: Cyclone 68000 + ARM speed hacks.
    list(APPEND INC sources/cpu/cyclone)
    list(REMOVE_ITEM SRC_CPU
            ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/cpu/m68000_intf.cpp)
    list(APPEND SRC_PFBA
            sources/cpu/c68000_intf.cpp
            sources/cpu/cyclone/Cyclone.s)

    list(APPEND FLAGS
            -DBUILD_C68K
            -DUSE_FILE32API
            -DUSE_SPEEDHACKS
            -D__PFBA_ARM__)

    # VitaSDK uses static libstdc++/libpthread.  Force the pthread archive into
    # the ELF so pthread_once/pthread_key_* remain available even when a later
    # libstdc++.a introduces those references.
    list(APPEND LDFLAGS
            ScePower_stub
            "-Wl,--whole-archive"
            pthread
            "-Wl,--no-whole-archive")

elseif (PLATFORM_PS4)
    set(PS4_PKG_TITLE "pFBN - Portable Final Burn Neo")
    set(PS4_PKG_TITLE_ID "PFBN00001")
    set(PS4_PKG_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
    list(APPEND FLAGS -D__fastcall=)
    list(APPEND LDFLAGS)
endif ()

# source files
set(SRC ${SRC_PFBA} ${SRC_CPU} ${SRC_DRV} ${SRC_BURN} ${SRC_BURNER} ${SRC_INTF} ${SRC_7Z} ${SRC_CHDR})

#####################
# pfbneo executable
#####################
add_executable(${PROJECT_NAME} ${SRC} ${CMAKE_CURRENT_BINARY_DIR}/deps/m68kops.c)
target_compile_options(${PROJECT_NAME} PRIVATE ${FLAGS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INC})
target_link_libraries(${PROJECT_NAME} cross2dui cross2d ${LDFLAGS})
include(${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/targets.cmake)

# add common skin data
add_custom_target(${PROJECT_NAME}.data.skin
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/common/datadir -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_datadir -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/${TARGET_PLATFORM}/datadir -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_datadir -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/common/romfs -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_romfs -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/${TARGET_PLATFORM}/romfs -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_romfs -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
)
add_dependencies(${PROJECT_NAME}.data.skin ${PROJECT_NAME}.data)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data.skin)

# fbneo deps
find_package(Perl REQUIRED)
add_custom_target(${PROJECT_NAME}.deps
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/deps
        # ctv.h
        COMMAND g++ ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/capcom/ctv_make.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/deps/ctv_make
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/deps/ctv_make > ${CMAKE_CURRENT_BINARY_DIR}/deps/ctv.h
        # pgm sprite
        COMMAND g++ ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/burn/drv/pgm/pgm_sprite_create.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/deps/pgm_sprite_create
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/deps/pgm_sprite_create > ${CMAKE_CURRENT_BINARY_DIR}/deps/pgm_sprite.h
        # func headers
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/toa_gp9001_func.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/toa_gp9001_func.h
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/neo_sprite_func.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/neo_sprite_func.h
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/cave_tile_func.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/cave_tile_func.h
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/cave_sprite_func.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/cave_sprite_func.h
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/psikyo_tile_func.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/psikyo_tile_func.h
        # driverlist.h
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/external/cores/FBNeo/src/dep/scripts/gamelist.pl -o ${CMAKE_CURRENT_BINARY_DIR}/deps/driverlist.h -l ${CMAKE_CURRENT_BINARY_DIR}/deps/gamelist.txt ${SRC_DRV}
)
