cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(main)

pico_sdk_init()

add_executable(main    
    main.c
)

pico_add_extra_outputs(main)
pico_enable_stdio_usb(main 0)
pico_enable_stdio_uart(main 1)

target_include_directories(main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(main 
    pico_stdlib
    pico_multicore
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
)

add_custom_target(flash
    COMMAND /bin/bash ../flash
    DEPENDS main
)