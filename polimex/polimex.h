#ifndef __POLIMEX_H__
#define __POLIMEX_H__

#include <c_types.h>

extern uint8_t bridge_active;
extern uint32_t heart_start;

void ICACHE_FLASH_ATTR polimexInit(void);
void ICACHE_FLASH_ATTR start_heartbeat(void);
                           
#endif