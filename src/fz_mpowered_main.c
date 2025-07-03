// Flipper Zero M-Powered Module App | Copyright (c) 2025, Samm. Licensed under MIT.
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_rtc.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/canvas.h>
#include <gui/modules/menu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include "pb_parser.h"

#define PREAMBLE0      0x00
#define PREAMBLE1      0xA5
#define MAX_MSG_LEN    512
#define SHORT_NAME_SZ  32

#define TX_MSG_LEN (size_t)200

#define LOG_VIS_LINES 2
#define LOG_MAX_ENTRIES 4

// view dispatcher scenes
#define VIEW_ID_MAIN_MENU 0
#define VIEW_ID_MSG_LOG 1
#define VIEW_ID_MSG_SENDER 2
#define VIEW_ID_SETTINGS 3

typedef struct {
    char sender[12];
    char timestamp[16];
    char msg[MSG_SZ];
} ComLog;

static char username[32] = "you";

// static uint8_t encrypt_com_file = 0;
// static char file_encryption_key[22] = "";
static File* current_com_file = NULL;

static ComLog log_entries[LOG_MAX_ENTRIES];
static size_t com_log_scroll = 0;
static uint32_t com_log_count = 0;

static enum { WAIT_P0, WAIT_P1, WAIT_L0, WAIT_L1, WAIT_PAY } parse_state = WAIT_P0;
static uint16_t expected_len = 0;
static uint16_t received = 0;
static uint8_t payload_buf[MAX_MSG_LEN];

static char cmd_buffer[768] = "";
static FuriHalSerialHandle* serial_handle;

static ViewDispatcher* view_dispatcher;
static Menu* main_menu;
static View* com_log_view;
static TextInput* text_input;

static char msg_buffer[TX_MSG_LEN] = "";
static uint8_t save_msg_after_send = 0;

static bool append_file(File* file, void* data, size_t data_size) {
    if (!storage_file_is_open(file)) return false;
    if (!storage_file_seek(file, (uint32_t)storage_file_size(file), true)) return false;
    if (storage_file_write(file, data, data_size) == 0) return false;
    return true;
}

static void add_com_log_entry(const char* timestamp, const char* sender, const char* msg) {
    if (com_log_count < LOG_MAX_ENTRIES) {
        strncpy(log_entries[com_log_count].timestamp, timestamp, sizeof(log_entries[com_log_count].timestamp) - 1);
        log_entries[com_log_count].timestamp[sizeof(log_entries[com_log_count].timestamp) - 1] = '\0';

        strncpy(log_entries[com_log_count].sender, sender, sizeof(log_entries[com_log_count].sender) - 1);
        log_entries[com_log_count].sender[sizeof(log_entries[com_log_count].sender) - 1] = '\0';

        strncpy(log_entries[com_log_count].msg, msg, sizeof(log_entries[com_log_count].msg) - 1);
        log_entries[com_log_count].msg[sizeof(log_entries[com_log_count].msg) - 1] = '\0';

        com_log_count++;
    }
    else {
        memmove(&log_entries[0], &log_entries[1], (LOG_MAX_ENTRIES - 1) * sizeof(ComLog));

        strncpy(log_entries[LOG_MAX_ENTRIES - 1].timestamp, timestamp, sizeof(log_entries[0].timestamp) - 1);
        log_entries[LOG_MAX_ENTRIES - 1].timestamp[sizeof(log_entries[0].timestamp) - 1] = '\0';

        strncpy(log_entries[LOG_MAX_ENTRIES - 1].sender, sender, sizeof(log_entries[0].sender)-1);
        log_entries[LOG_MAX_ENTRIES - 1].sender[sizeof(log_entries[0].sender) - 1] = '\0';

        strncpy(log_entries[LOG_MAX_ENTRIES - 1].msg, msg, sizeof(log_entries[0].msg) - 1);
        log_entries[LOG_MAX_ENTRIES - 1].msg[sizeof(log_entries[0].msg) - 1] = '\0';
    }

    // auto-scroll
    if (com_log_count > LOG_VIS_LINES) {
        com_log_scroll = com_log_count - LOG_VIS_LINES;
    }
    else {
        com_log_scroll = 0;
    }
}

static void uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* ctx) {
    UNUSED(ctx);
    if (!(ev & FuriHalSerialRxEventData)) return;
    uint8_t byte = furi_hal_serial_async_rx(handle);

    switch (parse_state) {
    case WAIT_P0:
        if (byte == PREAMBLE0) parse_state = WAIT_P1;
        break;

    case WAIT_P1:
        if (byte == PREAMBLE1) parse_state = WAIT_L0;
        else parse_state = WAIT_P0;
        break;

    case WAIT_L0:
        expected_len = ((uint16_t)byte) << 8;
        parse_state = WAIT_L1;
        break;

    case WAIT_L1:
        expected_len |= byte;
        if (expected_len == 0 || expected_len > MAX_MSG_LEN) {
            parse_state = WAIT_P0;
        }
        else {
            received = 0;
            parse_state = WAIT_PAY;
        }
        break;

    case WAIT_PAY:
        payload_buf[received++] = byte;
        if (received >= expected_len) {
            uint32_t time;
            uint64_t time2;
            char short_name[SHORT_NAME_SZ];
            char msg[MSG_SZ + 1];
            char time_str[32] = "";
            pb_parse_endpoint(payload_buf, expected_len, &time2, short_name, sizeof(short_name), msg);
            time = (uint32_t)time2;


            DateTime dt;
            datetime_timestamp_to_datetime(time, &dt);
            snprintf(time_str, sizeof(time_str), "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day, (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
            char combined[272];
            if (snprintf(combined, sizeof(combined), "%s\n%s\n%s\n\n", time_str, short_name, msg) > 0)
                append_file(current_com_file, combined, strlen(combined));

            add_com_log_entry(time_str, short_name, msg);
            view_commit_model(com_log_view, true);

            parse_state = WAIT_P0;
        }
        break;
    }
}

static void on_change_scene_btn(void* ctx, uint32_t scene) {
    UNUSED(ctx);
    view_dispatcher_switch_to_view(view_dispatcher, scene);
}

static void text_entered_callback(void* ctx) {
    UNUSED(ctx);
    if (strlen(msg_buffer) == 0) {
        view_dispatcher_switch_to_view(view_dispatcher, VIEW_ID_MAIN_MENU);
        return;
    }

    cmd_buffer[0] = '\0';
    snprintf(cmd_buffer, sizeof(cmd_buffer), "{\"type\":\"sendtext\",\"payload\":\"%s\"}\n", msg_buffer);

    furi_hal_serial_tx(serial_handle, (const uint8_t*)cmd_buffer, strlen(cmd_buffer));
    furi_hal_serial_tx_wait_complete(serial_handle);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day, (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
    char combined[272];
    if (snprintf(combined, sizeof(combined), "%s\n%s\n%s\n\n", timestamp_str, username, msg_buffer) > 0)
        append_file(current_com_file, combined, strlen(combined));

    add_com_log_entry(timestamp_str, username, msg_buffer);

    if (!save_msg_after_send) {
        msg_buffer[0] = '\0';
    }
    view_dispatcher_switch_to_view(view_dispatcher, VIEW_ID_MSG_LOG);
}

static void com_log_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    const uint8_t line_h = 12;
    for (size_t i = 0; i < LOG_VIS_LINES; i++) {
        size_t idx = com_log_scroll + i;
        size_t adjusted_i = i * 3;
        if (idx < com_log_count) {
            canvas_draw_str(canvas, 0, (adjusted_i + 1) * line_h, log_entries[idx].timestamp);
            canvas_draw_str(canvas, 24, (adjusted_i + 1) * line_h * 2, log_entries[idx].sender);
            canvas_draw_str(canvas, 0, (adjusted_i + 1) * line_h * 3, log_entries[idx].msg);
        }
    }
}

static bool com_log_input(InputEvent* ev, void* ctx) {
    UNUSED(ctx);
    if (ev->type == InputTypePress) {
        switch (ev->key) {
        case InputKeyRight: {
            view_dispatcher_switch_to_view(view_dispatcher, VIEW_ID_MSG_SENDER);
            return true;
        } break;
        case InputKeyUp: {
            if (com_log_scroll > 0) com_log_scroll--;
            view_commit_model(com_log_view, true);
            return true;
        } break;
        case InputKeyDown: {
            if (com_log_scroll + LOG_VIS_LINES < com_log_count) com_log_scroll++;
            view_commit_model(com_log_view, true);
            return true;
        } break;
        case InputKeyBack: {
            view_dispatcher_switch_to_view(view_dispatcher, VIEW_ID_MAIN_MENU);
            return true;
        } break;
        default: break;
        }
    }
    return false;
}

static void exit_program(void* ctx, uint32_t index) {
    UNUSED(ctx);
    UNUSED(index);
    view_dispatcher_stop(view_dispatcher);
}

int32_t fz_mpowered_main(void* p) {
    UNUSED(p);
    // initialize UART
    serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(serial_handle, 115200);
    furi_hal_serial_configure_framing(serial_handle, FuriHalSerialDataBits8, FuriHalSerialParityNone, FuriHalSerialStopBits1);

    // enable serial transceiving
    furi_hal_serial_enable_direction(serial_handle, FuriHalSerialDirectionRx);
    furi_hal_serial_enable_direction(serial_handle, FuriHalSerialDirectionTx);

    Storage* storage = furi_record_open("storage");

    // open log file
    current_com_file = storage_file_alloc(storage);
    bool ok = storage_file_open(current_com_file, "/ext/mpowered_log.txt", FSAM_READ_WRITE, FSOM_OPEN_ALWAYS);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(view_dispatcher);
    view_dispatcher_attach_to_gui(view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    main_menu = menu_alloc();
    menu_add_item(main_menu, "Message log", NULL, VIEW_ID_MSG_LOG, on_change_scene_btn, NULL);
    menu_add_item(main_menu, "Send message", NULL, VIEW_ID_MSG_SENDER, on_change_scene_btn, NULL);
    // menu_add_item(main_menu, "Settings", NULL, VIEW_ID_SETTINGS, on_change_scene_btn, NULL);
    menu_add_item(main_menu, "EXIT", NULL, 0, exit_program, NULL);
    view_dispatcher_add_view(view_dispatcher, VIEW_ID_MAIN_MENU, menu_get_view(main_menu));

    com_log_view = view_alloc();
    view_set_draw_callback(com_log_view, com_log_draw);
    view_set_input_callback(com_log_view, com_log_input);
    view_dispatcher_add_view(view_dispatcher, VIEW_ID_MSG_LOG, com_log_view);

    text_input = text_input_alloc();
    text_input_set_header_text(text_input, "Send message:");
    text_input_set_result_callback(text_input, text_entered_callback, NULL, msg_buffer, TX_MSG_LEN, true);
    view_dispatcher_add_view(view_dispatcher, VIEW_ID_MSG_SENDER, text_input_get_view(text_input));

    furi_hal_serial_async_rx_start(serial_handle, uart_rx_callback, NULL, true);

    view_dispatcher_switch_to_view(view_dispatcher, VIEW_ID_MAIN_MENU);
    if (ok) view_dispatcher_run(view_dispatcher);

    // cleanup
    view_dispatcher_remove_view(view_dispatcher, VIEW_ID_MAIN_MENU);
    view_dispatcher_remove_view(view_dispatcher, VIEW_ID_MSG_LOG);
    view_dispatcher_remove_view(view_dispatcher, VIEW_ID_MSG_SENDER);
    view_free(com_log_view);
    storage_file_close(current_com_file);
    storage_file_free(current_com_file);
    text_input_free(text_input);
    menu_free(main_menu);
    view_dispatcher_free(view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_hal_serial_async_rx_stop(serial_handle);
    furi_hal_serial_disable_direction(serial_handle, FuriHalSerialDirectionRx);
    furi_hal_serial_disable_direction(serial_handle, FuriHalSerialDirectionTx);
    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);

    return 0;
}
