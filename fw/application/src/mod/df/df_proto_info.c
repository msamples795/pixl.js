#include "df_proto_info.h"
#include "df_buffer.h"
#include "utils2.h"

#include "nrf_log.h"
#include "ble_main.h"
#include "version2.h"
#include "bat.h"
#include "settings.h"
#include "boards.h"
#include "mui_canvas.h"
#include "mui_core.h"
#include <string.h>
#include "ble_main.h"

void df_proto_handler_info_get_version(df_event_t *evt) {
    if (evt->type == DF_EVENT_DATA_RECEVIED) {
        df_frame_t out;

        NEW_BUFFER_ZERO(buff, out.data, sizeof(out.data));
        buff_put_string(&buff, version_get_version(version_get()));
        char ble_addr[24];
        ble_get_addr_str(ble_addr);
        buff_put_string(&buff, ble_addr);

        OUT_FRAME_WITH_DATA_0(out, DF_PROTO_CMD_INFO_VERSION_INFO, DF_STATUS_OK, buff_get_size(&buff));

        df_core_send_frame(&out);
    }
}

void df_proto_handler_info_enter_dfu(df_event_t *evt) {
    if (evt->type == DF_EVENT_DATA_RECEVIED) {
        df_frame_t out;
        OUT_FRAME_NO_DATA(out, DF_PROTO_CMD_INFO_ENTER_DFU, DF_STATUS_OK);
        df_core_send_frame(&out);
    } else if (evt->type == DF_EVENT_DATA_TRANSMIT_READY) {
        NRF_LOG_INFO("enter dfu ...");
        ble_disable();
        enter_dfu();
    }
}

/* ---- Pixl Manager additions ---------------------------------------------------------------- */

/* Restart without saving: unlike the Settings menu reboot, settings_save() is not called, so the
 * settings file on flash (for example one just written over BLE) is what the device boots with. */
void df_proto_handler_info_reboot(df_event_t *evt) {
    if (evt->type == DF_EVENT_DATA_RECEVIED) {
        df_frame_t out;
        OUT_FRAME_NO_DATA(out, DF_PROTO_CMD_INFO_REBOOT, DF_STATUS_OK);
        df_core_send_frame(&out);
    } else if (evt->type == DF_EVENT_DATA_TRANSMIT_READY) {
        NRF_LOG_INFO("reboot (no save) ...");
        ble_disable();
        system_reboot();
    }
}

extern chrg_data_t chrg;

void df_proto_handler_info_battery(df_event_t *evt) {
    if (evt->type == DF_EVENT_DATA_RECEVIED) {
        df_frame_t out;
        NEW_BUFFER_ZERO(buff, out.data, sizeof(out.data));
        float v = chrg.voltage;
        uint16_t mv = v > 0 ? (uint16_t)(v * 1000.0f + 0.5f) : 0;
        buff_put_u8(&buff, chrg.level);
        buff_put_u16(&buff, mv);
        buff_put_u8(&buff, get_stats() ? 1 : 0);
        buff_put_u8(&buff, settings_get_data()->bat_mode ? 1 : 0);
        buff_put_u8(&buff, 9);
        OUT_FRAME_WITH_DATA_0(out, DF_PROTO_CMD_INFO_BATTERY, DF_STATUS_OK, buff_get_size(&buff));
        df_core_send_frame(&out);
    }
}

#ifdef OLED_SCREEN
#define PIXL_SCREEN_ROTATION 2
#else
#define PIXL_SCREEN_ROTATION 0
#endif
#define PIXL_SCREEN_BYTES (MUI_SCREEN_SHADOW_SIZE + 4)

static uint8_t m_screen[PIXL_SCREEN_BYTES];
static uint16_t m_screen_off;
static uint16_t m_screen_chunk;

static void screen_send_chunk(void) {
    if (m_screen_off >= PIXL_SCREEN_BYTES) {
        return;
    }
    df_frame_t out;
    uint16_t n = PIXL_SCREEN_BYTES - m_screen_off;
    if (n > sizeof(out.data)) {
        n = sizeof(out.data);
    }
    memcpy(out.data, m_screen + m_screen_off, n);
    m_screen_off += n;
    uint16_t chunk = m_screen_chunk++;
    if (m_screen_off < PIXL_SCREEN_BYTES) {
        chunk |= 0x8000;
    }
    OUT_FRAME_WITH_DATA_CHUNK(out, DF_PROTO_CMD_INFO_SCREEN, DF_STATUS_OK, chunk, n);
    df_core_send_frame(&out);
}

void df_proto_handler_info_screen(df_event_t *evt) {
    if (evt->type == DF_EVENT_DATA_RECEVIED) {
        m_screen[0] = 128;
        m_screen[1] = 64;
        m_screen[2] = PIXL_SCREEN_ROTATION;
        m_screen[3] = 0;
        if (!mui_screen_shadow_copy(m_screen + 4)) {
            memcpy(m_screen + 4, u8g2_GetBufferPtr(&mui()->u8g2), MUI_SCREEN_SHADOW_SIZE);
        }
        m_screen_off = 0;
        m_screen_chunk = 0;
        screen_send_chunk();
    } else if (evt->type == DF_EVENT_DATA_TRANSMIT_READY) {
        screen_send_chunk();
    }
}

const df_cmd_entry_t df_proto_handler_info_entries[] = {
    {DF_PROTO_CMD_INFO_VERSION_INFO, df_proto_handler_info_get_version},
    {DF_PROTO_CMD_INFO_ENTER_DFU, df_proto_handler_info_enter_dfu},
    {DF_PROTO_CMD_INFO_REBOOT, df_proto_handler_info_reboot},
    {DF_PROTO_CMD_INFO_BATTERY, df_proto_handler_info_battery},
    {DF_PROTO_CMD_INFO_SCREEN, df_proto_handler_info_screen},
    {0, NULL}};