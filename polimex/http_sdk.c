// Copyright 2016 by TMCDOS

#include "esp8266.h"
#include "espconn.h"
#include "config.h"
#include "polimex.h"
#include "icon.h"
#include "http_sdk.h"
#include "cJSON.h"

static struct espconn serverIcon;
static esp_tcp iconTcp;

ip_addr_t dns_host;

os_timer_t http_timer;
uint32_t json_stamp; // OS_TIME of the last PUSH timeout
int json_len;
bool http_running; // there is currently a HTTP PUSH in progress
bool json_timeout; // HTTP JSON PUSH has timed out, do not attempt new PUSH next 60 seconds
bool need_200; // HTTP PUSH requires status 200 (only events require it)
bool heart_wait; // heart beat occured, but there is HTTP in progress - send the beat after this HTTP reply

int udp_len; // used by HTTP reply
char udp_reply[MAX_JSON];

void ICACHE_FLASH_ATTR prepare_json_event(icon_node *dev)
{
  NODE_DBG("Generating JSON\n");
  json_len = os_sprintf(udp_reply, "{\"convertor\": %02X%02X%02X, \"key\":\"%02X%02X\", \"event\":{"
    "\"id\":%d, \"cmd\":%d, \"err\":%d, \"tos\":%c%c%c%c%c, \"bos\":%c%c%c%c%c,"
    "\"event_n\":%d, \"day\":%d, \"reader\":%d,"
    "\"time\":\"%c%c:%c%c:%c%c\","
    "\"date\":\"%c%c-%c%c-%c%c\","
    "\"card\":\"%c%c%c%c%c%c%c%c%c%c\","
    "\"dt\":\"%c%c%c%c\"}}", 
    flashConfig.convertor[0], flashConfig.convertor[1], flashConfig.convertor[2],
    flashConfig.key_data[0], flashConfig.key_data[1], icon_adr, icon_cmd, (dev->err & 15)+'0',
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
    dev->event.day,
    dev->event.reader,
    // time
    (dev->event.hour >> 4)+'0',
    (dev->event.hour & 15)+'0',
    (dev->event.min >> 4)+'0',
    (dev->event.min & 15)+'0',
    (dev->event.sec >> 4)+'0',
    (dev->event.sec & 15)+'0',
    // date
    (dev->event.date >> 4)+'0',
    (dev->event.date & 15)+'0',
    (dev->event.month >> 4)+'0',
    (dev->event.month & 15)+'0',
    (dev->event.year >> 4)+'0',
    (dev->event.year & 15)+'0',
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
    // PIN-code
    (dev->event.PIN[0] >> 4)+'0', 
    (dev->event.PIN[0] & 15)+'0', 
    (dev->event.PIN[1] >> 4)+'0', 
    (dev->event.PIN[1] & 15)+'0'
    /*
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
    */
  );
}

void ICACHE_FLASH_ATTR http_end(void)
{
  if(icon_state == ICS_CMD) icon_send_ping();
    else icon_next_poll();
}

// HTTP communications timed out
void ICACHE_FLASH_ATTR http_timerfunc(void)
{
  NODE_DBG("HTTP timeout\n");
  espconn_abort(&serverIcon);
  //flashConfig.flags & F_SSL_PUSH ? espconn_secure_disconnect(&serverIcon) : espconn_disconnect(&serverIcon);
}

// Disconnection callback
static void ICACHE_FLASH_ATTR UrlDisconCb(void *arg)  
{
  struct espconn *conn = arg;
  char *js;
  int i;
  
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  http_running = false;
  if (conn->reverse != NULL) // actually the chinese mistake for "reserve"
  {
    os_free(conn->reverse);
    conn->reverse = NULL;
  }
  NODE_DBG("HTTP end\n");
  udp_reply[udp_len] = 0;
  i = 0;
  while(i < udp_len && udp_reply[i] != ' ') i++; // find first space in "HTTP/1.0 200 OK"
  if(i < udp_len && udp_reply[i] == '2')
  {
    i++; 
    if(i < udp_len && udp_reply[i] == '0')
    {
      i++;
      if(i < udp_len && udp_reply[i] == '0') 
      {
        // skip headers
        js = strstr(udp_reply,"\r\n\r\n");
        if(js)
        {
          // parse JSON
          cJSON * root = cJSON_Parse(js);
          if(root)
          {
            // store 
          
            cJSON_Delete(root);
            cmd_wait = true;
          }
        }
        if(need_200) icon_must_delete();
      }
    }
  }
  if(heart_wait) 
  {
    heart_wait = false;
    send_heartbeat();
  }
  else icon_send_ping();
}

// Connection reset callback (note that there will be no DisconCb)
static void ICACHE_FLASH_ATTR UrlResetCb(void *arg, sint8 err) 
{
  struct espconn *conn = arg;

  NODE_DBG("HTTP conn. reset\n");
  os_timer_disarm(&http_timer); // Disarm HTTP timeout timer
  if (conn->reverse != NULL) // actually the chinese mistake for "reserve"
  {
    os_free(conn->reverse);
    conn->reverse = NULL;
  }
  http_running = false;
  json_timeout = true;
  json_stamp = system_get_time();
  http_end();
} 

// Receive callback
static void ICACHE_FLASH_ATTR UrlRecvCb(void *arg, char *data, unsigned short len)  
{
  NODE_DBG("HTTP receive %d bytes\n",len);
  if(udp_len + len >= sizeof(udp_reply)) len = sizeof(udp_reply) - udp_len - 1;
  if(len)
  {
    memcpy(&udp_reply[udp_len], data, len);
    udp_len += len;
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
  udp_len = 0;
  conn->reverse = os_malloc(MAX_JSON);
  len = os_sprintf(conn->reverse, "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: Close\r\nCache-Control: no-cache\r\n\r\n%s", 
    flashConfig.icon_url, flashConfig.icon_host, json_len, udp_reply);
  if(flashConfig.flags & F_SSL_PUSH) espconn_secure_send(conn, (uint8_t*)conn->reverse, len);
    else espconn_sent(conn, (uint8_t*)conn->reverse, len);
  os_timer_arm(&http_timer, PUSH_TIMEOUT, 0); // 4 seconds
  NODE_DBG("HTTP packet sending\n");
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
    http_running = true;
    return;
  }
  http_end();
}

void ICACHE_FLASH_ATTR send_json_info(void)
{
  uint32_t ip;
  struct ip_info ipconfig;
  
  if(flashConfig.icon_host[0] != 0 && flashConfig.icon_url[0] != 0 && !json_timeout)
  {
    NODE_DBG("HTTP prepare to connect\n");
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
      http_running = true;
      NODE_DBG("TCP connect called\n");
      return;
    }
    else if(ESPCONN_OK == espconn_gethostbyname(&serverIcon,flashConfig.icon_host,&dns_host,user_dns_found)) 
    {
      NODE_DBG("DNS searching\n");
      return;
    }
  }
  http_end();
}

// HTTP heart-beat
void ICACHE_FLASH_ATTR send_heartbeat(void)
{
  heart_id++;
  json_len = os_sprintf(udp_reply, "{\"convertor\":%02X%02X%02X, \"key\":\"%02X%02X\", \"heartbeat\":%lu}",
    flashConfig.convertor[0], flashConfig.convertor[1], flashConfig.convertor[2],
    flashConfig.key_data[0], flashConfig.key_data[1], heart_id
  );
  NODE_DBG("Heartbeat HTTP push\n");
  need_200 = false;
  heart_wait = false;
  send_json_info();
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