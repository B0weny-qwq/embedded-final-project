# Hardware Pins

Target board: EmbedFire Wildfire STM32F103 "Zhinanzhe", MCU STM32F103VET6.

| Function | Pin | Behavior |
| --- | --- | --- |
| RGB LED red | PB5 / TIM3 CH2 partial remap | Common-anode, low active |
| RGB LED green | PB0 / TIM3 CH3 | Common-anode, low active |
| RGB LED blue | PB1 / TIM3 CH4 | Common-anode, low active |
| Buzzer | PA8 | Active buzzer, high active |
| K1 | PA0 | Pressed = high |
| K2 | PC13 | Pressed = high, reserved |
| USB D- | PA11 | Native USB Device DM |
| USB D+ | PA12 | Native USB Device DP |
| USB soft disconnect | PD3 | Board USB disconnect control |

Clock plan:

| Clock | Value |
| --- | --- |
| HSE | 8 MHz |
| PLL | 8 MHz * 9 |
| SYSCLK/HCLK | 72 MHz |
| APB1 | 36 MHz |
| APB2 | 72 MHz |
| USB | 48 MHz from PLL / 1.5 |
| ADC | PCLK2 / 6 = 12 MHz |
