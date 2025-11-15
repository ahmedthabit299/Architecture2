#ifndef FLASH_W25Q32_H
#define FLASH_W25Q32_H

#include <stddef.h>
#include <stdint.h>

// Initialize the SPI flash (chip-select pin etc.).
void Flash_Init(void);

// Read `len` bytes starting at `addr` into `buf`.
void Flash_Read(uint32_t addr, uint8_t *buf, size_t len);

// Erase a 4?KB sector containing `addr`.
void Flash_EraseSector(uint32_t addr);

// Program (write) `len` bytes from `buf` starting at `addr`.
// Handles page alignment (W25Q32 pages are 256 bytes).
void Flash_Write(uint32_t addr, const uint8_t *buf, size_t len);

// Compute CRC32 on a buffer (can also be placed in store.c if preferred).
uint32_t Flash_CRC32(const void *data, size_t len);

#endif /* FLASH_W25Q32_H */
