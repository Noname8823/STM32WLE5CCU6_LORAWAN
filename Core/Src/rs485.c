#include "rs485.h"
#include "usart.h"

/*
 * Theo CubeMX:
 * USART2_TX = PA2
 * USART2_RX = PA3
 * USART2_DR = PA4
 *
 * USART2_DR dùng điều khiển hướng MAX3485.
 *
 * Nếu DE và /RE của MAX3485 nối chung:
 * PA4 = 0 -> Receive mode
 * PA4 = 1 -> Transmit mode
 */

#define RS485_UART_HANDLE huart2

static void RS485_WaitTxComplete(uint32_t timeout_ms)
{
    uint32_t tick_start = HAL_GetTick();

    while (__HAL_UART_GET_FLAG(&RS485_UART_HANDLE, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - tick_start) >= timeout_ms)
        {
            break;
        }
    }
}

void RS485_Init(void)
{
    RS485_SetRxMode();
}

void RS485_SetRxMode(void)
{
    HAL_GPIO_WritePin(USART2_DR_GPIO_Port, USART2_DR_Pin, GPIO_PIN_RESET);
}

void RS485_SetTxMode(void)
{
    HAL_GPIO_WritePin(USART2_DR_GPIO_Port, USART2_DR_Pin, GPIO_PIN_SET);
}

RS485_Status_t RS485_Transmit(const uint8_t *data,
                              uint16_t len,
                              uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;

    if ((data == NULL) || (len == 0U))
    {
        return RS485_STATUS_ERROR;
    }

    RS485_SetTxMode();

    /*
     * Delay nhỏ để MAX3485 kịp chuyển sang truyền.
     * Test ban đầu để 1 ms cho an toàn.
     */
    HAL_Delay(1);

    hal_status = HAL_UART_Transmit(&RS485_UART_HANDLE,
                                   (uint8_t *)data,
                                   len,
                                   timeout_ms);

    RS485_WaitTxComplete(timeout_ms);

    RS485_SetRxMode();

    if (hal_status == HAL_OK)
    {
        return RS485_STATUS_OK;
    }

    if (hal_status == HAL_TIMEOUT)
    {
        return RS485_STATUS_TIMEOUT;
    }

    return RS485_STATUS_ERROR;
}

RS485_Status_t RS485_ReadAvailable(uint8_t *buf,
                                    uint16_t max_len,
                                    uint16_t *out_len,
                                    uint32_t byte_timeout_ms)
{
    uint16_t count = 0U;
    uint8_t rx_byte;
    HAL_StatusTypeDef hal_status;

    if ((buf == NULL) || (out_len == NULL) || (max_len == 0U))
    {
        return RS485_STATUS_ERROR;
    }

    RS485_SetRxMode();

    while (count < max_len)
    {
        hal_status = HAL_UART_Receive(&RS485_UART_HANDLE,
                                      &rx_byte,
                                      1U,
                                      byte_timeout_ms);

        if (hal_status == HAL_OK)
        {
            buf[count] = rx_byte;
            count++;
        }
        else
        {
            break;
        }
    }

    *out_len = count;

    if (count > 0U)
    {
        return RS485_STATUS_OK;
    }

    return RS485_STATUS_NONE;
}
