#ifndef __RELAY_H
#define __RELAY_H

#include "eagle_soc.h" 

#define PLATFORM_GPIO_FLOAT 0
#define PLATFORM_GPIO_PULLUP 1
#define PLATFORM_GPIO_PULLDOWN 2

#define PLATFORM_GPIO_INT 2
#define PLATFORM_GPIO_OUTPUT 1
#define PLATFORM_GPIO_INPUT 0

#define PLATFORM_GPIO_HIGH 1
#define PLATFORM_GPIO_LOW 0 

#define PERIPHS_IO_MUX_PULLDWN          BIT6
#define PERIPHS_IO_MUX_SLEEP_PULLDWN    BIT2
#define PIN_PULLDWN_DIS(PIN_NAME)             CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)
#define PIN_PULLDWN_EN(PIN_NAME)              SET_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)

extern uint8_t relay_status[4]; 
	
int ICACHE_FLASH_ATTR relay_get_state(int relayNumber);
int ICACHE_FLASH_ATTR relay_set_state(int relayNumber,unsigned state);
int ICACHE_FLASH_ATTR relay_toggle_state(int relayNumber);
void ICACHE_FLASH_ATTR relay_init();
#endif