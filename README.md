# Embedded Final Project

STM32F103 chip-temperature monitor with USB CDC communication, realtime host
plotting, LED/buzzer control, filtering, and alarm handling.

## Build

Firmware:

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build
```

Host:

```sh
cd host
cargo run
```

Flash:

```sh
scripts/flash.sh
```

## Target Board

- Board: Wildfire / EmbedFire STM32F103 "Zhinanzhe"
- MCU: STM32F103VET6
- Reference material path:

```text
/home/sov710/temp/野火【STM32F103开发板-指南者】资料
```

## Selected Architecture

- Firmware framework: STM32 HAL + STM32 USB Device Library
- Build system: CMake
- Compiler: arm-none-eabi-gcc
- Debug/flashing: CMSIS-DAP + OpenOCD
- Temperature sensor: STM32 internal temperature sensor via ADC1
- USB communication: native USB Device port, USB CDC ACM virtual serial
- Host application: Rust + egui/eframe
- Plotting: egui_plot
- Serial access: Rust `serialport`
- Filtering: EWMA / first-order IIR

## Hardware Mapping

| Function | Pin | Notes |
| --- | --- | --- |
| Red LED | PB5 | RGB LED, common-anode, low active |
| Green LED | PB0 | RGB LED, common-anode, low active |
| Blue LED | PB1 | RGB LED, common-anode, low active |
| Buzzer | PA8 | Active buzzer, high active |
| K1 | PA0 | Pressed = high |
| K2 | PC13 | Pressed = high |
| USB D- | PA11 | Native USB Device |
| USB D+ | PA12 | Native USB Device |
| USB soft disconnect | PD3 | From Wildfire USB device example |

Use the board's USB Device Mini USB connector for CDC communication. The CH340 USB-to-UART connector is a separate USART1 path and is only a fallback/debug option.

## Clock Plan

| Item | Value |
| --- | --- |
| HSE | 8 MHz |
| SYSCLK | 72 MHz |
| HCLK | 72 MHz |
| APB1 | 36 MHz |
| APB2 | 72 MHz |
| USB clock | 48 MHz, PLL / 1.5 |
| Flash latency | 2 wait states |
| ADC clock | 12 MHz, PCLK2 / 6 |

## Firmware Behavior

- Sample STM32 internal chip temperature.
- Smooth temperature with EWMA filtering.
- Send realtime samples to host over USB CDC.
- Normal state: green breathing LED.
- Alarm state: red fast blinking LED and active-buzzer alarm pattern.
- Default alarm threshold: 45.00 C.
- Default recovery threshold: 42.00 C.
- Alarm clears after temperature recovers or after user acknowledgement where applicable.
- Host can display and control LED and buzzer state.

The board buzzer is active, so the alarm should be implemented as a rhythm/pattern. Accurate musical notes require an external passive buzzer.

## USB Application Protocol

Initial protocol: newline-delimited ASCII text.

Device to host:

```text
T,<ms>,<raw_c_x100>,<filtered_c_x100>,<alarm>,<led>,<beep>
```

Host to device:

```text
CMD,TH,<centi_c>
CMD,LED,AUTO|OFF|RED|GREEN|BLUE|WHITE
CMD,BEEP,AUTO|ON|OFF
CMD,ACK
```

The text protocol is chosen first because it can be debugged directly with serial tools on `/dev/ttyACM*`.

## Recommended Directory Layout

```text
embedded-final-project/
├── CMakeLists.txt
├── cmake/
│   ├── arm-none-eabi-gcc.cmake
│   └── stm32f103vetx.cmake
├── firmware/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── app_config.h
│   │   └── board.h
│   ├── src/
│   │   ├── main.c
│   │   ├── system_clock.c
│   │   ├── app.c
│   │   ├── temperature.c
│   │   ├── filter.c
│   │   ├── alarm.c
│   │   ├── protocol.c
│   │   └── usb_cdc_port.c
│   ├── bsp/
│   │   ├── bsp_led.c
│   │   ├── bsp_beep.c
│   │   ├── bsp_key.c
│   │   └── bsp_adc_temp.c
│   ├── drivers/
│   │   ├── CMSIS/
│   │   ├── STM32F1xx_HAL_Driver/
│   │   └── STM32_USB_Device_Library/
│   ├── startup/
│   │   └── startup_stm32f103xe.s
│   ├── linker/
│   │   └── STM32F103VETx_FLASH.ld
│   └── usb/
│       ├── usbd_conf.c
│       ├── usbd_desc.c
│       ├── usbd_cdc_if.c
│       └── include/
│           ├── usbd_conf.h
│           ├── usbd_desc.h
│           └── usbd_cdc_if.h
├── host/
│   ├── Cargo.toml
│   └── src/
│       ├── main.rs
│       ├── app.rs
│       ├── serial.rs
│       ├── protocol.rs
│       ├── plot.rs
│       └── state.rs
├── docs/
│   ├── technical-plan.md
│   ├── protocol.md
│   └── hardware-pins.md
├── scripts/
│   ├── flash.sh
│   └── monitor.sh
└── AGENTS.md
```

## Flash Command

```sh
openocd \
  -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg \
  -c "transport select swd" \
  -f /usr/share/openocd/scripts/target/stm32f1x.cfg \
  -c "adapter speed 1000" \
  -c "program build/firmware.elf verify reset exit"
```
