// Flipper Zero M-Powered Module App | Copyright (c) 2025, Samm. Licensed under MIT.
#include "pb_encoder.h"

size_t pb_encode_int(uint32_t value, uint8_t* buf) {
    size_t i = 0;
    while (value > 0x7F) {
        buf[i++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buf[i++] = (uint8_t)value;
    return i;
}

void pb_send_want_config_id(FuriHalSerialHandle* serial_handle) {
    uint32_t cfg_id = (uint32_t)rand() | 1;
    uint8_t payload[8];
    size_t payload_len = 0;
    payload[payload_len++] = 0x18;
    payload_len += pb_encode_int(cfg_id, payload + payload_len);

    uint8_t header[4] = { 0x94, 0xC3, (uint8_t)((payload_len >> 8) & 0xFF), (uint8_t)(payload_len & 0xFF) };
    furi_hal_serial_tx(serial_handle, header, sizeof(header));
    furi_hal_serial_tx(serial_handle, payload, payload_len);
    furi_hal_serial_tx_wait_complete(serial_handle);
}
