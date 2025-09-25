#pragma once
#include <stddef.h>
#include <stdint.h>
void Esp_HandleFrame(const uint8_t* payload, size_t len);


extern volatile bool sms_enabled;
void handle_sms_enable_cmd(uint8_t flag);
