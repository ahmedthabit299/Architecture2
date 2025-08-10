// esp32_comm.c
#include "peripheral/uart/plib_uart1.h"
#include "esp32_comm.h"

#define RX_BUF_MAX     512
#define PAYLOAD_MAX    256
#define IDLE_TIMEOUT_MS 20

static volatile bool rx_flag = false;
static ESP32_FrameHandler app_cb = 0;

static uint16_t crc16_ccitt(const uint8_t* d, size_t n){
    uint16_t c=0xFFFF;
    while(n--){ c ^= (uint16_t)(*d++)<<8;
        for(int i=0;i<8;i++) c=(c&0x8000)?(c<<1)^0x1021:(c<<1);
    } return c;
}
/* ISR callback: set a flag only */
static void uart1_rx_cb(UART_EVENT ev, uintptr_t ctx){
    if (ev==UART_EVENT_READ_THRESHOLD_REACHED || ev==UART_EVENT_READ_BUFFER_FULL || ev==UART_EVENT_READ_ERROR)
        rx_flag = true;
}

void ESP32_UartInit(void){
    UART1_ReadThresholdSet(1);
    UART1_ReadNotificationEnable(true, true);
    UART1_ReadCallbackRegister(uart1_rx_cb, 0);
}
//
//bool ESP32_TakeRxFlag(void)
//{
//    bool v = esp_rx_ready;
//    esp_rx_ready = false;
//    return v;
//}



/* Parser state */
static enum { RX_LEN0, RX_LEN1, RX_PAY, RX_CRC0, RX_CRC1 } st = RX_LEN0;
static uint16_t want_len=0, got=0, rx_crc=0;
static uint8_t  frame[PAYLOAD_MAX];
static uint32_t last_ms=0;

/* Call from Esp32Thread when rx_flag is set (or periodically) */
void ESP32_Poll(void){
    if (!rx_flag && st==RX_LEN0) return;  // nothing pending
    rx_flag = false;

    uint8_t chunk[64];
    size_t n = UART1_Read(chunk, sizeof chunk);
    extern volatile uint32_t msTicks;

    for (size_t i=0;i<n;i++){
        uint8_t b = chunk[i];
        last_ms = msTicks;

        switch (st){
        case RX_LEN0: want_len = b; st=RX_LEN1; break;
        case RX_LEN1:
            want_len |= (uint16_t)b<<8;
            if (want_len==0 || want_len>PAYLOAD_MAX){ st=RX_LEN0; break; }
            got=0; st=RX_PAY; break;
        case RX_PAY:
            frame[got++] = b;
            if (got==want_len){ st=RX_CRC0; }
            break;
        case RX_CRC0: rx_crc = b; st=RX_CRC1; break;
        case RX_CRC1:
            rx_crc |= (uint16_t)b<<8;
            if (app_cb && rx_crc==crc16_ccitt(frame, want_len))
                app_cb(frame, want_len);  // <-- your application handler
            st=RX_LEN0; break;
        }
    }

    // idle timeout to drop a stuck partial frame
    if (st!=RX_LEN0 && (uint32_t)(msTicks - last_ms) >= IDLE_TIMEOUT_MS)
        st = RX_LEN0;
}

void ESP32_RegisterFrameHandler(ESP32_FrameHandler cb){ app_cb = cb; }

bool ESP32_SendFrame(const uint8_t* p, size_t len){
    if (len==0 || len>PAYLOAD_MAX) return false;
    uint8_t hdr[2] = { (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
    uint16_t c = crc16_ccitt(p, len);
    uint8_t ftr[2] = { (uint8_t)(c & 0xFF), (uint8_t)(c >> 8) };

    size_t n = 0;
    n += UART1_Write(hdr, sizeof hdr);
    n += UART1_Write((uint8_t*)p, len);
    n += UART1_Write(ftr, sizeof ftr);
    return n == (2 + len + 2);
}