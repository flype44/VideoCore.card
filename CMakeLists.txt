cmake_minimum_required(VERSION 3.14.0)
project(VideoCore VERSION 1.5)

include(cmake/verstring.cmake)
get_verstring(VERSTRING)

if(NOT TARGET devicetree)
    add_subdirectory(devicetree.resource EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET unicam)
    add_subdirectory(unicam.resource EXCLUDE_FROM_ALL)
endif()

add_executable(VideoCore.card
    src/main.c
    src/mbox.c
    src/vc4.c
    src/vc6.c
    src/utils.c
    src/debug.c
    src/end.c
    src/unicam.c
    src/buddyalloc.c
    src/hvs.c
    src/chip.c
    src/vblank.c
    src/special-features.c
    src/memory-window.c
    src/task.c
)

target_include_directories(VideoCore.card PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(VideoCore.card PRIVATE -Os -m68040 -fomit-frame-pointer)
target_compile_definitions(VideoCore.card PRIVATE VERSION_STRING="${VERSTRING}")
target_link_options(VideoCore.card PRIVATE -ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
target_link_libraries(VideoCore.card devicetree mailbox amiga unicam)
target_compile_definitions(VideoCore.card PUBLIC CARD_NAME="VideoCore.card")

# vcmeter, a demonstrator of the memory window of the driver (a Picasso96 PiP), only on demand: make vcmeter
# It needs the include files of the Picasso96 SDK for programs, which are not in this repository:
#   cmake -DP96_INCLUDE=<path>/Picasso96DevelopPublic/Include <source>
set(P96_INCLUDE "" CACHE PATH "The Include directory of the Picasso96 SDK for programs, for vcmeter")
if(P96_INCLUDE)
    add_executable(vcmeter EXCLUDE_FROM_ALL
        tools/vcmeter/start.c
        tools/vcmeter/main.c
    )
    target_include_directories(vcmeter PRIVATE ${P96_INCLUDE})
    target_compile_options(vcmeter PRIVATE -Os -m68040 -fomit-frame-pointer -Wall -Wno-pointer-sign)
    target_link_options(vcmeter PRIVATE -ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
    target_link_libraries(vcmeter mailbox amiga)
endif()

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/VideoCore.card DESTINATION VideoCore/)
