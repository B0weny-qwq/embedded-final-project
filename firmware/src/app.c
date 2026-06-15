#include "app.h"

#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "app_config.h"
#include "bsp_adc_temp.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "filter.h"
#include "protocol.h"
#include "temperature.h"
#include "usb_cdc_port.h"

static AlarmState alarm_state;
static EwmaFilter filter_state;
static LedMode led_mode = LED_MODE_AUTO;
static BeepMode beep_mode = BEEP_MODE_AUTO;
static int32_t raw_cx100;
static int32_t filtered_cx100;
static uint32_t last_sample_ms;
static uint32_t last_key_scan_ms;
static uint32_t last_output_ms;
static char rx_line[80];
static uint8_t rx_len;

static uint8_t triangle_brightness(uint32_t ms, uint32_t period_ms)
{
    uint32_t phase = ms % period_ms;
    uint32_t half = period_ms / 2U;
    uint32_t value = phase < half ? phase : (period_ms - phase);
    return (uint8_t)((value * 255U) / half);
}

static void apply_outputs(void)
{
    uint32_t now = HAL_GetTick();

    if (led_mode == LED_MODE_AUTO) {
        if (alarm_state.active) {
            bool on = ((now / APP_ALARM_LED_BLINK_HALF_PERIOD_MS) % 2U) == 0U;
            BspLed_ApplyMode(on ? LED_MODE_RED : LED_MODE_OFF, 255);
        } else {
            BspLed_ApplyMode(LED_MODE_GREEN, triangle_brightness(now, APP_NORMAL_LED_BREATH_PERIOD_MS));
        }
    } else {
        BspLed_ApplyMode(led_mode, 255);
    }

    bool beep_on = false;
    if (beep_mode == BEEP_MODE_ON) {
        beep_on = true;
    } else if (beep_mode == BEEP_MODE_AUTO && alarm_state.active && !alarm_state.silenced) {
        beep_on = (now % APP_ALARM_BEEP_PERIOD_MS) < APP_ALARM_BEEP_ON_MS ||
                  ((now + APP_ALARM_BEEP_SECOND_OFFSET_MS) % APP_ALARM_BEEP_PERIOD_MS) < APP_ALARM_BEEP_ON_MS;
    }
    BspBeep_Set(beep_on);
}

static void handle_command(const ProtocolCommand *command)
{
    switch (command->kind) {
    case COMMAND_THRESHOLD:
        App_SetThreshold(command->threshold_cx100);
        break;
    case COMMAND_LED:
        App_SetLedMode(command->led_mode);
        break;
    case COMMAND_BEEP:
        App_SetBeepMode(command->beep_mode);
        break;
    case COMMAND_ACK:
        App_AckAlarm();
        break;
    case COMMAND_NONE:
    default:
        break;
    }
}

static void process_usb_rx(void)
{
    int byte = UsbCdc_ReadByte();
    while (byte >= 0) {
        char ch = (char)byte;
        if (ch == '\n' || ch == '\r') {
            if (rx_len > 0U) {
                rx_line[rx_len] = '\0';
                ProtocolCommand command;
                if (Protocol_ParseLine(rx_line, &command)) {
                    handle_command(&command);
                }
                rx_len = 0;
            }
        } else if (rx_len < sizeof(rx_line) - 1U) {
            rx_line[rx_len++] = ch;
        } else {
            rx_len = 0;
        }
        byte = UsbCdc_ReadByte();
    }
}

static void sample_and_report(void)
{
    uint32_t adc = BspAdcTemp_ReadRaw();
    raw_cx100 = Temperature_FromAdcCx100(adc);
    filtered_cx100 = Filter_Update(&filter_state, raw_cx100);
    Alarm_Update(&alarm_state, filtered_cx100);

    if (UsbCdc_IsConfigured()) {
        char line[96];
        int len = Protocol_FormatTelemetry(line,
                                           sizeof(line),
                                           HAL_GetTick(),
                                           raw_cx100,
                                           filtered_cx100,
                                           alarm_state.active,
                                           led_mode,
                                           beep_mode);
        if (len > 0) {
            UsbCdc_Write((const uint8_t *)line, (size_t)len);
        }
    }
}

void App_Init(void)
{
    BspLed_Init();
    BspBeep_Init();
    BspKey_Init();
    BspAdcTemp_Init();
    UsbCdc_Init();
    Filter_Init(&filter_state);
    Alarm_Init(&alarm_state, APP_ALARM_THRESHOLD_CX100, APP_RECOVERY_THRESHOLD_CX100);
    last_sample_ms = HAL_GetTick();
    last_key_scan_ms = last_sample_ms;
    last_output_ms = last_sample_ms;
}

void App_Tick(void)
{
    UsbCdc_Poll();
    process_usb_rx();

    uint32_t now = HAL_GetTick();
    if ((now - last_key_scan_ms) >= APP_KEY_SCAN_PERIOD_MS) {
        last_key_scan_ms += APP_KEY_SCAN_PERIOD_MS;
        if (BspKey_K1PressedEdge()) {
            App_AckAlarm();
        }
    }

    if ((now - last_sample_ms) >= APP_SAMPLE_PERIOD_MS) {
        last_sample_ms += APP_SAMPLE_PERIOD_MS;
        sample_and_report();
    }

    if ((now - last_output_ms) >= APP_OUTPUT_PERIOD_MS) {
        last_output_ms += APP_OUTPUT_PERIOD_MS;
        apply_outputs();
    }
}

void App_SetThreshold(int32_t threshold_cx100)
{
    Alarm_SetThreshold(&alarm_state, threshold_cx100);
}

void App_SetLedMode(LedMode mode)
{
    led_mode = mode;
}

void App_SetBeepMode(BeepMode mode)
{
    beep_mode = mode;
    if (mode == BEEP_MODE_AUTO) {
        alarm_state.silenced = false;
    }
}

void App_AckAlarm(void)
{
    Alarm_Ack(&alarm_state);
}
