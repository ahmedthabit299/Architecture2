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


const char* phonebook_get_number(uint8_t slot) {
    return (slot < 16 && g_cfg.phonebook.numbers[slot][0] != '\0')
           ? g_cfg.phonebook.numbers[slot]
           : NULL;
}

void phonebook_set_number(uint8_t slot, const char* num) {
    if (slot < 16 && num) {
        strncpy(g_cfg.phonebook.numbers[slot], num, 23);
        g_cfg.phonebook.numbers[slot][23] = '\0';
        Cfg_Save();    // persist to flash
    }
}

void phonebook_delete_number(uint8_t slot) {
    if (slot < 16) {
        g_cfg.phonebook.numbers[slot][0] = '\0';
        Cfg_Save();
    }
}

void phonebook_set_default(uint8_t slot) {
    if (slot < 16) {
        g_cfg.phonebook.default_index = slot;
        Cfg_Save();
    }
}
uint8_t phonebook_get_default(void) { return g_cfg.phonebook.default_index; }
