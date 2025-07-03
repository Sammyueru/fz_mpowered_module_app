// Flipper Zero M-Powered Module App | Copyright (c) 2025, Samm. Licensed under MIT.
#ifndef MESHTASTIC_MODULE_PB_ENCODER_H
#define MESHTASTIC_MODULE_PB_ENCODER_H
#include <furi_hal_serial.h>
#include <stdlib.h>
#include <stdint.h>

size_t pb_encode_int(uint32_t value, uint8_t* buf);
void pb_send_want_config_id(FuriHalSerialHandle* serial_handle);

#endif
