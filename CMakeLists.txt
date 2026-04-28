# Encoder sniffer for Raspberry Pi Pico
# Copyright (C) 2026-...  Oleksandr Kolodkin <oleksandr.kolodkin@ukr.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 3.20)
include(FetchContent)
include(cmake/flash.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(encoder-emulator C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ---- u8g2 (display library) ---------------------------------------------------

FetchContent_Declare(
    u8g2_fetch
    GIT_REPOSITORY https://github.com/olikraus/u8g2.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  "csrc"   # csrc/ has no CMakeLists.txt → MakeAvailable skips add_subdirectory
)
FetchContent_MakeAvailable(u8g2_fetch)

file(GLOB U8G2_SOURCES "${u8g2_fetch_SOURCE_DIR}/csrc/*.c")
add_library(u8g2lib STATIC ${U8G2_SOURCES})
target_include_directories(u8g2lib PUBLIC "${u8g2_fetch_SOURCE_DIR}/csrc")
target_compile_options(u8g2lib PRIVATE -w -ffunction-sections -fdata-sections)

# ---- WS2812 PIO ---------------------------------------------------------------
add_library(ws2812_pio INTERFACE)
pico_generate_pio_header(ws2812_pio ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# ---- Input debounce PIO -------------------------------------------------------
add_library(input_debounce_pio INTERFACE)
pico_generate_pio_header(input_debounce_pio ${CMAKE_CURRENT_LIST_DIR}/input_debounce.pio)

# ---- Main executable ----------------------------------------------------------
add_executable(encoder-emulator
    main.c
    emulated.c
    display.c
    input.c
    serial.c
    encoder_out.c
)

# Set the name and version of the program (required for UF2 generation)
pico_set_program_name(encoder-emulator "encoder-emulator")
pico_set_program_version(encoder-emulator "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(encoder-emulator 0)
pico_enable_stdio_usb(encoder-emulator 1)

# Add the standard include files to the build
target_include_directories(encoder-emulator PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Add libraries to the executable
target_link_libraries(encoder-emulator
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_timer
    hardware_clocks
    u8g2lib
    ws2812_pio
    input_debounce_pio
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(encoder-emulator)

# Print memory usage after each successful build
add_custom_command(TARGET encoder-emulator POST_BUILD
    COMMAND arm-none-eabi-size $<TARGET_FILE:encoder-emulator>
    VERBATIM
)

# Flash to device on every build invocation, even if nothing changed.
# Override at configure time if auto-detection fails:
#   cmake -DFLASH_PORT=COM3 ..
#   cmake -DFLASH_UID=e6614c311b812b23 ..  (from the 'uid' serial command)
set(FLASH_PORT "" CACHE STRING "Serial port for flashing (e.g. COM3). Auto-detected if empty.")
set(FLASH_UID  "" CACHE STRING "RP2040 unique ID (hex) to target a specific device.")

pico_add_flash_target(encoder-emulator PORT ${FLASH_PORT} UID ${FLASH_UID})
