include(${CMAKE_SOURCE_DIR}/cmake/stm32f103vetx.cmake)

set(HAL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/drivers/STM32F1xx_HAL_Driver/Src)

add_executable(firmware.elf
    startup/startup_stm32f103xe.s
    src/main.c
    src/system_stm32f1xx.c
    src/stm32f1xx_it.c
    src/app.c
    src/system_clock.c
    src/filter.c
    src/alarm.c
    src/protocol.c
    src/temperature.c
    bsp/bsp_adc_temp.c
    bsp/bsp_beep.c
    bsp/bsp_key.c
    bsp/bsp_led.c
    usb/usb_cdc_port.c
    ${HAL_SRC_DIR}/stm32f1xx_hal.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_adc.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_adc_ex.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_cortex.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_dma.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_exti.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_flash.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_flash_ex.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_gpio.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_gpio_ex.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_pwr.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_rcc.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_rcc_ex.c
    ${HAL_SRC_DIR}/stm32f1xx_hal_tim.c
)

target_include_directories(firmware.elf PRIVATE
    include
    usb/include
    drivers/CMSIS/Include
    drivers/CMSIS/Device/ST/STM32F1xx/Include
    drivers/STM32F1xx_HAL_Driver/Inc
    drivers/STM32F1xx_HAL_Driver/Inc/Legacy
)

target_compile_definitions(firmware.elf PRIVATE
    STM32F103xE
    USE_HAL_DRIVER
)

target_compile_options(firmware.elf PRIVATE ${STM32_COMMON_FLAGS})
target_link_options(firmware.elf PRIVATE
    ${STM32_LINK_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F103VETx_FLASH.ld
)

set_target_properties(firmware.elf PROPERTIES
    OUTPUT_NAME firmware
    SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_command(TARGET firmware.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware.elf> ${CMAKE_BINARY_DIR}/firmware.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:firmware.elf> ${CMAKE_BINARY_DIR}/firmware.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware.elf>
    BYPRODUCTS ${CMAKE_BINARY_DIR}/firmware.bin ${CMAKE_BINARY_DIR}/firmware.hex
)
