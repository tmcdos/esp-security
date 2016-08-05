// Copyright 2016 by TMCDOS

#include "esp8266.h"
#include "espconn.h"
#include "config.h"
#include "polimex.h"
#include "icon.h"
#include "http_sdk.h"

static struct espconn serverIcon;
static esp_tcp iconTcp;

ip_addr_t dns_host;
cbs_type callback_state;

os_timer_t http_timer;
uint32_t json_stamp; // OS_TIME of the last PUSH timeout
int json_len;
bool json_timeout; // HTTP JSON PUSH has timed out, do not attempt new PUSH next 60 seconds
bool need_200; // HTTP PUSH requires status 200 (only events require it)

char udp_reply[MAX_JSON];

void ICACHE_FLASH_ATTR prepare_json_event(icon_node *dev)
{
  static char stamp[22];

  stamp[0] = (dev->event.year >> 4)+'0';
  stamp[1] = (dev->event.year & 15)+'0';
  stamp[2] = '-';
  stamp[3] = (dev->event.month >> 4)+'0';
  stamp[4] = (dev->event.month & 15)+'0';
  stamp[5] = '-';
  stamp[6] = (dev->event.date >> 4)+'0';
  stamp[7] = (dev->event.date & 15)+'0';
  stamp[8] = '+';
  stamp[9] = (dev->event.hour >> 4)+'0';
  stamp[10] = (dev->event.hour & 15)+'0';
  stamp[11] = '%';
  stamp[12] = '3';
  stamp[13] = 'A';
  stamp[14] = (dev->event.min >> 4)+'0';
  stamp[15] = (dev->event.min & 15)+'0';
  stamp[16] = '%';
  stamp[17] = '3';
  stamp[18] = 'A';
  stamp[19] = (dev->event.sec >> 4)+'0';
  stamp[20] = (dev->event.sec & 15)+'0';
  stamp[21] = 0;
  NODE_DBG("Generating JSON\n");
    json_len = os_sprintf(udp_reply, "%%7B%%22icon_addr%%22%%3A%d%%2C+"
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
      icon_adr, 
      ((dev->inp & 0x7F00) >> 1) | (dev->inp & 0x007F), // digital inputs
      ((dev->outp & 0x7F00) >> 1) | (dev->outp & 0x007F), // digital outputs
      // battery voltage
      (dev->UH >> 4)+'0', (dev->UH & 15)+'0', 
      (dev->UL >> 4)+'0', (dev->UL & 15)+'0',
      // DT - 8 BCD digits (8 bytes)
      (dev->DT[0] & 15)+'0', (dev->DT[1] & 15)+'0', 
      (dev->DT[2] & 15)+'0', (dev->DT[3] & 15)+'0', 
      (dev->DT[4] & 15)+'0', (dev->DT[5] & 15)+'0', 
      (dev->DT[6] & 15)+'0', (dev->DT[7] & 15)+'0',
      // TOS - 5 BCD digits (5 bytes)
      (dev->event.TOS[0] & 15)+'0', 
      (dev->event.TOS[1] & 15)+'0', 
      (dev->event.TOS[2] & 15)+'0', 
      (dev->event.TOS[3] & 15)+'0', 
      (dev->event.TOS[4] & 15)+'0',
      // BOS - 5 BCD digits (5 bytes)
      (dev->event.BOS[0] & 15)+'0', 
      (dev->event.BOS[1] & 15)+'0', 
      (dev->event.BOS[2] & 15)+'0', 
      (dev->event.BOS[3] & 15)+'0', 
      (dev->event.BOS[4] & 15)+'0',
      dev->event.event_nom,
      stamp,
      // Card - 10 BCD digits (10 bytes)
      (dev->event.CARD[0] & 15)+'0', 
      (dev->event.CARD[1] & 15)+'0', 
      (dev->event.CARD[2] & 15)+'0', 
      (dev->event.CARD[3] & 15)+'0', 
      (dev->event.CARD[4] & 15)+'0', 
      (dev->event.CARD[5] & 15)+'0', 
      (dev->event.CARD[6] & 15)+'0', 
      (dev->event.CARD[7] & 15)+'0', 
      (dev->event.CARD[8] & 15)+'0', 
      (dev->event.CARD[9] & 15)+'0', 
      dev->event.reader,
      (dev->event.PIN[0] >> 4)+'0', (dev->event.PIN[0] & 15)+'0', 
      (dev->event.PIN[1] >> 4)+'0', (dev->event.PIN[1] & 15)+'0'
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

}

// HTTP communications timed out
void ICACHE_FLASH_ATTR http_timerfunc(void)
{
  NODE_DBG("HTTP timeout = %d\n",flashConfig.flags & F_SSL_PUSH ? espconn_secure_disconnect(&serverIcon) : espconn_disconnect(&serverIcon));
  json_timeout = true;
  json_stamp = system_get_time();
  if(icon_state == ICS_CMD) icon_send_ping();
    else icon_next_poll();
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
  {
    if(1/*Need_200*/)
    {
      NODE_DBG("HTTP deleting event\n");
      //icon_dev[icon_current].modified = 0;
      // delete event from controller
      //icon_send_read(true);
    }
    else 
    {
      //icon_poll_next();
    }
  }
  //else icon_poll_next();
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
  int len;
  
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  NODE_DBG("Registering HTTP callbacks\n");
  espconn_regist_recvcb(conn, UrlRecvCb);
  espconn_regist_disconcb(conn, UrlDisconCb);
  espconn_regist_reconcb(conn, UrlResetCb);
  espconn_regist_sentcb(conn, UrlSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
  conn->reverse = os_malloc(MAX_JSON);
  len = os_sprintf(conn->reverse, "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: Close\r\nCache-Control: no-cache\r\n\r\n%s", 
    flashConfig.icon_url, flashConfig.icon_host, json_len, udp_reply);
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
    //else icon_poll_next();
  }
  //else icon_poll_next();
}


//===== Initialization

void ICACHE_FLASH_ATTR httpsdkInit(void) 
{
  // JSON callbacks
  serverIcon.type = ESPCONN_TCP;
  serverIcon.state = ESPCONN_NONE;
  serverIcon.proto.tcp = &iconTcp;
  espconn_regist_connectcb(&serverIcon, UrlConnectCb); 
  os_timer_setfn(&http_timer, (os_timer_func_t *)http_timerfunc, NULL); // HTTP timeouts
}