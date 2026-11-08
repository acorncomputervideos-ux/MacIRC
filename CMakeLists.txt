cmake_minimum_required(VERSION 3.10)
project(MacIRC C)

# Build with the Retro68 toolchain, e.g.:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=<Retro68-build>/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
#   cmake --build build

add_application(MacIRC
    src/main.c
    src/window.c
    src/irc.c
    src/tcp.c
    src/dnr.c
    src/charset.c
    src/prefs.c
    src/dialogs.c
    src/util.c
    src/log.c
    src/url.c
    src/chanlist.c
    src/MacIRC.r
    src/Balloons.r
    TYPE "APPL"
    CREATOR "MIRC"
)

target_compile_options(MacIRC PRIVATE -Os -Wall -Wno-multichar -Wno-pointer-sign -ffunction-sections)
set_target_properties(MacIRC PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
