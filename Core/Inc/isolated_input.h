#ifndef ISOLATED_INPUT_H
#define ISOLATED_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * Quy ước board:
 * 0 = có tín hiệu
 * 1 = không có tín hiệu
 */
#define ISOLATED_INPUT_HAS_SIGNAL  0U
#define ISOLATED_INPUT_NO_SIGNAL   1U

typedef struct
{
    uint8_t in1;
    uint8_t in2;
    uint8_t in3;
    uint8_t in4;
    uint8_t raw;
} IsolatedInput_State_t;

void IsolatedInput_Init(void);
uint8_t IsolatedInput_ReadRaw(void);
void IsolatedInput_ReadState(IsolatedInput_State_t *state);

#ifdef __cplusplus
}
#endif

#endif /* ISOLATED_INPUT_H */