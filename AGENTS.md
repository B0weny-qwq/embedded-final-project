# Project Notes

This project targets the EmbedFire Wildfire STM32F103 "Zhinanzhe" board.

## Fixed Tooling

- Build system: CMake
- Firmware compiler: arm-none-eabi-gcc
- Flash/debug path: CMSIS-DAP with OpenOCD
- Verified flash command:

```sh
openocd \
  -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg \
  -c "transport select swd" \
  -f /usr/share/openocd/scripts/target/stm32f1x.cfg \
  -c "adapter speed 1000" \
  -c "program build/firmware.elf verify reset exit"
```

## Technical Choices

- Firmware framework: STM32 HAL plus STM32 USB Device Library.
- MCU: STM32F103VET6.
- Temperature source: internal STM32 temperature sensor through ADC1 channel `ADC_CHANNEL_TEMPSENSOR`.
- USB interface: native USB Device Mini USB port, not the CH340 USB-to-UART port.
- USB protocol: USB CDC ACM virtual serial port.
- Host application: Rust with egui/eframe, using `serialport` and `egui_plot`.
- Temperature smoothing: EWMA / first-order IIR filter.
- Alarm defaults: 45.00 C alarm threshold, 42.00 C recovery threshold unless changed later.

## Board Pin Mapping

| Function | Pin | Behavior |
| --- | --- | --- |
| RGB LED red | PB5 | Common-anode LED, low active |
| RGB LED green | PB0 | Common-anode LED, low active |
| RGB LED blue | PB1 | Common-anode LED, low active |
| Buzzer | PA8 | Active buzzer, high active |
| Key K1 | PA0 | Pressed = high |
| Key K2 | PC13 | Pressed = high |
| USB D- | PA11 | Native USB Device DM |
| USB D+ | PA12 | Native USB Device DP |
| USB soft disconnect | PD3 | Used by Wildfire USB device example |

## Clock Plan

| Clock | Value |
| --- | --- |
| HSE | 8 MHz |
| PLL | 8 MHz * 9 |
| SYSCLK/HCLK | 72 MHz |
| APB1 | 36 MHz |
| APB2 | 72 MHz |
| USB clock | 48 MHz from PLL / 1.5 |
| Flash latency | 2 wait states |
| ADC clock | PCLK2 / 6 = 12 MHz |

## Runtime Behavior

- Firmware samples chip temperature continuously and reports data to the host over USB CDC.
- Host displays raw and filtered temperature curves in real time.
- Normal state: green breathing LED.
- Alarm state: red fast blinking LED and active-buzzer alarm pattern.
- Alarm is raised when filtered temperature exceeds threshold.
- Alarm clears automatically after temperature falls below recovery threshold.
- K1 or host ACK can silence the buzzer, but the host should keep showing alarm while temperature remains above threshold.
- Host can display and control LED and buzzer state.

## Application Protocol

Use newline-delimited ASCII text first, because it is easy to debug through `/dev/ttyACM*`.

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

Binary framing can be added later if the text protocol proves insufficient.

## Recommended Repository Layout

```text
embedded-final-project/
├── CMakeLists.txt
├── cmake/
│   ├── arm-none-eabi-gcc.cmake
│   └── stm32f103vetx.cmake
├── firmware/
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── bsp/
│   ├── drivers/
│   ├── startup/
│   ├── linker/
│   └── usb/
├── host/
│   ├── Cargo.toml
│   └── src/
├── docs/
├── scripts/
└── README.md
```

## Current Hardware State

The PC has been physically connected to the board's USB Device Mini USB port. Before CDC firmware is flashed, no `/dev/ttyACM*` device is expected. The CMSIS-DAP device is also connected and should be used for flashing.
