#include "isolated_input.h"

/*
 * Theo CubeMX:
 * IN1 = PB5
 * IN2 = PB6
 * IN3 = PB7
 * IN4 = PB8
 *
 * Quy ước:
 * 0 = có tín hiệu
 * 1 = không có tín hiệu
 *
 * bit0 = IN1
 * bit1 = IN2
 * bit2 = IN3
 * bit3 = IN4
 *
 * Chưa có tín hiệu gì:
 * IN1 = 1, IN2 = 1, IN3 = 1, IN4 = 1
 * raw = 0000 1111 = 0x0F
 */

void IsolatedInput_Init(void)
{
    /*
     * GPIO đã được CubeMX init trong MX_GPIO_Init().
     * File này chỉ đọc trạng thái input.
     */
}

uint8_t IsolatedInput_ReadRaw(void)
{
    uint8_t raw = 0U;

    if (HAL_GPIO_ReadPin(IN1_GPIO_Port, IN1_Pin) == GPIO_PIN_SET)
    {
        raw |= (1U << 0);
    }

    if (HAL_GPIO_ReadPin(IN2_GPIO_Port, IN2_Pin) == GPIO_PIN_SET)
    {
        raw |= (1U << 1);
    }

    if (HAL_GPIO_ReadPin(IN3_GPIO_Port, IN3_Pin) == GPIO_PIN_SET)
    {
        raw |= (1U << 2);
    }

    if (HAL_GPIO_ReadPin(IN4_GPIO_Port, IN4_Pin) == GPIO_PIN_SET)
    {
        raw |= (1U << 3);
    }

    return raw & 0x0FU;
}

void IsolatedInput_ReadState(IsolatedInput_State_t *state)
{
    uint8_t raw;

    if (state == NULL)
    {
        return;
    }

    raw = IsolatedInput_ReadRaw();

    state->raw = raw;
    state->in1 = (raw >> 0) & 0x01U;
    state->in2 = (raw >> 1) & 0x01U;
    state->in3 = (raw >> 2) & 0x01U;
    state->in4 = (raw >> 3) & 0x01U;
}
