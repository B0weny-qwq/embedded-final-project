# Technical Plan

This repository contains a self-contained demonstration for STM32 chip
temperature monitoring:

- Firmware: CMake + `arm-none-eabi-gcc`, STM32 HAL, internal ADC temperature
  sensor, EWMA filtering, alarm state, RGB LED, buzzer, K1 acknowledgement, and
  USB CDC ACM text telemetry.
- Host: Rust + eframe/egui, `serialport`, and `egui_plot` for serial
  connection management, live plotting, and control commands.

Defaults:

- Alarm threshold: 45.00 C.
- Recovery threshold: 42.00 C.
- Report rate: 10 Hz.
- Normal output: green breathing LED, buzzer off.
- Alarm output: red fast blink, active-buzzer alarm pattern.

Build firmware:

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build
```

Run host:

```sh
cd host
cargo run
```

Flash firmware:

```sh
scripts/flash.sh
```
