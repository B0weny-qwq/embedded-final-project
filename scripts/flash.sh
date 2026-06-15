#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

openocd \
  -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg \
  -c "transport select swd" \
  -f /usr/share/openocd/scripts/target/stm32f1x.cfg \
  -c "adapter speed 1000" \
  -c "program ${ROOT_DIR}/build/firmware.elf verify reset exit"
