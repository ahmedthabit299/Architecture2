#include "proto.h"
#include "io_monitor.h"

#include "schema.h"
#include "gsm_le910.h"
#include <string.h>

int Handle_GetStatus(uint8_t* out, int max){
  // write TLVs into out[]; return length
  uint8_t* p=out;
  uint8_t in=IO_GetInputsMask(), outm=IO_GetOutputsMask();
  uint16_t m10=IO_GetMoistPct10();
  int8_t rssi = (int8_t)GSM_GetRSSI();
  // TAG_IN_MASK
  *p++=TAG_IN_MASK; *p++=1; *p++=in;
  *p++=TAG_OUT_MASK; *p++=1; *p++=outm;
  *p++=TAG_MOIST_PCT; *p++=2; *p++=(m10&0xFF); *p++=(m10>>8)&0xFF;
  *p++=TAG_ALARM_CODE; *p++=1; *p++=AlarmMgr_GetCode();
  *p++=TAG_RSSI; *p++=1; *p++=rssi;
  return (int)(p-out);
}

int Handle_ResetAlarm(uint8_t* out, int max){ AlarmMgr_Reset(); return 0; }

int Handle_SetPhonebook(const uint8_t* in, int ilen){
  // parse TLVs: may be multiple SLOT+NUMBER pairs
  int i=0; while(i<ilen){
    uint8_t tag=in[i++], len=in[i++]; const uint8_t* v=&in[i]; i+=len;
    if(tag==TAG_PB_CLEAR && len==1 && v[0]==1){
      memset(&g_cfg.phonebook,0,sizeof(g_cfg.phonebook));
    }else if(tag==TAG_PB_SLOT){
      uint8_t slot=v[0]; // expect next TLV is TAG_PB_NUMBER
      if(i<ilen && in[i]==TAG_PB_NUMBER){
        uint8_t l=in[i+1]; const char* s=(const char*)&in[i+2];
        if(slot>=1 && slot<=16 && l<24){
          memcpy(g_cfg.phonebook.numbers[slot-1], s, l);
          g_cfg.phonebook.numbers[slot-1][l]=0;
        }
        i+=2+l;
      }
    }
  }
  Cfg_Save(); return 0;
}
