#ifndef __ICON_H__
#define __ICON_H__

#include <c_types.h>

// 200msec for the iCON timeout
#define ICON_INTERVAL 200

// buffer for incoming and outgoing iCON messages
#define UART_BUF_LEN 250

// maximum packet for /sdk/cmd.json
#define MAX_DATA UART_BUF_LEN - 7

// maximum number of tracked iCON devices
#define MAX_ICON 32

typedef enum {ICS_IDLE, ICS_SCAN, ICS_EVENT, ICS_DELETE, ICS_CMD} ics_type;

typedef struct  __attribute__((packed))
{
  uint8_t start;
  uint8_t addr;
  uint8_t cmd;
  uint8_t hw_type[2], sernum[4], sw_ver[3], num_in[3], num_out[3], num_reader;
  uint8_t ts[2], io_tab[2], sot, mode, max_card[5], max_event[5];
  uint8_t err;
  uint8_t crc[3];
  uint8_t end;
} scan_result;

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
  scan_result ans_scan;
  event_empty ans_empty;
  event_733 ans_event;
} icon_input;

typedef struct icon_node_str
{
  uint8_t adr, skip, num_timeout, priority, def_priority;
  bool must_delete; // an event is pending to be deleted
  scan_result ctrl_info;
  uint16_t inp,outp; // digital inputs, outputs
  uint8_t UH,UL; // battery voltage
  uint8_t DT[8]; // BCD counter
  uint8_t err;
  icon_event_format event;
} icon_node;

extern ics_type icon_state;
extern uint8_t icon_start_adr, icon_stop_adr, icon_scan_adr, icon_count, icon_current, icon_adr, icon_cmd;
extern uint8_t saved_adr, saved_cmd;
//extern struct icon_node icon_dev[MAX_ICON];
extern int icon_data_len;
extern char icon_data[MAX_DATA];
extern os_timer_t icon_timer;
extern bool cmd_wait;

uint8 ICACHE_FLASH_ATTR calc_crc(const char *buf, int len);
void ICACHE_FLASH_ATTR icon_recv(char *buf, int length);
void ICACHE_FLASH_ATTR icon_scan_next(void);
void ICACHE_FLASH_ATTR icon_send_scan(void);
void ICACHE_FLASH_ATTR icon_start_poll(void);
void ICACHE_FLASH_ATTR icon_next_poll(void);
void ICACHE_FLASH_ATTR icon_send_ping(void);
void ICACHE_FLASH_ATTR icon_send_cmd(void);
void ICACHE_FLASH_ATTR icon_must_delete(void);
void ICACHE_FLASH_ATTR icon_timerfunc(void);

#endif