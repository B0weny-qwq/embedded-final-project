#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"

typedef enum {
    COMMAND_NONE = 0,
    COMMAND_THRESHOLD,
    COMMAND_LED,
    COMMAND_BEEP,
    COMMAND_ACK,
} CommandKind;

typedef struct {
    CommandKind kind;
    int32_t threshold_cx100;
    LedMode led_mode;
    BeepMode beep_mode;
} ProtocolCommand;

const char *Protocol_LedModeName(LedMode mode);
const char *Protocol_BeepModeName(BeepMode mode);
bool Protocol_ParseLine(const char *line, ProtocolCommand *command);
int Protocol_FormatTelemetry(char *buffer,
                             size_t len,
                             uint32_t ms,
                             int32_t raw_cx100,
                             int32_t filtered_cx100,
                             bool alarm,
                             LedMode led,
                             BeepMode beep);
