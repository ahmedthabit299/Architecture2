#include "schema.h"
#include <string.h>
// TODO: implement flash read/write; simple header {magic,version,len,crc}
DeviceCfg g_cfg;

void Cfg_Defaults(void){
  memset(&g_cfg,0,sizeof(g_cfg));
  g_cfg.version = 0x00010100; // v1.1.0
}
void Cfg_Load(void){ /* read flash; if bad -> defaults */ }
void Cfg_Save(void){ /* write flash */ }
int  Cfg_Validate(void){ return 0; }
