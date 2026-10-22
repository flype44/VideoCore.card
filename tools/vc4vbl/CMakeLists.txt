cmake_minimum_required(VERSION 3.14.0)
project(vc4vbl VERSION 1.0)

# Probe of the vertical blank interrupt of the VideoCore IV (Raspberry Pi 3), not part of the driver.
# Build: CC=m68k-amigaos-gcc cmake <this directory> && make vc4vbl

if(NOT TARGET devicetree)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../devicetree.resource devicetree EXCLUDE_FROM_ALL)
endif()

add_executable(vc4vbl vc4vbl.c)
target_compile_options(vc4vbl PRIVATE -Os -m68040 -noixemul -Wall -Wextra)
target_link_options(vc4vbl PRIVATE -noixemul -s)
target_link_libraries(vc4vbl devicetree)
