// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
#include "cgiwifi.h"

// change the wifi state indication
void ICACHE_FLASH_ATTR statusWifiUpdate(uint8_t state) {
  wifiState = state;
} 

//===== Init status stuff

void ICACHE_FLASH_ATTR statusInit(void) 
{
}


