#include "flash_w25q32.h"
#include "peripheral/spi/spi_master/plib_spi3_master.h"
#include "peripheral/gpio/plib_gpio.h"

// Use RA9 for the W25Q32 chip-select.
// Define these once and remove any duplicate definitions.
#define FLASH_CS_LOW()  GPIO_RA9_FL_SS3_Clear()
#define FLASH_CS_HIGH() GPIO_RA9_FL_SS3_Set()

#define CMD_WRITE_ENABLE  0x06
#define CMD_PAGE_PROGRAM  0x02
#define CMD_READ_DATA     0x03
#define CMD_SECTOR_ERASE  0x20
#define CMD_READ_STATUS   0x05

static void Flash_WaitBusy(void) {
    uint8_t cmd = CMD_READ_STATUS;
    uint8_t status;
    do {
        FLASH_CS_LOW();
        SPI3_Write(&cmd, 1);  // send status command
        SPI3_Read(&status, 1); // read status byte
        FLASH_CS_HIGH();
    } while (status & 0x01);   // repeat until busy bit clears
}

static void Flash_WriteEnable(void) {
    uint8_t cmd = CMD_WRITE_ENABLE;
    FLASH_CS_LOW();
    SPI3_Write(&cmd, 1);
    FLASH_CS_HIGH();
}

void Flash_Init(void) {
    SPI3_Initialize();
    FLASH_CS_HIGH();
}

void Flash_Read(uint32_t addr, uint8_t *buf, size_t len) {
    uint8_t cmd[4] = { CMD_READ_DATA,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };
    FLASH_CS_LOW();
    SPI3_Write(cmd, 4);
    SPI3_Read(buf, len);
    FLASH_CS_HIGH();
}

void Flash_EraseSector(uint32_t addr) {
    Flash_WriteEnable();
    uint8_t cmd[4] = { CMD_SECTOR_ERASE,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };
    FLASH_CS_LOW();
    SPI3_Write(cmd, 4);
    FLASH_CS_HIGH();
    Flash_WaitBusy();
}

static void Flash_PageProgram(uint32_t addr, const uint8_t *buf, size_t len) {
    Flash_WriteEnable();
    uint8_t cmd[4] = { CMD_PAGE_PROGRAM,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };
    FLASH_CS_LOW();
    SPI3_Write(cmd, 4);
    SPI3_Write((void *)buf, len);  // cast to void* to match API
    FLASH_CS_HIGH();
    Flash_WaitBusy();
}

void Flash_Write(uint32_t addr, const uint8_t *buf, size_t len) {
    while (len) {
        size_t pageOff = addr & 0xFF;
        size_t space   = 256 - pageOff;
        size_t chunk   = (len < space) ? len : space;
        Flash_PageProgram(addr, buf, chunk);
        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
}

uint32_t Flash_CRC32(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t*)data;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return crc ^ 0xFFFFFFFF;
}
