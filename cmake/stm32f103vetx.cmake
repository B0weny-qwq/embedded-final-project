set(STM32_CPU_FLAGS
    -mcpu=cortex-m3
    -mthumb
)

set(STM32_COMMON_FLAGS
    ${STM32_CPU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
)

set(STM32_LINK_FLAGS
    ${STM32_CPU_FLAGS}
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nosys.specs
)
