#ifndef AT_CMD_H
#define AT_CMD_H

#include <stdint.h>

void ATCMD_Init(void);
void ATCMD_Process(void);
void ATCMD_InputChar(uint8_t c);

#endif