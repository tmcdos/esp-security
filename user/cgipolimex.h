#ifndef CGIPOLIMEX_H
#define CGIPOLIMEX_H

#include "httpd.h"

int ICACHE_FLASH_ATTR cgiConScanStart(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConScanStop(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConScanStatus(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConDeviceList(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConUrl(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConSetUrl(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiConGetUrl(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexConfig(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexWebVer(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexIOstat(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexStatus(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexCmd(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexDetail(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexOut(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexSerial(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiPolimexKey(HttpdConnData *connData);

#endif
