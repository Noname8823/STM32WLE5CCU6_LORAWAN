#include "app_config.h"
#include "stm32wlxx_hal.h"
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

AppConfig_t g_app_config;

static uint32_t AppConfig_CalcChecksum(const AppConfig_t *cfg)
{
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t len = offsetof(AppConfig_t, checksum);
    uint32_t hash = 2166136261UL;

    for (uint32_t i = 0; i < len; i++)
    {
        hash ^= p[i];
        hash *= 16777619UL;
    }

    return hash;
}

void AppConfig_SetDefaults(void)
{
    memset(&g_app_config, 0, sizeof(g_app_config));

    g_app_config.magic = APP_CONFIG_MAGIC;
    g_app_config.version = APP_CONFIG_VERSION;

    /*
     * Mặc định dùng UID chip làm DevEUI.
     */
    g_app_config.use_custom_dev_eui = 0;

    /*
     * JoinEUI/AppEUI mặc định = 0000000000000000
     */
    memset(g_app_config.join_eui, 0x00, sizeof(g_app_config.join_eui));

    /*
     * AppKey test = ASCII "TRANQUOCVU080803"
     * HEX: 54 52 41 4E 51 55 4F 43 56 55 30 38 30 38 30 33
     */
    const uint8_t default_key[16] =
    {
        0x54, 0x52, 0x41, 0x4E,
        0x51, 0x55, 0x4F, 0x43,
        0x56, 0x55, 0x30, 0x38,
        0x30, 0x38, 0x30, 0x33
    };

    memcpy(g_app_config.app_key, default_key, 16);
    memcpy(g_app_config.nwk_key, default_key, 16);

    g_app_config.checksum = AppConfig_CalcChecksum(&g_app_config);
}

bool AppConfig_IsValid(const AppConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return false;
    }

    if (cfg->magic != APP_CONFIG_MAGIC)
    {
        return false;
    }

    if (cfg->version != APP_CONFIG_VERSION)
    {
        return false;
    }

    if (cfg->checksum != AppConfig_CalcChecksum(cfg))
    {
        return false;
    }

    return true;
}

bool AppConfig_Load(void)
{
    const AppConfig_t *flash_cfg = (const AppConfig_t *)APP_CONFIG_FLASH_ADDR;

    memcpy(&g_app_config, flash_cfg, sizeof(AppConfig_t));

    if (AppConfig_IsValid(&g_app_config) == false)
    {
        AppConfig_SetDefaults();
        return false;
    }

    return true;
}

static bool AppConfig_ErasePage(void)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    memset(&erase_init, 0, sizeof(erase_init));

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = (APP_CONFIG_FLASH_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase_init.NbPages = 1;

    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
    {
        return false;
    }

    return true;
}

bool AppConfig_Save(const AppConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return false;
    }

    AppConfig_t temp;
    memcpy(&temp, cfg, sizeof(AppConfig_t));

    temp.magic = APP_CONFIG_MAGIC;
    temp.version = APP_CONFIG_VERSION;
    temp.checksum = AppConfig_CalcChecksum(&temp);

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    if (AppConfig_ErasePage() == false)
    {
        HAL_FLASH_Lock();
        return false;
    }

    const uint8_t *src = (const uint8_t *)&temp;
    uint32_t size = sizeof(AppConfig_t);
    uint32_t addr = APP_CONFIG_FLASH_ADDR;

    for (uint32_t i = 0; i < size; i += 8)
    {
        uint64_t data64 = 0xFFFFFFFFFFFFFFFFULL;
        uint32_t remain = size - i;

        if (remain >= 8)
        {
            memcpy(&data64, &src[i], 8);
        }
        else
        {
            memcpy(&data64, &src[i], remain);
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, data64) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();

    memcpy(&g_app_config, &temp, sizeof(AppConfig_t));

    return true;
}

bool AppConfig_ResetFlash(void)
{
    bool ret;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    ret = AppConfig_ErasePage();

    HAL_FLASH_Lock();

    AppConfig_SetDefaults();

    return ret;
}
