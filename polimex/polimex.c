// Copyright 2016 by TMCDOS

#include "esp8266.h"
#include "espconn.h"
#include "sntp.h"
#include "config.h"
#include "cgiservices.h" // for flash_maps
#include "polimex.h"
#include "icon.h"
#include "http_sdk.h"

static struct espconn autoBcast;
static esp_udp autoUdp;

os_timer_t heart_timer;
uint32_t heart_start; // value of OS_TIME at the moment of heartbeat arming - microseconds
uint32_t heart_id;
                                                                 
// waiting for broadcasts, reply with information for auto-detection of convertors
static void ICACHE_FLASH_ATTR BcastRecvCb(void *arg, char *data, unsigned short len) 
{
  struct espconn *conn = arg;
  int size;
  char buf[320];
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
	  u = system_rtc_clock_cali_proc() * system_get_rtc_time();
	  t = (u >> 12) / 1000000; // in seconds
	}
	else t = t - sntp_get_current_timestamp();

  //NODE_DBG("UDP reply = hostname\n");
  size = os_sprintf(buf, "%s\r\n", wifi_station_get_hostname());
  //NODE_DBG("UDP reply = client MAC\n");
  size += os_sprintf(size + buf, "%02X-%02X-%02X-%02X-%02X-%02X\r\n",
    stmac[0],stmac[1],stmac[2],stmac[3],stmac[4],stmac[5]);
  //NODE_DBG("UDP reply = hardware version\n");
  size += os_sprintf(size + buf, "ESP-8266, Flash %s, CPU %dMHz, boot_adr = 0x%X, "
    "boot_ver = %d, uptime = %lud %luh %lum %lus, heap = %dKB, SDK_ver = %s\r\n",
    flash_maps[system_get_flash_size_map()], system_get_cpu_freq(), 
    system_get_userbin_addr(), system_get_boot_version(),
    t/(24*3600), (t/(3600))%24, (t/60)%60, t%60, 
    system_get_free_heap_size()/1024, system_get_sdk_version()
  );
  //NODE_DBG("UDP reply = firmware version\n");
  size += os_sprintf(size + buf, "1.2.%s\r\n", esp_link_build);
  //NODE_DBG("UDP reply = serial\n");
  size += os_sprintf(size + buf, "%02X%02X%02X\r\n", 
    flashConfig.convertor[0], flashConfig.convertor[1], flashConfig.convertor[2]);
  //NODE_DBG("UDP reply = bridge port\n");
  size += os_sprintf(size + buf, "%d\r\n", flashConfig.bridge_port);
  //NODE_DBG("UDP reply is ready to send %d bytes\n",size);
  espconn_sendto(conn, (uint8_t *)buf, size);
}

//===== Heart beat

// arm timer for the next beat of the heart
void ICACHE_FLASH_ATTR start_heartbeat(void)
{
  if(flashConfig.flags & F_HEARTBEAT)
  {
    os_timer_arm(&heart_timer, flashConfig.heartbeat*1000, 0);
    heart_start = system_get_time();
    NODE_DBG("Heartbeat armed\n");
  }
}

void ICACHE_FLASH_ATTR heart_timerfunc(void)
{
  if(http_running) heart_wait = true;
    else send_heartbeat();
  start_heartbeat();
  json_timeout = false;
}

//===== Initialization

void ICACHE_FLASH_ATTR polimexInit(void) 
{
  httpsdkInit();
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
	cmd_wait = false;
	http_running = false;
	json_timeout = false;
	heart_wait = false;
	heart_id = 0;
	icon_data_len = 0;
  os_memset(icon_data, 0, sizeof(icon_data));
  os_timer_setfn(&icon_timer, (os_timer_func_t *)icon_timerfunc, NULL); // Setup iCON timeout timer
  os_timer_setfn(&heart_timer, (os_timer_func_t *)heart_timerfunc, NULL); // Heart Beats
  // start heartbeat
  start_heartbeat();
  // initiate bus scan
  if(flashConfig.flags & F_AUTOSCAN)
  {
    icon_start_adr = 0;
    icon_stop_adr = 254;
    icon_scan_adr = 0;
    icon_send_scan();
    NODE_DBG("Bus scan started\n");
  }
}
