#ifndef __POLIMEX_H__
#define __POLIMEX_H__

#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

// 200msec for the iCON timeout
#define ICON_INTERVAL 200
#define PUSH_TIMEOUT 4000

// buffer for incoming iCON messages
#define UART_BUF_LEN 80

// maximum number of tracked iCON devices
#define MAX_ICON 32

// maximum size of buffer for HTTP request - aligned to 32-bit
#define MAX_ICON_JSON 1500

typedef enum {ICS_IDLE, ICS_SCAN, ICS_EVENT, ICS_DELETE, ICS_CMD} ics_type;
typedef enum {CBU_NONE, CBU_SENT, CBU_OKAY, CBU_CLOSE} cbs_type;

typedef struct  __attribute__((packed))
{
  uint8_t start;
  uint8_t addr;
  uint8_t cmd;
  uint16_t in;
  uint16_t out;
  uint8_t UH;
  uint8_t UL;
  uint8_t DT[8];
  uint8_t err;
  uint8_t crc[3];
  uint8_t end;
} event_empty;

typedef struct  __attribute__((packed))
{
  uint8_t TOS[5]; // BCD counter
  uint8_t BOS[5]; // BCD counter
  uint8_t event_nom; // BCD event number
  uint8_t sec; // BCD
  uint8_t min; // BCD
  uint8_t hour; // BCD
  uint8_t day; // BCD - day of week
  uint8_t date; // BCD - date of month
  uint8_t month; // BCD
  uint8_t year; // BCD
  uint8_t CARD[10]; // BCD - digits from card number
  uint8_t reader; // BCD - ID of the reader
  uint8_t PIN[2]; // BCD - user PIN code

} icon_event_format;

typedef struct  __attribute__((packed))
{
  uint8_t start;
  uint8_t addr;
  uint8_t cmd;
  icon_event_format event_data;
  uint8_t err;
  uint8_t crc[3];
  uint8_t end;
} event_733;

typedef union __attribute__((packed)) 
{
  char uart[UART_BUF_LEN];
  event_empty ans_empty;
  event_733 ans_event;
} icon_input;


struct icon_node
{
  uint8_t adr;
  uint16_t inp,outp;
  uint8_t UH;
  uint8_t UL;
  uint8_t DT[8]; // BCD counter
  icon_event_format event;
};

extern ics_type icon_state;
extern uint8_t bridge_used, icon_start_adr, icon_stop_adr, icon_scan_adr, icon_count;
extern uint32_t heart_start;
extern struct icon_node icon_dev[MAX_ICON];
char udp_reply[256];

void ICACHE_FLASH_ATTR polimexInit(void);
void ICACHE_FLASH_ATTR send_scan(void);
void ICACHE_FLASH_ATTR icon_recv(char *buf, int length);

uint8 ICACHE_FLASH_ATTR calc_crc(const char *buf, int len);
void ICACHE_FLASH_ATTR icon_timerfunc(void);
void ICACHE_FLASH_ATTR icon_send_scan(uint8_t adr);
void ICACHE_FLASH_ATTR icon_scan_next(void);
void ICACHE_FLASH_ATTR icon_send_read(bool del_event);
void ICACHE_FLASH_ATTR icon_wait_event(void);
void ICACHE_FLASH_ATTR icon_poll_next(void);
void ICACHE_FLASH_ATTR event_timerfunc(void);
void ICACHE_FLASH_ATTR http_timerfunc(void);
void ICACHE_FLASH_ATTR send_json_info(void);
                           
#endif /* __SER_BRIDGE_H__ */
