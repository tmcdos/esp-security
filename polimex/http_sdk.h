#ifndef __HTTPSDK_H__
#define __HTTPSDK_H__

#define PUSH_TIMEOUT 4000

// maximum size of buffer for HTTP request - aligned to 32-bit
#define MAX_JSON 1400

extern char udp_reply[MAX_JSON];
extern int json_len;
extern bool json_timeout, need_200, http_running, heart_wait;

void ICACHE_FLASH_ATTR httpsdkInit(void);
void ICACHE_FLASH_ATTR http_timerfunc(void);
void ICACHE_FLASH_ATTR send_json_info(void);
void ICACHE_FLASH_ATTR send_heartbeat(void);
void ICACHE_FLASH_ATTR prepare_json_event(icon_node *dev);

#endif