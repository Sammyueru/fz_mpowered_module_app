// Flipper Zero M-Powered Module App | Copyright (c) 2025, Samm. Licensed under MIT.
#include "pb_parser.h"
#include <string.h>

static uint64_t parse_int(const uint8_t* buf, size_t len, size_t* out_consumed) {
    uint64_t value = 0;
    int32_t shift = 0;
    size_t i = 0;
    while (i < len) {
        uint8_t byte = buf[i++];
        value |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    *out_consumed = i;
    return value;
}

void pb_parse_endpoint(const uint8_t* buf, size_t len, uint64_t* out_time, char* out_short_name, size_t sn_sz, char* msg) {
    size_t idx = 0;
    *out_time = 0;
    out_short_name[0] = '\0';
    msg[0] = '\0';

    while (idx < len) {
        size_t consumed;
        uint64_t key = parse_int(buf + idx, len - idx, &consumed);
        idx += consumed;
        uint32_t field = key >> 3;
        uint32_t wire = key & 0x7;

        // time
        if (field == 2 && wire == 0) {
            *out_time = (uint64_t)parse_int(buf + idx, len - idx, &consumed);
            idx += consumed;
        }
        // user message
        else if (field == 3 && wire == 2) {
            uint64_t user_len = parse_int(buf + idx, len - idx, &consumed);
            idx += consumed;
            size_t end = idx + user_len;

            // user short name
            while (idx < end) {
                uint64_t key2 = parse_int(buf + idx, end - idx, &consumed);
                idx += consumed;
                uint32_t f2 = key2 >> 3;
                uint32_t w2 = key2 & 0x7;

                if (f2 == 1 && w2 == 2) {
                    uint64_t str_len = parse_int(buf + idx, end - idx, &consumed);
                    idx += consumed;
                    size_t copy_len = str_len <= sn_sz ? str_len : sn_sz - 1;
                    memcpy(out_short_name, buf + idx, copy_len);
                    out_short_name[copy_len] = '\0';
                    idx += str_len;
                    break;
                }
                // skip unknown data (for now)
                else {
                    if (w2 == 0) {
                        parse_int(buf + idx, end - idx, &consumed);
                        idx += consumed;
                    }
                    else if (w2 == 2) {
                        uint64_t skip = parse_int(buf + idx, end - idx, &consumed);
                        idx += consumed + skip;
                    }
                    else if (w2 == 5) { idx += 4; }
                    else if (w2 == 1) { idx += 8; }
                    else { break; }
                }
            }
        }
        // message text
        else if (field == 5 && wire == 2) {
            uint64_t text_len = parse_int(buf + idx, len - idx, &consumed);
            idx += consumed;
            size_t copy = text_len <= MSG_SZ ? text_len : MSG_SZ - 1;
            memcpy(msg, buf + idx, copy);
            msg[copy] = '\0';
            idx += text_len;
        }
        // skip other data
        else {
            if (wire == 0) {
                parse_int(buf + idx, len - idx, &consumed);
                idx += consumed;
            }
            else if (wire == 2) {
                uint64_t skip = parse_int(buf + idx, len - idx, &consumed);
                idx += consumed + skip;
            }
            else if (wire == 5) { idx += 4; }
            else if (wire == 1) { idx += 8; }
            else { break; }
        }
    }
}
