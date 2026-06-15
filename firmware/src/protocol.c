#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_led(const char *value, LedMode *mode)
{
    if (strcmp(value, "AUTO") == 0) {
        *mode = LED_MODE_AUTO;
    } else if (strcmp(value, "OFF") == 0) {
        *mode = LED_MODE_OFF;
    } else if (strcmp(value, "RED") == 0) {
        *mode = LED_MODE_RED;
    } else if (strcmp(value, "GREEN") == 0) {
        *mode = LED_MODE_GREEN;
    } else if (strcmp(value, "BLUE") == 0) {
        *mode = LED_MODE_BLUE;
    } else if (strcmp(value, "WHITE") == 0) {
        *mode = LED_MODE_WHITE;
    } else {
        return false;
    }
    return true;
}

static bool parse_beep(const char *value, BeepMode *mode)
{
    if (strcmp(value, "AUTO") == 0) {
        *mode = BEEP_MODE_AUTO;
    } else if (strcmp(value, "ON") == 0) {
        *mode = BEEP_MODE_ON;
    } else if (strcmp(value, "OFF") == 0) {
        *mode = BEEP_MODE_OFF;
    } else {
        return false;
    }
    return true;
}

const char *Protocol_LedModeName(LedMode mode)
{
    switch (mode) {
    case LED_MODE_AUTO:
        return "AUTO";
    case LED_MODE_OFF:
        return "OFF";
    case LED_MODE_RED:
        return "RED";
    case LED_MODE_GREEN:
        return "GREEN";
    case LED_MODE_BLUE:
        return "BLUE";
    case LED_MODE_WHITE:
        return "WHITE";
    default:
        return "AUTO";
    }
}

const char *Protocol_BeepModeName(BeepMode mode)
{
    switch (mode) {
    case BEEP_MODE_AUTO:
        return "AUTO";
    case BEEP_MODE_ON:
        return "ON";
    case BEEP_MODE_OFF:
        return "OFF";
    default:
        return "AUTO";
    }
}

bool Protocol_ParseLine(const char *line, ProtocolCommand *command)
{
    char copy[64];
    char *save = NULL;
    char *tok = NULL;

    command->kind = COMMAND_NONE;
    if (strlen(line) >= sizeof(copy)) {
        return false;
    }
    strcpy(copy, line);

    tok = strtok_r(copy, ",\r\n", &save);
    if (tok == NULL || strcmp(tok, "CMD") != 0) {
        return false;
    }

    tok = strtok_r(NULL, ",\r\n", &save);
    if (tok == NULL) {
        return false;
    }

    if (strcmp(tok, "ACK") == 0) {
        command->kind = COMMAND_ACK;
        return true;
    }

    if (strcmp(tok, "TH") == 0) {
        char *end = NULL;
        tok = strtok_r(NULL, ",\r\n", &save);
        if (tok == NULL) {
            return false;
        }
        long value = strtol(tok, &end, 10);
        if (*end != '\0') {
            return false;
        }
        command->kind = COMMAND_THRESHOLD;
        command->threshold_cx100 = (int32_t)value;
        return true;
    }

    if (strcmp(tok, "LED") == 0) {
        tok = strtok_r(NULL, ",\r\n", &save);
        if (tok == NULL || !parse_led(tok, &command->led_mode)) {
            return false;
        }
        command->kind = COMMAND_LED;
        return true;
    }

    if (strcmp(tok, "BEEP") == 0) {
        tok = strtok_r(NULL, ",\r\n", &save);
        if (tok == NULL || !parse_beep(tok, &command->beep_mode)) {
            return false;
        }
        command->kind = COMMAND_BEEP;
        return true;
    }

    return false;
}

int Protocol_FormatTelemetry(char *buffer,
                             size_t len,
                             uint32_t ms,
                             int32_t raw_cx100,
                             int32_t filtered_cx100,
                             bool alarm,
                             LedMode led,
                             BeepMode beep)
{
    return snprintf(buffer,
                    len,
                    "T,%lu,%ld,%ld,%u,%s,%s\n",
                    (unsigned long)ms,
                    (long)raw_cx100,
                    (long)filtered_cx100,
                    alarm ? 1U : 0U,
                    Protocol_LedModeName(led),
                    Protocol_BeepModeName(beep));
}
