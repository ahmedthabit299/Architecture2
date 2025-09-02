#pragma once
#include <stdint.h>

typedef struct {
  char   pin[8];          // optional for SMS admin
  uint8_t reserved;
} GlobalCfg;

typedef struct {
  uint8_t in_no_nc;       // bit0..3: 0=NO 1=NC
  uint8_t in_latch;       // bit0..3: latch flags
  uint16_t moist_thr;     // 0..1000 (tenths %)
} InputCfg;

typedef struct {
  char numbers[16][24];   // E164 or local; zero-terminated
} PhonebookCfg;

typedef struct {
  GlobalCfg   global;
  InputCfg    io;
  PhonebookCfg phonebook;
  uint32_t    crc;
  uint32_t    version;
} DeviceCfg;

extern DeviceCfg g_cfg;
void Cfg_Load(void);
void Cfg_Save(void);
void Cfg_Defaults(void);
int  Cfg_Validate(void);
