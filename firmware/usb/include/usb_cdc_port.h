#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void UsbCdc_Init(void);
void UsbCdc_Poll(void);
void UsbCdc_IrqHandler(void);
bool UsbCdc_IsConfigured(void);
size_t UsbCdc_Write(const uint8_t *data, size_t len);
int UsbCdc_ReadByte(void);
