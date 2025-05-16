# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.24.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(picostation)

pico_sdk_init()

function(addBinaryFileWithSize target name sizeName path)
    set(asmFile "${PROJECT_BINARY_DIR}/includes/${target}_${name}.s")
    cmake_path(ABSOLUTE_PATH path OUTPUT_VARIABLE fullPath)

    file(
        CONFIGURE
        OUTPUT  "${asmFile}"
        CONTENT [[
.section .rodata.${name}, "a"
.balign 8

.global ${name}
.type ${name}, %object
.size ${name}, (${name}_end - ${name})

${name}:
    .incbin "${fullPath}"
${name}_end:

.section .rodata.${sizeName}, "a"
.balign 4

.global ${sizeName}
.type ${sizeName}, %object
.size ${sizeName}, 4

${sizeName}:
    .int (${name}_end - ${name})
]]
        ESCAPE_QUOTES
        NEWLINE_STYLE LF
    )

    target_sources(${target} PRIVATE "${asmFile}")
    set_source_files_properties(
        "${asmFile}" PROPERTIES OBJECT_DEPENDS "${fullPath}"
    )
endfunction()


add_executable(picostation)

target_compile_definitions(
    picostation PUBLIC
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    MAXINDEX=2
)

addBinaryFileWithSize(picostation loaderImage loaderImageSize binary/picoloader.bin)

target_sources(picostation PRIVATE
    src/cmd.cpp
    src/disc_image.cpp
    src/drive_mechanics.cpp
    src/hw_config.cpp
    src/i2s.cpp
    src/main.cpp
    src/modchip.cpp
    src/picostation.cpp
    src/subq.cpp
    src/utils.cpp
    src/filesystem.cpp
    third_party/cueparser/cueparser.c
    third_party/cueparser/fileabstract.c
    third_party/cueparser/scheduler.c
    third_party/iec-60908b/edcecc.c
    third_party/iec-60908b/tables.c
    third_party/posix_file.c
)

target_include_directories(picostation PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/third_party
)

add_subdirectory(third_party/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)
add_subdirectory(third_party/RP2040_Pseudo_Atomic)

pico_enable_stdio_usb(picostation 1)

pico_generate_pio_header(picostation ${CMAKE_CURRENT_LIST_DIR}/pio/main.pio)

target_link_libraries(picostation PRIVATE
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_vreg
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    pico_bootrom
    pico_multicore
    pico_stdlib
)

target_include_directories(no-OS-FatFS-SD-SDIO-SPI-RPi-Pico BEFORE INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(picostation)
