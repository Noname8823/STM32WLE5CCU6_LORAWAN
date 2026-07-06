#ifndef RS485_H
#define RS485_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
    RS485_STATUS_NONE        = 0x00U,
    RS485_STATUS_OK          = 0x01U,
    RS485_STATUS_TIMEOUT     = 0x02U,
    RS485_STATUS_CRC_ERROR   = 0x03U,
    RS485_STATUS_FRAME_ERROR = 0x04U,
    RS485_STATUS_ERROR       = 0x05U
} RS485_Status_t;

void RS485_Init(void);
void RS485_SetRxMode(void);
void RS485_SetTxMode(void);

RS485_Status_t RS485_Transmit(const uint8_t *data,
                              uint16_t len,
                              uint32_t timeout_ms);

RS485_Status_t RS485_ReadAvailable(uint8_t *buf,
                                    uint16_t max_len,
                                    uint16_t *out_len,
                                    uint32_t byte_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* RS485_H */