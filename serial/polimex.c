// Copyright 2016 by TMCDOS

#include "esp8266.h"
#include "espconn.h"
#include "gpio.h"

#include "uart.h"
#include "serbridge.h"
#include "config.h"
#include "cgiservices.h" // for flash_maps
#include "polimex.h"

static struct espconn serverIcon, autoBcast;
static esp_tcp iconTcp;
static esp_udp autoUdp;
ics_type icon_state; // state machine position
uint8_t icon_adr; // which controller is currently being asked for data
uint8_t icon_cmd; // what command is currently executing the ICON_ADR controller

uint8_t icon_start_adr; // starting address for iCON-bus scan
uint8_t icon_stop_adr; // ending address for iCON-bus scan
uint8_t icon_scan_adr; // currently being scanned device ID

uint8_t bridge_used; // there is currently active TCP-Bridge
ip_addr_t dns_host;
cbs_type callback_state;
os_timer_t icon_timer, event_timer, http_timer;
uint32_t heart_start; // value of OS_TIME at the moment of heartbeat arming

uint8_t icon_count; // numer of valid elements in ICON_DEV
uint8_t icon_current; // currently polled element in ICON_DEV
struct icon_node icon_dev[MAX_ICON];

uint8_t icon_len; // current amount of symbols in ICON_BUF
icon_input icon_buf;

char udp_reply[MAX_ICON_JSON];


uint8_t ICACHE_FLASH_ATTR calc_crc(const char *buf, int len)
{
  uint8_t crc,i,c;
  int n;
  crc = 0;
  for(n=0; n<len; n++)
  {
    i = (buf[n] ^ crc);
    c = 0;
    if(i & 1)   c^= 0x5e;
    if(i & 2)   c^= 0xbc;
    if(i & 4)   c^= 0x61;
    if(i & 8)   c^= 0xc2;
    if(i & 16)  c^= 0x9d;
    if(i & 32)  c^= 0x23;
    if(i & 64)  c^= 0x46;
    if(i & 128) c^= 0x8c;
    crc = c;
  }
  return crc;
}

void ICACHE_FLASH_ATTR icon_scan_reply(void) 
{
  if(icon_count < MAX_ICON)
  {
    NODE_DBG("Add device at position %d\n",icon_count);
    // add newly discovered iCON device to the list
    memset(&icon_dev[icon_count], 0, sizeof(struct icon_node)); 
    icon_dev[icon_count].adr = icon_adr;
    icon_count++;
  }
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR icon_recv(char *buf, int length) 
{
  uint32_t icon_crc;
  int i;

  i = 0;
  while(i<length && icon_len < UART_BUF_LEN)
  {
    if(buf[i]==0xCB) icon_len = 0;
    icon_buf.uart[icon_len++] = buf[i++];
  }
  // packet is longer than minimal, has Start and Stop, comes from requested Address + Command,
  if(icon_len>=8 && icon_buf.uart[icon_len-1] == 0xCE && icon_buf.uart[0] == 0xCB 
    && icon_buf.uart[1]==icon_adr && icon_buf.uart[2]==icon_cmd)
  {
    os_timer_disarm(&icon_timer); // Disarm iCON timer
    NODE_DBG("UART got %d bytes\n",length);
    // check CRC
    icon_crc = calc_crc(&icon_buf.uart[1], icon_len-2-3); // exclude Start/Stop and CRC itself
    if(icon_crc == icon_buf.uart[icon_len-2] + icon_buf.uart[icon_len-3]*10 + icon_buf.uart[icon_len-4]*100)
    {
      // good CRC -- interpret packet
      NODE_DBG("CRC is good\n");
      switch(icon_state)
      {
        case ICS_SCAN:
          break;
        case ICS_EVENT:
          break;
        case ICS_CMD:
          break;
      }
      if(icon_state == ICS_SCAN)
      {
        if(icon_count < MAX_ICON)
        {
          NODE_DBG("Add device at position %d\n",icon_count);
          // add newly discovered iCON device to the list
          memset(&icon_dev[icon_count], 0, sizeof(struct icon_node)); 
          icon_dev[icon_count].adr = icon_adr;
  	      icon_count++;
  	    }
      }
      else if(icon_state == ICS_EVENT)
      {
        // check for new event
        if(icon_len == sizeof(event_empty))
        {
          NODE_DBG("Empty event received for #%d\n",icon_adr);
          //icon_dev[icon_current].modified = 0;
          // no new events - only update some info variables about controller
          //if(icon_buf.ans_empty.in != icon_dev[icon_current].inp) 
          {
            icon_dev[icon_current].inp = icon_buf.ans_empty.in;
            //icon_dev[icon_current].modified = 1;
          }
          //if(icon_buf.ans_empty.out != icon_dev[icon_current].outp) 
          {
            icon_dev[icon_current].outp = icon_buf.ans_empty.out;
            //icon_dev[icon_current].modified = 1;
          }
          //if(icon_buf.ans_empty.UL != 0 && icon_buf.ans_empty.UH != 0 
          //  && abs(
          //    ((icon_buf.ans_empty.UL & 15) + 10*(icon_buf.ans_empty.UL >> 4) + 100*(icon_buf.ans_empty.UH & 15) + 1000*(icon_buf.ans_empty.UH >> 4)) -
          //    ((icon_dev[icon_current].UL & 15) + 10*(icon_dev[icon_current].UL >> 4) + 100*(icon_dev[icon_current].UH & 15) + 1000*(icon_dev[icon_current].UH >> 4))
          //  ) > 99)
          {
            icon_dev[icon_current].UL = icon_buf.ans_empty.UL;
            icon_dev[icon_current].UH = icon_buf.ans_empty.UH;
            //icon_dev[icon_current].modified = 1;
          }
          for(i=0; i<sizeof(icon_buf.ans_empty.DT); i++)
            //if(icon_buf.ans_empty.DT[i] != icon_dev[icon_current].DT[i]) 
            {
              icon_dev[icon_current].DT[i] = icon_buf.ans_empty.DT[i];
              //icon_dev[icon_current].modified = 1;
            }
          //if(icon_dev[icon_current].modified != 0)
          {
            NODE_DBG("No events - but some other change for #%d\n",icon_adr);
            // send info to callback URL
            //send_json_info();
          }
          //else 
          icon_poll_next();
        }
        else
        {
          NODE_DBG("New event for #%d\n",icon_adr);
          // new event
          memcpy(&icon_dev[icon_current].event, &icon_buf.ans_event.event_data, sizeof(icon_event_format));
          //icon_dev[icon_current].modified = 3;
          // send event to callback URL
          send_json_info();
        }
      }
      else if(icon_state == ICS_DELETE)
      {
        NODE_DBG("Deleted event for #%d\n",icon_adr);
        // event deleted - pull another event (from same device)
        icon_state = ICS_IDLE;
        icon_send_read(false);
      }
    }
    if(icon_state == ICS_SCAN) icon_scan_next();
	}
}

static void ICACHE_FLASH_ATTR BcastRecvCb(void *arg, char *data, unsigned short len) 
{
  struct espconn *conn = arg;
  int size;
	uint8 stmac[6], apmac[6]; 
	unsigned long long u;
	uint32_t t = sntp_start;

  NODE_DBG("Receive callback for UDP\n");
  if (conn == NULL) return;

  // send our info to remote side
	wifi_get_macaddr(STATION_IF, stmac); 
	wifi_get_macaddr(SOFTAP_IF, apmac); 
	if(t==0)
	{
	  u = system_rtc_cali_proc() * system_get_rtc_time();
	  t = (u >> 12) / 1000000; // in seconds
	}
	else t = t - sntp_get_current_timestamp();

  //NODE_DBG("UDP reply = hostname\n");
  size = os_sprintf(udp_reply, "%s\r\n", wifi_station_get_hostname());
  //NODE_DBG("UDP reply = client MAC\n");
  size += os_sprintf(size + udp_reply, "%02X-%02X-%02X-%02X-%02X-%02X\r\n",
    stmac[0],stmac[1],stmac[2],stmac[3],stmac[4],stmac[5]);
  //NODE_DBG("UDP reply = hardware version\n");
  size += os_sprintf(size + udp_reply, "ESP-8266, Flash %s, CPU %dMHz, boot_adr = 0x%X, "
    "boot_ver = %d, uptime = %dd %dh %dm %ds, heap = %dKB, SDK_ver = %s\r\n",
    flash_maps[system_get_flash_size_map()], system_get_cpu_freq(), 
    system_get_userbin_addr(), system_get_boot_version(),
    t/(24*3600), (t/(3600))%24, (t/60)%60, t%60, 
    system_get_free_heap_size()/1024, system_get_sdk_version()
  );
  //NODE_DBG("UDP reply = firmware version\n");
  size += os_sprintf(size + udp_reply, "1.2.%s\r\n", esp_link_build);
  //NODE_DBG("UDP reply = serial\n");
  size += os_sprintf(size + udp_reply, "%02X%02X%02X\r\n", 
    flashConfig.convertor[0], flashConfig.convertor[1], flashConfig.convertor[2]);
  //NODE_DBG("UDP reply = bridge port\n");
  size += os_sprintf(size + udp_reply, "%d\r\n", flashConfig.bridge_port);
  //NODE_DBG("UDP reply is ready to send %d bytes\n",size);
  espconn_sendto(conn, (uint8_t *)udp_reply, size);
}

//======= iCON functions ========

void ICACHE_FLASH_ATTR icon_scan_bus(uint8_t from_adr, uint8_t to_adr)
{
  icon_adr = from_adr;
  icon_stop_adr = to_adr;   
  icon_count = 0;
  icon_state = ICS_SCAN;
  icon_send_scan(icon_adr);   
  NODE_DBG("Bus scan started\n");
}

void ICACHE_FLASH_ATTR icon_wait_event(void)
{
  icon_state = ICS_IDLE;
  icon_current = 0;
  if(icon_count != 0) os_timer_arm(&event_timer, 5*ICON_INTERVAL, 0); // Arm iCON Event timer, 1sec, once
  NODE_DBG("Polling for events\n");
}

// ping a controller
void ICACHE_FLASH_ATTR icon_send_scan(uint8_t adr)
{
  uint8_t crc;

  os_timer_disarm(&event_timer); // Disarm Events timer
  icon_cmd = 0xF0;
  // prepare ICON packet
  icon_buf.uart[0] = 0xCB;
  icon_buf.uart[1] = adr;
  icon_buf.uart[2] = icon_cmd;
  crc = calc_crc(&icon_buf.uart[1], 2);
  icon_buf.uart[3] = (crc / 100);
  icon_buf.uart[4] = ((crc % 100) / 10);
  icon_buf.uart[5] = (crc % 10);
  icon_buf.uart[6] = 0xCE;
  icon_len = 0;
  uart0_tx_buffer(icon_buf.uart, 7);
  os_timer_arm(&icon_timer, ICON_INTERVAL, 0); // Arm iCON timer, 200msec, once
  NODE_DBG("Sent cmd F0 to %d\n",icon_adr);
}

void ICACHE_FLASH_ATTR icon_scan_next(void)
{
  if(icon_adr < 255 || icon_adr == 0xCB || icon_adr == 0xCE) icon_adr++;
  if(icon_adr == 255 || icon_adr > icon_stop_adr)
  {
  NODE_DBG("ICS_SCAN finished\n");
    icon_wait_event();
  }
  else
  {
  NODE_DBG("ICS_SCAN_NEXT %d\n",icon_adr);
    icon_send_scan(icon_adr);
  }
}

// request oldest event - or delete oldest event
void ICACHE_FLASH_ATTR icon_send_read(bool del_event)
{
  uint8_t crc;
  
  icon_adr = icon_dev[icon_current].adr;
  icon_cmd = (del_event ? 0xDA : 0xFA);
  // prepare ICON packet
  icon_buf.uart[0] = 0xCB;
  icon_buf.uart[1] = icon_adr;
  icon_buf.uart[2] = icon_cmd;
  crc = calc_crc(&icon_buf.uart[1], 2);
  icon_buf.uart[3] = (crc / 100);
  icon_buf.uart[4] = ((crc % 100) / 10);
  icon_buf.uart[5] = (crc % 10);
  icon_buf.uart[6] = 0xCE;
  icon_len = 0;
  uart0_tx_buffer(icon_buf.uart, 7);
  icon_state = (del_event ? ICS_DELETE : ICS_EVENT);
  os_timer_arm(&icon_timer, ICON_INTERVAL, 0); // Arm iCON timer, 200msec, once
  NODE_DBG("Sent cmd %02X to %d\n",icon_cmd,icon_adr);
}

// poll events from next controller
void ICACHE_FLASH_ATTR icon_poll_next(void)
{
  icon_state = ICS_IDLE;
  icon_current++;
  if(icon_current >= icon_count) icon_current = 0;
  NODE_DBG("Polling controller %d\n",icon_current);
  //icon_send_read(false);
  os_timer_arm(&event_timer, 5*ICON_INTERVAL, 0); // recall polling after 1 second
}

void ICACHE_FLASH_ATTR send_scan(void)
{
}

// called on receive timeout
void ICACHE_FLASH_ATTR icon_timerfunc(void)
{
  NODE_DBG("iCON bus timeout\n");
  switch(icon_state)
  {
    case ICS_IDLE:
      break;
    case ICS_SCAN:
      {
        // no response - move to next device
        icon_scan_next();
      }
      //  else icon_wait_event(); 
      break;
    case ICS_EVENT:
    case ICS_DELETE:
      // no response - move to next device
      icon_poll_next();
      break;
  }
}

// called periodically to poll iCON events
void ICACHE_FLASH_ATTR event_timerfunc(void)
{
  NODE_DBG("Time to read next event\n");
  if(icon_state == ICS_IDLE) icon_send_read(false); 
}



// HTTP communications timed out
void ICACHE_FLASH_ATTR http_timerfunc(void)
{
  NODE_DBG("HTTP timeout = %d\n",flashConfig.flags & F_SSL_PUSH ? espconn_secure_disconnect(&serverIcon) : espconn_disconnect(&serverIcon));
  if(icon_state == ICS_IDLE) icon_poll_next();
}

// Receive callback
static void ICACHE_FLASH_ATTR UrlRecvCb(void *arg, char *data, unsigned short len)  
{
  char *http_code = data;
  int i = len;
  
  NODE_DBG("HTTP receive %d bytes\n",len);
  if(callback_state == CBU_CLOSE) return;
  while(i-- > 0 && *(http_code++) != ' '); // find first space in "HTTP/1.0 200 OK"
  if(*(http_code++) == '2' && *(http_code++) == '0' && *(http_code++) == '0') 
  {
    callback_state = CBU_OKAY;
    NODE_DBG("HTTP state = OK\n");
  }
  else 
  {
    callback_state = CBU_CLOSE;
    NODE_DBG("HTTP state = CLOSE\n");
  }
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR UrlSentCb(void *arg)  
{
  struct espconn *conn = arg;
  if (conn->reverse != NULL) // actually the chinese mistake for "reserve"
  {
    os_free(conn->reverse);
    conn->reverse = NULL;
  }
  NODE_DBG("HTTP packet sent\n");
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  // wait 8 seconds to get response and connection to be closed from HTTP server
  os_timer_arm(&http_timer, 2*PUSH_TIMEOUT, 0); 
}

// Disconnection callback
static void ICACHE_FLASH_ATTR UrlDisconCb(void *arg)  
{
  struct espconn *conn = arg;
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  if (conn->reverse != NULL) // actually the chinese mistake for "reserve"
  {
    os_free(conn->reverse);
    conn->reverse = NULL;
  }
  NODE_DBG("HTTP end\n");
  if(callback_state == CBU_OKAY)
    if(1/*Need_200*/)
    {
      NODE_DBG("HTTP deleting event\n");
      //icon_dev[icon_current].modified = 0;
      // delete event from controller
      icon_send_read(true);
    }
    else 
    {
      icon_poll_next();
    }
  else icon_poll_next();
  callback_state = CBU_NONE;
}

// Connection reset callback (note that there will be no DisconCb)
static void ICACHE_FLASH_ATTR UrlResetCb(void *arg, sint8 err) 
{
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  NODE_DBG("HTTP conn. reset\n");
  UrlDisconCb(arg);
} 

static void ICACHE_FLASH_ATTR UrlConnectCb(void *arg) 
{ 
  struct espconn *conn = arg; 
  int len, size;
  static char stamp[22];
  
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  NODE_DBG("Registering HTTP callbacks\n");
  espconn_regist_recvcb(conn, UrlRecvCb);
  espconn_regist_disconcb(conn, UrlDisconCb);
  espconn_regist_reconcb(conn, UrlResetCb);
  espconn_regist_sentcb(conn, UrlSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
  NODE_DBG("Preparing timestamp for event\n");
  stamp[0] = (icon_dev[icon_current].event.year >> 4)+'0';
  stamp[1] = (icon_dev[icon_current].event.year & 15)+'0';
  stamp[2] = '-';
  stamp[3] = (icon_dev[icon_current].event.month >> 4)+'0';
  stamp[4] = (icon_dev[icon_current].event.month & 15)+'0';
  stamp[5] = '-';
  stamp[6] = (icon_dev[icon_current].event.date >> 4)+'0';
  stamp[7] = (icon_dev[icon_current].event.date & 15)+'0';
  stamp[8] = '+';
  stamp[9] = (icon_dev[icon_current].event.hour >> 4)+'0';
  stamp[10] = (icon_dev[icon_current].event.hour & 15)+'0';
  stamp[11] = '%';
  stamp[12] = '3';
  stamp[13] = 'A';
  stamp[14] = (icon_dev[icon_current].event.min >> 4)+'0';
  stamp[15] = (icon_dev[icon_current].event.min & 15)+'0';
  stamp[16] = '%';
  stamp[17] = '3';
  stamp[18] = 'A';
  stamp[19] = (icon_dev[icon_current].event.sec >> 4)+'0';
  stamp[20] = (icon_dev[icon_current].event.sec & 15)+'0';
  stamp[21] = 0;
  NODE_DBG("Generating JSON\n");
    size = os_sprintf(udp_reply, "%%7B%%22icon_addr%%22%%3A%d%%2C+"
      "%%22icon_input%%22%%3A%d%%2C+"
      "%%22icon_output%%22%%3A%d%%2C+"
      "%%22icon_volt%%22%%3A%c%c.%c%c%%2C+"
      "%%22icon_dt%%22%%3A%%22%c%c%c%c%c%c%c%c%%22%%2C+"
      "%%22icon_tos%%22%%3A%%22%c%c%c%c%c%%22%%2C+"
      "%%22icon_bos%%22%%3A%%22%c%c%c%c%c%%22%%2C+"
      "%%22event_id%%22%%3A%d%%2C+"
      "%%22timestamp%%22%%3A%%2220%s%%22%%2C+"
      "%%22card_id%%22%%3A%%22%c%c%c%c%c%c%c%c%c%c%%22%%2C+"
      "%%22reader_id%%22%%3A%d%%2C+"
      "%%22pin_code%%22%%3A%%22%c%c%c%c%%22%%7D", 
      icon_dev[icon_current].adr, 
      ((icon_dev[icon_current].inp & 0x7F00) >> 1) | (icon_dev[icon_current].inp & 0x007F), // digital inputs
      ((icon_dev[icon_current].outp & 0x7F00) >> 1) | (icon_dev[icon_current].outp & 0x007F), // digital outputs
      // battery voltage
      (icon_dev[icon_current].UH >> 4)+'0', (icon_dev[icon_current].UH & 15)+'0', 
      (icon_dev[icon_current].UL >> 4)+'0', (icon_dev[icon_current].UL & 15)+'0',
      // DT - 8 BCD digits (8 bytes)
      (icon_dev[icon_current].DT[0] & 15)+'0', (icon_dev[icon_current].DT[1] & 15)+'0', 
      (icon_dev[icon_current].DT[2] & 15)+'0', (icon_dev[icon_current].DT[3] & 15)+'0', 
      (icon_dev[icon_current].DT[4] & 15)+'0', (icon_dev[icon_current].DT[5] & 15)+'0', 
      (icon_dev[icon_current].DT[6] & 15)+'0', (icon_dev[icon_current].DT[7] & 15)+'0',
      // TOS - 5 BCD digits (5 bytes)
      (icon_dev[icon_current].event.TOS[0] & 15)+'0', 
      (icon_dev[icon_current].event.TOS[1] & 15)+'0', 
      (icon_dev[icon_current].event.TOS[2] & 15)+'0', 
      (icon_dev[icon_current].event.TOS[3] & 15)+'0', 
      (icon_dev[icon_current].event.TOS[4] & 15)+'0',
      // BOS - 5 BCD digits (5 bytes)
      (icon_dev[icon_current].event.BOS[0] & 15)+'0', 
      (icon_dev[icon_current].event.BOS[1] & 15)+'0', 
      (icon_dev[icon_current].event.BOS[2] & 15)+'0', 
      (icon_dev[icon_current].event.BOS[3] & 15)+'0', 
      (icon_dev[icon_current].event.BOS[4] & 15)+'0',
      icon_dev[icon_current].event.event_nom,
      stamp,
      // Card - 10 BCD digits (10 bytes)
      (icon_dev[icon_current].event.CARD[0] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[1] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[2] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[3] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[4] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[5] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[6] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[7] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[8] & 15)+'0', 
      (icon_dev[icon_current].event.CARD[9] & 15)+'0', 
      icon_dev[icon_current].event.reader,
      (icon_dev[icon_current].event.PIN[0] >> 4)+'0', (icon_dev[icon_current].event.PIN[0] & 15)+'0', 
      (icon_dev[icon_current].event.PIN[1] >> 4)+'0', (icon_dev[icon_current].event.PIN[1] & 15)+'0'
    );
  /*else
    size = os_sprintf(udp_reply, "%%7B%%22icon_addr%%22%%3A%d%%2C+"
      "%%22icon_input%%22%%3A%d%%2C+"
      "%%22icon_output%%22%%3A%d%%2C+"
      "%%22icon_volt%%22%%3A%c%c.%c%c%%2C+"
      "%%22icon_dt%%22%%3A%%22%c%c%c%c%c%c%c%c%%22%%7D", 
      icon_dev[icon_current].adr, 
      ((icon_dev[icon_current].inp & 0x7F00) >> 1) | (icon_dev[icon_current].inp & 0x007F), // digital inputs
      ((icon_dev[icon_current].outp & 0x7F00) >> 1) | (icon_dev[icon_current].outp & 0x007F), // digital outputs
      // battery voltage
      (icon_dev[icon_current].UH >> 4)+'0', (icon_dev[icon_current].UH & 15)+'0', 
      (icon_dev[icon_current].UL >> 4)+'0', (icon_dev[icon_current].UL & 15)+'0',
      // DT - 8 BCD digits (8 bytes)
      (icon_dev[icon_current].DT[0] & 15)+'0', (icon_dev[icon_current].DT[1] & 15)+'0', 
      (icon_dev[icon_current].DT[2] & 15)+'0', (icon_dev[icon_current].DT[3] & 15)+'0', 
      (icon_dev[icon_current].DT[4] & 15)+'0', (icon_dev[icon_current].DT[5] & 15)+'0', 
      (icon_dev[icon_current].DT[6] & 15)+'0', (icon_dev[icon_current].DT[7] & 15)+'0');*/
  conn->reverse = os_malloc(MAX_ICON_JSON);
  len = os_sprintf(conn->reverse, "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: Close\r\nCache-Control: no-cache\r\n\r\n%s", 
    flashConfig.icon_url, flashConfig.icon_host, size, udp_reply);
  NODE_DBG("JSON is ready\n");
  if(flashConfig.flags & F_SSL_PUSH) espconn_secure_send(conn, (uint8_t*)conn->reverse, len);
    else espconn_sent(conn, (uint8_t*)conn->reverse, len);
  os_timer_arm(&http_timer, PUSH_TIMEOUT, 0); // 4 seconds
  NODE_DBG("HTTP packet sending\n");
  callback_state = CBU_SENT;
}

LOCAL void ICACHE_FLASH_ATTR user_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *conn = (struct espconn *)arg;
  if(ipaddr != NULL && ipaddr->addr != 0)
  {
    os_memcpy(conn->proto.tcp->remote_ip,&ipaddr->addr,4);
    NODE_DBG("DNS connect = %d.%d.%d.%d\n",iconTcp.remote_ip[0],iconTcp.remote_ip[1],iconTcp.remote_ip[2],iconTcp.remote_ip[3]);
    if(flashConfig.flags & F_SSL_PUSH) espconn_secure_connect(conn);
      else espconn_connect(conn);
    os_timer_arm(&http_timer, PUSH_TIMEOUT, 0); // 4 seconds
  }
}

void ICACHE_FLASH_ATTR send_json_info(void)
{
  uint32_t ip;
  struct ip_info ipconfig;
  
  icon_state = ICS_IDLE;
  if(flashConfig.icon_host[0] != 0 && flashConfig.icon_url[0] != 0)
  {
    NODE_DBG("HTTP prepare to connect\n");
    callback_state = CBU_NONE;
    iconTcp.remote_port = flashConfig.url_port;
    iconTcp.local_port = espconn_port();
    serverIcon.reverse = NULL;
    ip = ipaddr_addr(flashConfig.icon_host);
    if(ip != IPADDR_NONE)
    {
      os_memcpy(iconTcp.remote_ip,&ip,4);
      //set up the local IP
      wifi_get_ip_info(STATION_IF, &ipconfig);
      os_memcpy(iconTcp.local_ip, &ipconfig.ip, 4);      
      NODE_DBG("TCP connect = %d.%d.%d.%d\n",iconTcp.remote_ip[0],iconTcp.remote_ip[1],iconTcp.remote_ip[2],iconTcp.remote_ip[3]);
      if(flashConfig.flags & F_SSL_PUSH) espconn_secure_connect(&serverIcon);
        else espconn_connect(&serverIcon);
      os_timer_arm(&http_timer, PUSH_TIMEOUT, 0); // 4 seconds
      NODE_DBG("TCP connect called\n");
    }
    else if(ESPCONN_OK == espconn_gethostbyname(&serverIcon,flashConfig.icon_host,&dns_host,user_dns_found)) NODE_DBG("DNS searching\n");
    else icon_poll_next();
  }
  else icon_poll_next();
}

//===== Initialization

void ICACHE_FLASH_ATTR polimexInit(void) 
{
  // JSON callbacks
  serverIcon.type = ESPCONN_TCP;
  serverIcon.state = ESPCONN_NONE;
  serverIcon.proto.tcp = &iconTcp;
  espconn_regist_connectcb(&serverIcon, UrlConnectCb); 
  // UDP broadcaster for autodetection
  autoBcast.type = ESPCONN_UDP;
  autoBcast.proto.udp = &autoUdp;
  autoUdp.local_port = 30303;
  autoUdp.remote_port = 30303;
  autoUdp.remote_ip[0] = 255;
  autoUdp.remote_ip[1] = 255;
  autoUdp.remote_ip[2] = 255;
  autoUdp.remote_ip[3] = 255;
  if(espconn_create(&autoBcast) == 0) espconn_regist_recvcb(&autoBcast, BcastRecvCb);

	icon_state = ICS_IDLE;
  os_timer_setfn(&icon_timer, (os_timer_func_t *)icon_timerfunc, NULL); // Setup iCON timeout timer
  //os_timer_setfn(&event_timer, (os_timer_func_t *)event_timerfunc, NULL); // Setup iCON Events timer
  os_timer_setfn(&http_timer, (os_timer_func_t *)http_timerfunc, NULL); // HTTP timeouts
  // initiate bus scan
  if(flashConfig.flags & F_AUTOSCAN) icon_scan_bus(0,254);
}
