# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "$ENV{HOME}/.pico-sdk/sdk/2.3.1")
set(PICO_BOARD pico2)

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(epaper_uc8253_c C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(epaper_uc8253_c
    main.c
)

target_link_libraries(epaper_uc8253_c
    pico_stdlib
    hardware_uart
    hardware_spi
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
