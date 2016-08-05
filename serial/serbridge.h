#ifndef __SER_BRIDGE_H__
#define __SER_BRIDGE_H__

#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

#define MAX_CONN 1
#define SER_BRIDGE_TIMEOUT 300 // 300 seconds = 5 minutes

// Send buffer size
#define MAX_TXBUFFER 1024

typedef struct serbridgeConnData serbridgeConnData;

struct serbridgeConnData 
{
	struct espconn *conn;
	char           *txbuffer;     // buffer for the data to send
	uint16         txbufferlen;   // length of data in txbuffer
  char           *sentbuffer;   // buffer sent, awaiting callback to get freed
  uint32_t       txoverflow_at; // when the transmitter started to overflow
	bool           readytosend;   // true, if txbuffer can be sent by espconn_sent
};

void ICACHE_FLASH_ATTR serbridgeInit(int port);
void ICACHE_FLASH_ATTR serbridgeInitPins(void);
void ICACHE_FLASH_ATTR serbridgeUartCb(char *buf, int len);
void ICACHE_FLASH_ATTR serbridgeReset();
                           
#endif /* __SER_BRIDGE_H__ */
