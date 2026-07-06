#include "at_cmd.h"
#include "app_config.h"
#include "usart.h"
#include "sys_app.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define ATCMD_LINE_MAX 128U

static volatile uint8_t at_line_ready = 0;
static volatile uint16_t at_line_index = 0;
static char at_line[ATCMD_LINE_MAX];

static int HexNibble(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }

    if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }

    if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }

    return -1;
}

static bool HexToBytes(const char *hex, uint8_t *out, uint32_t out_len)
{
    if ((hex == NULL) || (out == NULL))
    {
        return false;
    }

    if (strlen(hex) != (out_len * 2U))
    {
        return false;
    }

    for (uint32_t i = 0; i < out_len; i++)
    {
        int hi = HexNibble(hex[i * 2U]);
        int lo = HexNibble(hex[i * 2U + 1U]);

        if ((hi < 0) || (lo < 0))
        {
            return false;
        }

        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

static void PrintHex(const char *name, const uint8_t *data, uint32_t len)
{
    APP_LOG(TS_OFF, VLEVEL_ALWAYS, "%s", name);

    for (uint32_t i = 0; i < len; i++)
    {
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "%02X", data[i]);
    }

    APP_LOG(TS_OFF, VLEVEL_ALWAYS, "\r\n");
}

static bool StartsWith(const char *s, const char *prefix)
{
    return (strncmp(s, prefix, strlen(prefix)) == 0);
}

void ATCMD_Init(void)
{
    /*
     * Load config vào RAM để AT command có dữ liệu hiện tại.
     * Nếu Flash chưa có config hợp lệ thì g_app_config sẽ dùng default.
     */
    (void)AppConfig_Load();

    at_line_ready = 0;
    at_line_index = 0;

    /*
     * USART2 đã được project dùng cho log UART.
     * Ta bật RX interrupt từng byte.
     */
    //(void)HAL_UART_Receive_IT(&huart2, &at_rx_byte, 1);

    APP_LOG(TS_OFF, VLEVEL_ALWAYS, "\r\nATCMD READY\r\n");
}

static void ATCMD_HandleLine(char *cmd)
{
    if (strcmp(cmd, "AT") == 0)
    {
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK\r\n");
    }
    else if (strcmp(cmd, "AT+HELP") == 0)
    {
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+INFO?\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+DEVEUI?\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+USEUID=0|1\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+DEVEUI=<16 hex>\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+JOINEUI=<16 hex>\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+APPKEY=<32 hex>\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+NWKKEY=<32 hex>\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+SAVE\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+RESETCFG\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "AT+REBOOT\r\n");
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK\r\n");
    }
    else if (strcmp(cmd, "AT+INFO?") == 0)
    {
        uint8_t uid_dev_eui[8];

        GetUniqueId(uid_dev_eui);

        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "CONFIG: %s\r\n",
                AppConfig_IsValid(&g_app_config) ? "VALID" : "DEFAULT_RAM");

        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "USE_CUSTOM_DEVEUI: %d\r\n",
                g_app_config.use_custom_dev_eui);

        PrintHex("UID_DEVEUI: ", uid_dev_eui, 8);

        if (g_app_config.use_custom_dev_eui != 0U)
        {
            PrintHex("CFG_DEVEUI: ", g_app_config.dev_eui, 8);
        }

        PrintHex("JOINEUI: ", g_app_config.join_eui, 8);

        /*
         * Demo thì có thể in key ra. Làm sản phẩm thật thì nên đổi thành APPKEY: SET.
         */
        PrintHex("APPKEY: ", g_app_config.app_key, 16);
        PrintHex("NWKKEY: ", g_app_config.nwk_key, 16);

        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK\r\n");
    }
    else if (strcmp(cmd, "AT+DEVEUI?") == 0)
    {
        uint8_t dev_eui[8];

        if (g_app_config.use_custom_dev_eui != 0U)
        {
            memcpy(dev_eui, g_app_config.dev_eui, 8);
        }
        else
        {
            GetUniqueId(dev_eui);
        }

        PrintHex("DEVEUI: ", dev_eui, 8);
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK\r\n");
    }
    else if (strcmp(cmd, "AT+USEUID=1") == 0)
    {
        g_app_config.use_custom_dev_eui = 0;
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK USE UID DEVEUI\r\n");
    }
    else if (strcmp(cmd, "AT+USEUID=0") == 0)
    {
        g_app_config.use_custom_dev_eui = 1;
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK USE CUSTOM DEVEUI\r\n");
    }
    else if (StartsWith(cmd, "AT+DEVEUI="))
    {
        const char *hex = cmd + strlen("AT+DEVEUI=");

        if (HexToBytes(hex, g_app_config.dev_eui, 8) == true)
        {
            g_app_config.use_custom_dev_eui = 1;
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK DEVEUI\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR DEVEUI FORMAT\r\n");
        }
    }
    else if (StartsWith(cmd, "AT+JOINEUI="))
    {
        const char *hex = cmd + strlen("AT+JOINEUI=");

        if (HexToBytes(hex, g_app_config.join_eui, 8) == true)
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK JOINEUI\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR JOINEUI FORMAT\r\n");
        }
    }
    else if (StartsWith(cmd, "AT+APPKEY="))
    {
        const char *hex = cmd + strlen("AT+APPKEY=");

        if (HexToBytes(hex, g_app_config.app_key, 16) == true)
        {
            /*
             * LoRaWAN 1.0.x/TTN thường dùng cùng key cho AppKey/NwkKey trong stack ST.
             */
            memcpy(g_app_config.nwk_key, g_app_config.app_key, 16);
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK APPKEY AND NWKKEY\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR APPKEY FORMAT\r\n");
        }
    }
    else if (StartsWith(cmd, "AT+NWKKEY="))
    {
        const char *hex = cmd + strlen("AT+NWKKEY=");

        if (HexToBytes(hex, g_app_config.nwk_key, 16) == true)
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK NWKKEY\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR NWKKEY FORMAT\r\n");
        }
    }
    else if (strcmp(cmd, "AT+SAVE") == 0)
    {
        if (AppConfig_Save(&g_app_config) == true)
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK SAVE, PLEASE REBOOT\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR SAVE\r\n");
        }
    }
    else if (strcmp(cmd, "AT+RESETCFG") == 0)
    {
        if (AppConfig_ResetFlash() == true)
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK RESETCFG, PLEASE REBOOT\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR RESETCFG\r\n");
        }
    }
    else if (strcmp(cmd, "AT+REBOOT") == 0)
    {
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "OK REBOOT\r\n");
        HAL_Delay(100);
        NVIC_SystemReset();
    }
    else
    {
        APP_LOG(TS_OFF, VLEVEL_ALWAYS, "ERROR UNKNOWN CMD\r\n");
    }
}

void ATCMD_Process(void)
{
    char line_copy[ATCMD_LINE_MAX];

    if (at_line_ready == 0U)
    {
        return;
    }

    __disable_irq();

    strncpy(line_copy, at_line, sizeof(line_copy) - 1U);
    line_copy[sizeof(line_copy) - 1U] = '\0';
    at_line_ready = 0U;

    __enable_irq();

    ATCMD_HandleLine(line_copy);
}

void ATCMD_InputChar(uint8_t c)
{
    if ((c == '\r') || (c == '\n'))
    {
        if ((at_line_index > 0U) && (at_line_ready == 0U))
        {
            at_line[at_line_index] = '\0';
            at_line_ready = 1U;
            at_line_index = 0U;
        }
    }
    else
    {
        if ((at_line_ready == 0U) && (at_line_index < (ATCMD_LINE_MAX - 1U)))
        {
            at_line[at_line_index++] = (char)c;
        }
        else
        {
            at_line_index = 0U;
        }
    }
}
