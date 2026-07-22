#pragma once

#include <stdint.h>
#include "usb_cdc.h"

#define CDC_DATA_SZ 64

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CdcStateDisconnected,
    CdcStateConnected,
} CdcState;

typedef enum {
    CdcCtrlLineDTR = (1 << 0),
    CdcCtrlLineRTS = (1 << 1),
} CdcCtrlLine;

typedef struct {
    void (*tx_ep_callback)(void* context); //一次 USB 发送完成(TX endpoint 空了)
    void (*rx_ep_callback)(void* context); // USB 收到数据(RX endpoint 有数据)
    void (*state_callback)(void* context, CdcState state); // USB 连接状态变化
    void (*ctrl_line_callback)(void* context, CdcCtrlLine ctrl_lines); // 控制线变化(DTR/RTS)
    void (*config_callback)(void* context, struct usb_cdc_line_coding* config); // 串口参数变化
} CdcCallbacks;

void furi_hal_cdc_set_callbacks(uint8_t if_num, CdcCallbacks* cb, void* context);

struct usb_cdc_line_coding* furi_hal_cdc_get_port_settings(uint8_t if_num);

uint8_t furi_hal_cdc_get_ctrl_line_state(uint8_t if_num);

void furi_hal_cdc_send(uint8_t if_num, uint8_t* buf, uint16_t len);

int32_t furi_hal_cdc_receive(uint8_t if_num, uint8_t* buf, uint16_t max_len);

#ifdef __cplusplus
}
#endif
