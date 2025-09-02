#include <stdint.h>
#include "config/../io_monitor.h"
#include "schema.h"
static uint8_t s_alarm; // 0 none, 1 moist, 2.. inputs

void AlarmMgr_Task(void){
  // example rule: moisture high
  if(IO_GetMoistPct10() >= g_cfg.io.moist_thr) s_alarm = 1;
  else {
    // check latched inputs if any
  }
}
uint8_t AlarmMgr_GetCode(void){ return s_alarm; }
void    AlarmMgr_Reset(void){ s_alarm = 0; }
