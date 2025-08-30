#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "device.h"                             // for LATx/TRISx if you use LED
#include "esp32_comm.h"                         // ESP32_SendFrame(...)
#include "peripheral/uart/plib_uart1.h"         // UART1_SerialSetup, etc.
#include "peripheral/uart/plib_uart_common.h"   // UART_SERIAL_SETUP

// extern time base (from your Timer1 tick)
extern volatile uint32_t msTicks;

/* -------------------- Protocol definitions -------------------- */
enum {
    OPC_GET = 0x01,
    OPC_SET = 0x02,
    OPC_PING = 0x03,
    OPC_ECHO = 0x04,
};

enum {
    ST_OK = 0x00,
    ST_BAD_LEN = 0x01,
    ST_UNKNOWN_OP = 0x02,
    ST_BAD_TLV = 0x03,
    ST_APPLY_FAIL = 0x04,
};

enum {
    T_FW_VERSION = 0x01, // string
    T_UPTIME_MS = 0x02, // u32 LE
    T_UART1_BAUD = 0x11, // u32 LE
    T_LED_STATE = 0x10, // u8  (0=off,1=on)
};

static inline uint32_t PBCLK_Hz(void)
{
    return UART1_FrequencyGet()/2;   // <-- change here if you change clocks
}

/* Actual UART1 baud from hardware registers */
uint32_t uart1_calc_baud(void)
{
    const uint32_t pbclk = PBCLK_Hz();
    const uint32_t brgh  = (U1MODE & _U1MODE_BRGH_MASK) ? 1u : 0u;
    const uint32_t div   = brgh ? 4u : 16u;          // BRGH=1 ? 4x, BRGH=0 ? 16x
    const uint32_t brg   = (uint32_t)U1BRG;          // BRG register

    /* baud = PBCLK / (div * (BRG + 1)) */
    return pbclk / (div * (brg + 1u));
}

/* Optional: percentage error vs a target baud (e.g. 115200) *10000 = 0.01% units */
int32_t uart1_baud_err_ppm(uint32_t target, int32_t *percent_x100)
{
    uint32_t actual = uart1_calc_baud();
    if (percent_x100)
    {
        // percent in 0.01% units (2 decimal places)
        int64_t num = ((int64_t)actual - (int64_t)target) * 10000;
        *percent_x100 = (int32_t)(num / (int64_t)target);
    }
    return (int32_t)actual;
}

/* -------------------- Small hardware helpers -------------------- */
static inline void led_set(bool on) {
    // RA15 example (adjust to your board/BSP)
    ANSELACLR = (1u << 15);
    TRISACLR = (1u << 15);
    if (on) LATASET = (1u << 15);
    else LATACLR = (1u << 15);
}

static inline uint32_t get_pbclk_hz(void) {
    // If your PBCLK is fixed/known, put it here (or use UART1_FrequencyGet()).
    return UART1_FrequencyGet();
}

/* -------------------- TLV builder helpers -------------------- */
static inline bool put_u32le(uint8_t* b, size_t cap, size_t* idx, uint32_t v) {
    if (*idx + 4 > cap) return false;
    b[(*idx)++] = (uint8_t) (v & 0xFF);
    b[(*idx)++] = (uint8_t) ((v >> 8) & 0xFF);
    b[(*idx)++] = (uint8_t) ((v >> 16)& 0xFF);
    b[(*idx)++] = (uint8_t) ((v >> 24)& 0xFF);
    return true;
}

static inline bool put_tlv(uint8_t* b, size_t cap, size_t* idx, uint8_t t, const void* v, uint8_t l) {
    if (*idx + 2u + l > cap) return false;
    b[(*idx)++] = t;
    b[(*idx)++] = l;
    if (l) memcpy(&b[*idx], v, l);
    *idx += l;
    return true;
}

static inline bool put_tlv_u32(uint8_t* b, size_t cap, size_t* idx, uint8_t t, uint32_t v) {
    if (*idx + 2 + 4 > cap) return false;
    b[(*idx)++] = t;
    b[(*idx)++] = 4;
    return put_u32le(b, cap, idx, v);
}

/* -------------------- GET handlers -------------------- */
static bool tlv_get_reply(uint8_t tag, uint8_t* out, size_t cap, size_t* idx) {
    switch (tag) {
        case T_FW_VERSION:
        {
            static const char fw[] = "PIC32MM-APP 1.0";
            return put_tlv(out, cap, idx, T_FW_VERSION, fw, (uint8_t) strlen(fw));
        }
        case T_UPTIME_MS:
        {
            return put_tlv_u32(out, cap, idx, T_UPTIME_MS, (uint32_t) msTicks);
        }
        case T_UART1_BAUD:
        {
            uint32_t baud = uart1_calc_baud();
            return put_tlv_u32(out, cap, idx, T_UART1_BAUD, baud);
        }

        case T_LED_STATE:
        {
            // Can't read back LAT state reliably if pin is input; we know we drive it as output:
            bool on = (LATA & (1u << 15)) != 0;
            uint8_t v = on ? 1u : 0u;
            return put_tlv(out, cap, idx, T_LED_STATE, &v, 1);
        }
        default:
            // Unknown tag ? ignore (not an error for GET)
            return true;
    }
}

/* -------------------- SET handlers -------------------- */
static bool tlv_apply(const uint8_t* p, size_t plen, uint8_t* st_out) {
    if (plen < 2) {
        *st_out = ST_BAD_TLV;
        return false;
    }
    uint8_t tag = p[0];
    uint8_t len = p[1];
    if (plen < (size_t) (2 + len)) {
        *st_out = ST_BAD_TLV;
        return false;
    }

    switch (tag) {
        case T_LED_STATE:
            if (len != 1) {
                *st_out = ST_BAD_TLV;
                return false;
            }
            led_set(p[2] ? true : false);
            *st_out = ST_OK;
            return true;

        case T_UART1_BAUD:
            if (len != 4) {
                *st_out = ST_BAD_TLV;
                return false;
            }
        {
            uint32_t baud = (uint32_t) p[2] |
                    ((uint32_t) p[3] << 8) |
                    ((uint32_t) p[4] << 16) |
                    ((uint32_t) p[5] << 24);
            if (baud < 1200 || baud > 921600) {
                *st_out = ST_APPLY_FAIL;
                return false;
            }

            UART_SERIAL_SETUP s = {
                .baudRate = baud,
                .dataWidth = UART_DATA_8_BIT,
                .parity = UART_PARITY_NONE,
                .stopBits = UART_STOP_1_BIT
            };
            bool ok = UART1_SerialSetup(&s, get_pbclk_hz());
            *st_out = ok ? ST_OK : ST_APPLY_FAIL;
            return ok;
        }

        default:
            // Unknown tag in SET ? treat as error
            *st_out = ST_BAD_TLV;
            return false;
    }
}

/* -------------------- Public entry: handle one incoming frame -------------------- */
void Esp_HandleFrame(const uint8_t* payload, size_t len) {
    if (len == 0) return;

    const uint8_t op = payload[0];
    const uint8_t* q = payload + 1;
    size_t rem = (len > 0) ? (len - 1) : 0;

    // Response buffer
    uint8_t rsp[320];
    size_t ri = 0;

    uint8_t status = ST_OK;
    uint8_t rop = op | 0x80; // reply opcode

    // Reserve space for header now; fill later
    // rsp = [ROP][STATUS][TLVs...]
    if (ri + 2 > sizeof (rsp)) return;
    rsp[ri++] = rop;
    rsp[ri++] = ST_OK; // placeholder

    switch (op) {
        case OPC_PING:
            // No TLVs; just OK
            break;

        case OPC_ECHO:
            // Echo the payload (q..end) back as a TLV for convenience
            if (rem <= 255) {
                if (!put_tlv(rsp, sizeof (rsp), &ri, 0xEE, q, (uint8_t) rem)) status = ST_BAD_LEN;
            } else {
                status = ST_BAD_LEN;
            }
            break;

        case OPC_SET:
        {
            uint8_t last_led = 0xFF;
            while (rem >= 2) {
                uint8_t l = q[1];
                if (rem < (size_t) (2 + l)) {
                    status = ST_BAD_TLV;
                    break;
                }

                if (q[0] == T_LED_STATE && l == 1) last_led = q[2];

                uint8_t st_each = ST_OK;
                bool ok = tlv_apply(q, 2 + l, &st_each);
                if (!ok && status == ST_OK) status = st_each;

                q += 2 + l;
                rem -= 2 + l;
            }
            if (status == ST_OK && last_led != 0xFF)
                put_tlv(rsp, sizeof rsp, &ri, 0xA0, &last_led, 1); // ACK(ON/OFF)
            break;
        }
        case OPC_GET:
        {
            while (rem >= 2) {
                uint8_t tag = q[0];
                uint8_t l = q[1];
                if (rem < (size_t) (2 + l)) {
                    status = ST_BAD_TLV;
                    break;
                }

                if (!tlv_get_reply(tag, rsp, sizeof (rsp), &ri)) {
                    status = ST_BAD_LEN;
                    break;
                }
                q += 2 + l;
                rem -= 2 + l;
            }
            if (rem != 0 && status == ST_OK) status = ST_BAD_TLV;
            break;
        }





        default:
            status = ST_UNKNOWN_OP;
            break;
    }

    // Fill status and send
    rsp[1] = status;
    ESP32_SendFrame(rsp, ri); // enable
    //#if 0  // <- disable framed ACK while testing with RealTerm
    //    ESP32_SendFrame(rsp, ri);
    //#endif
}
