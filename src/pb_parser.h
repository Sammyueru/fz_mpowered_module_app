// Flipper Zero M-Powered Module App | Copyright (c) 2025, Samm. Licensed under MIT.
#ifndef MESHTASTIC_MODULE_PB_PARSER_H
#define MESHTASTIC_MODULE_PB_PARSER_H
#include <stdint.h>
#include <stddef.h>

#define MSG_SZ 200

void pb_parse_endpoint(const uint8_t* buf, size_t len, uint64_t* out_time, char* out_short_name, size_t sn_sz, char* msg);

#endif
