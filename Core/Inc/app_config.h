#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define APP_CONFIG_MAGIC       0xA55A2026UL
#define APP_CONFIG_VERSION     1UL

/*
 * Vùng Flash dùng để lưu config.
 * Chọn gần cuối Flash, tránh vùng LoRaWAN NVM.
 */
#define APP_CONFIG_FLASH_ADDR  0x0803E800UL

typedef struct
{
    uint32_t magic;
    uint32_t version;

    /*
     * 0: DevEUI lấy từ UID chip
     * 1: DevEUI lấy từ dev_eui[] trong config
     */
    uint8_t use_custom_dev_eui;
    uint8_t reserved[3];

    uint8_t dev_eui[8];
    uint8_t join_eui[8];
    uint8_t app_key[16];
    uint8_t nwk_key[16];

    uint32_t checksum;
} AppConfig_t;

extern AppConfig_t g_app_config;

void AppConfig_SetDefaults(void);
bool AppConfig_Load(void);
bool AppConfig_Save(const AppConfig_t *cfg);
bool AppConfig_ResetFlash(void);
bool AppConfig_IsValid(const AppConfig_t *cfg);

#endif