
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "cgiservices.h"
#include "sntp.h"
#include "relay.h"
#include "serbridge.h"
#include "cgipolimex.h"
#include "icon.h"
#include "polimex.h"
#include "http_sdk.h"

// Cgi to initiate scan of iCON bus
int ICACHE_FLASH_ATTR cgiConScanStart(HttpdConnData *connData) 
{
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  
  icon_start_adr = 0;
  icon_stop_adr = 254;
  char buff_1[8];
  char buff_2[8];

	int len_1 = httpdFindArg(connData->getArgs, "start", buff_1, sizeof(buff_1));
  if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "start", buff_1, sizeof(buff_1));
  if(len_1>0) icon_start_adr = atoi(buff_1);

	int len_2 = httpdFindArg(connData->getArgs, "final", buff_2, sizeof(buff_2));
  if(len_2==0) len_2 = httpdFindArg(connData->post->buff, "final", buff_2, sizeof(buff_2));
  if(len_2>0) icon_stop_adr = atoi(buff_2);
  NODE_DBG("iCON bus scan from %d to %d\n",icon_start_adr, icon_stop_adr);

	jsonHeader(connData, 200);
	if(icon_start_adr<0) icon_start_adr = 0;
	if(icon_stop_adr>254) icon_stop_adr = 254;
	
  // prepare for scanning ICON bus
  icon_count = 0;
  icon_send_scan();
	return HTTPD_CGI_DONE;
}

// Cgi to stop scanning of iCON bus
int ICACHE_FLASH_ATTR cgiConScanStop(HttpdConnData *connData) 
{
  if(icon_state == ICS_SCAN)
  {
  	NODE_DBG("Stop scanning iCON bus\n");
    icon_start_adr = 0;
  }
  else NODE_DBG("iCON bus is not currently scanning - stop request ignored\n");
  jsonHeader(connData, 200);
	return HTTPD_CGI_DONE;
}

// Cgi to return progress of the current iCON bus scan
int ICACHE_FLASH_ATTR cgiConScanStatus(HttpdConnData *connData) 
{
	char buff[128];
  int len;  
  
  NODE_DBG("iCON scanning status\n");
  len = os_sprintf(buff, "{ \"in_progress\":%d, \"device_count\":%d, \"current_address\":%d }", 
    icon_state == ICS_SCAN ? 1 : 0, icon_count, icon_scan_adr);  
 	jsonHeader(connData, 200);
	httpdSend(connData, buff, len); 
	return HTTPD_CGI_DONE;
}  

// Cgi to return list of devices on the iCON bus
int ICACHE_FLASH_ATTR cgiConDeviceList(HttpdConnData *connData) 
{
	char buff[64];
  int len,i;  

  NODE_DBG("iCON device list\n");
 	jsonHeader(connData, 200);
  len = os_sprintf(buff, "[ ");
	httpdSend(connData, buff, len); 
  if(icon_count!=0) for(i=0;i<icon_count;i++)
  {
    len = os_sprintf(buff, i!=0 ? ",{ \"address\":%d }" : "{ \"address\":%d }", icon_dev[i].adr);
  	httpdSend(connData, buff, len); 
  }
  len = os_sprintf(buff, " ]");
	httpdSend(connData, buff, len); 

  return HTTPD_CGI_DONE;
}

// Cgi to get/set the callback url where to send iCON events
int ICACHE_FLASH_ATTR cgiConUrl(HttpdConnData *connData) 
{
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	if (connData->requestType == HTTPD_METHOD_GET) 
		return cgiConGetUrl(connData);
	else if (connData->requestType == HTTPD_METHOD_POST) 
		return cgiConSetUrl(connData);
	else 
	{
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}

int ICACHE_FLASH_ATTR cgiConGetUrl(HttpdConnData *connData) 
{
	char buff[sizeof(flashConfig.icon_url)+110];
  int len;  

  NODE_DBG("Get URL for iCON events\n");
  len = os_sprintf(buff, "{ \"callback_host\":\"%s\", \"callback_port\":%d, \"callback_url\":\"%s\", \"callback_autoscan\":%d, \"callback_ssl\":%d }", 
    flashConfig.icon_host, flashConfig.url_port, flashConfig.icon_url, flashConfig.flags & F_AUTOSCAN ? 1 : 0, flashConfig.flags & F_SSL_PUSH ? 1 : 0);

 	jsonHeader(connData, 200);
	httpdSend(connData, buff, len); 
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiConSetUrl(HttpdConnData *connData) 
{
  char buff_1[sizeof(flashConfig.icon_url)];
  int i, len_1,modif;

  modif = 0;
	// get port
	len_1 = httpdFindArg(connData->getArgs, "callback_port", buff_1, sizeof(buff_1));
	if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "callback_port", buff_1, sizeof(buff_1));
	if(len_1>0) 
	{
	  flashConfig.url_port = atoi(buff_1);
	  if(flashConfig.url_port == 0) flashConfig.url_port = 80;
	  modif = 1;
	}
  // get hostname
	len_1 = httpdFindArg(connData->getArgs, "callback_server", buff_1, sizeof(buff_1));
	if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "callback_server", buff_1, sizeof(buff_1));
	if(len_1>0) 
	{
  	memset(flashConfig.icon_host, 0, sizeof(flashConfig.icon_host)); 
	  memcpy(flashConfig.icon_host, buff_1, len_1); 
	  modif = 1;
	}
	// get URL path
	len_1 = httpdFindArg(connData->getArgs, "callback_url", buff_1, sizeof(buff_1));
	if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "callback_url", buff_1, sizeof(buff_1));
	if(len_1>0) 
	{
  	memset(flashConfig.icon_url, 0, sizeof(flashConfig.icon_url)); 
	  memcpy(flashConfig.icon_url, buff_1, len_1);
	  modif = 1;
	}
	// get Autoscan flag
	len_1 = httpdFindArg(connData->getArgs, "callback_autoscan", buff_1, sizeof(buff_1));
	if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "callback_autoscan", buff_1, sizeof(buff_1));
	if(len_1>0) 
	{
	  i = atoi(buff_1);
	  if(!!(flashConfig.flags & F_AUTOSCAN) != i) 
	  {
	    flashConfig.flags ^= F_AUTOSCAN;
	    modif = 1;
	  }
	}
	// get SSL flag
	len_1 = httpdFindArg(connData->getArgs, "callback_ssl", buff_1, sizeof(buff_1));
	if(len_1==0) len_1 = httpdFindArg(connData->post->buff, "callback_ssl", buff_1, sizeof(buff_1));
	if(len_1>0) 
	{
	  i = atoi(buff_1);
	  if(!!(flashConfig.flags & F_SSL_PUSH) != i) 
	  {
	    flashConfig.flags ^= F_SSL_PUSH;
	    modif = 1;
	  }
	}
	
	if(modif)
	{
    if (configSave()) 
    { 
      NODE_DBG("iCON callback settings saved\n"); 
      httpdStartResponse(connData, 200);
      httpdEndHeaders(connData);
      httpdSend(connData, "Config saved", -1);
    } 
    else 
    {
      NODE_DBG("*** Failed to save config ***\n");
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  else
  {
    NODE_DBG("iCON callback settings - no modifications\n"); 
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, "Nothing changed", -1);
  }
	return HTTPD_CGI_DONE;
}

// URL == /config.json
int ICACHE_FLASH_ATTR cgiPolimexConfig(HttpdConnData *connData)
{
  int len;
	unsigned long long u;
	uint32_t t = sntp_start;
	uint8 stmac[6];
	struct ip_info info; 

  NODE_DBG("SDK config\n");
 	jsonHeader(connData, 200);
 	
  wifi_get_macaddr(STATION_IF, stmac); 
  wifi_get_ip_info(0, &info);
	if(t==0)
	{
	  u = system_rtc_clock_cali_proc() * system_get_rtc_time();
	  t = (u >> 12) / 1000000; // in seconds
	}
	else t = t - sntp_get_current_timestamp();
  len = os_sprintf(udp_reply, "{ \"convertor\": %02X%02X%02X,"
    "\"sdk\": {"
    "\"sdkVersion\": 1.2.%s,"
    "\"sdkHardware\": 1.2,"
    "\"TCPStackVersion\": \"%s\","
    "\"ConnectionType\": 2,"
    "\"cpuUsage\": 0,"
    "\"isDeviceScan\": %d,"
    "\"isEventScan\": %d,"
    "\"isEventPause\": %d,"
    "\"isCmdWaiting\": 1,"
    "\"isCmdExecute\": %d,"
    "\"isBrigdeActive\": %d,"
    "\"isServerToSendDown\": %d,"
    "\"maxDevInList\": %d,"
    "\"devFound\": %d,"
    "\"scanIDfrom\": %d,"
    "\"scanIDto\": %d,"
    "\"scanIDprogress\": %d,"
    "\"heartBeatCounter\": %d,"
    "\"heartBeatTimeOut\": %lu,"
    "\"upTime\": \"%lud %lu:%lu:%lu\","
    "\"remoteIP\": \"%d.%d.%d.%d\"},"

    "\"inputOutputHardware\": {"
    "\"portOut\": 4, \"portInDigital\": 0, \"portInAnalog\": 0, \"portPWM\": 0},"

    "\"netConfig\": {"
    "\"MAC_Address\": \"%02X:%02X:%02X:%02X:%02X:%02X\","
    "\"Host_Name\": \"%s\","
    "\"checkbox_DHCP\": \"%s\","
    "\"IP_Address\": \"%d.%d.%d.%d\","
    "\"Gateway\": \"%d.%d.%d.%d\","
    "\"Subnet_Mask\": \"%d.%d.%d.%d\","
    "\"Primary_DNS\": \"%d.%d.%d.%d\","
    "\"Secondary_DNS\": \"%d.%d.%d.%d\"},"

    "\"sdkSettings\": {"
    "\"checkbox_Enable_HTTP_Pull_Technology\": \"%s\","
    "\"checkbox_Enable_HTTP_Server_Push\": \"%s\","
    "\"checkbox_Enable_HTTP_IO_Event_Server_Push\": \"%s\","
    "\"checkbox_Enable_HeartBeat\": \"%s\","
    "\"checkbox_Enable_TCP_Bridge\": \"%s\","
    "\"checkbox_Enable_custom_Bridge_port\": \"checked\","
    "\"checkbox_SDK_Password_Require\": \"%s\","
    "\"Server_URL\": \"%s%s%s\","
    "\"Server_PORT\": %d,"
    "\"HeartBeat_Time\": %d,"
    "\"Bridge_PORT\": %d },"

    "\"currentIPFiltering\": {"
    "\"checkbox_Enable_IP1_filter\": \"%s\","
    "\"IP1\": \"%d.%d.%d.%d\"}}",
    flashConfig.convertor[0],flashConfig.convertor[1],flashConfig.convertor[2],
    esp_link_build, system_get_sdk_version(),
    icon_state==ICS_SCAN ? 1 : 0,
    icon_state==ICS_EVENT ? 1 : 0, 
    icon_state==ICS_EVENT ? 0 : 1,
    icon_state==ICS_CMD ? 1 : 0, 
    bridge_active, json_timeout ? 1 : 0,
    MAX_ICON, icon_count, icon_start_adr, icon_stop_adr, icon_scan_adr,
    flashConfig.heartbeat, (system_get_time() - heart_start) / 1000000,
    t/(24*3600), (t/(3600))%24, (t/60)%60, t%60, 
    (bridge_active && connData[0].conn) ? connData[0].conn->proto.tcp->remote_ip[0] : 0,
    (bridge_active && connData[0].conn) ? connData[0].conn->proto.tcp->remote_ip[1] : 0,
    (bridge_active && connData[0].conn) ? connData[0].conn->proto.tcp->remote_ip[2] : 0,
    (bridge_active && connData[0].conn) ? connData[0].conn->proto.tcp->remote_ip[3] : 0,
    stmac[0],stmac[1],stmac[2],stmac[3],stmac[4],stmac[5],
    wifi_station_get_hostname(), flashConfig.staticip == 0 ? "checked" : "\0",
    IP2STR(&info.ip.addr), IP2STR(&info.gw.addr), IP2STR(&info.netmask.addr),
    IP2STR(dns_getserver(0)), IP2STR(dns_getserver(1)),

    flashConfig.flags & F_SDK_PULL ? "checked" : "\0",
    flashConfig.flags & F_EVENT_PUSH ? "checked" : "\0",
    flashConfig.flags & F_IO_PUSH ? "checked" : "\0",
    flashConfig.flags & F_HEARTBEAT ? "checked" : "\0",
    flashConfig.flags & F_BRIDGE ? "checked" : "\0",
    flashConfig.flags & F_HTTP_AUTH ? "checked" : "\0",
    (flashConfig.icon_host[0] && flashConfig.icon_url[0]) ? (flashConfig.flags & F_SSL_PUSH ? "https://" : "http://") : "\0",
    (flashConfig.icon_host[0] && flashConfig.icon_url[0]) ? flashConfig.icon_host : "\0",
    (flashConfig.icon_host[0] && flashConfig.icon_url[0]) ? flashConfig.icon_url : "\0",
    flashConfig.url_port, flashConfig.heartbeat, flashConfig.bridge_port,
    
    flashConfig.flags & F_IPFILTER ? "checked" : "\0",
    IP2STR(&flashConfig.filter_ip)
  );
	httpdSend(connData, udp_reply, len); 
  return HTTPD_CGI_DONE;
}

// URL == /webversion.json
int ICACHE_FLASH_ATTR cgiPolimexWebVer(HttpdConnData *connData)
{
  int len;

  NODE_DBG("SDK web version\n");
 	jsonHeader(connData, 200);
 	
  len = os_sprintf(udp_reply, "{\"convertor\": %02X%02X%02X, \"webVersion\":\"1.2.%s\", \"osVersion\": \"%s\"}",
    flashConfig.convertor[0],flashConfig.convertor[1],flashConfig.convertor[2],
    esp_link_build, system_get_sdk_version());
	httpdSend(connData, udp_reply, len); 
  return HTTPD_CGI_DONE;
}

// URL == /iostatus.json
int ICACHE_FLASH_ATTR cgiPolimexIOstat(HttpdConnData *connData)
{
  int len;
  uint32_t t;

  NODE_DBG("SDK get IO status\n");
 	jsonHeader(connData, 200);
 	
 	t = system_get_time();
  len = os_sprintf(udp_reply, "{\"convertor\": %02X%02X%02X, \"portOut\": { "
    "out0: %d, out0timer: %lu, out0cnt: %lu, out1: %d, out1timer: %lu, out1cnt: %lu, "
    "out2: %d, out2timer: %lu, out2timer: %lu, out3: %d, out3timer: %lu, out3cnt: %lu}}",
    flashConfig.convertor[0],flashConfig.convertor[1],flashConfig.convertor[2],
    relay_data[0].state, relay_data[0].armed ? (t - relay_data[0].timeout) : 0, relay_data[0].counter,
    relay_data[1].state, relay_data[1].armed ? (t - relay_data[1].timeout) : 0, relay_data[1].counter,
    relay_data[2].state, relay_data[2].armed ? (t - relay_data[2].timeout) : 0, relay_data[2].counter,
    relay_data[3].state, relay_data[3].armed ? (t - relay_data[3].timeout) : 0, relay_data[3].counter
  );
	httpdSend(connData, udp_reply, len); 

  return HTTPD_CGI_DONE;
}

// URL == /sdk/setserial.json
int ICACHE_FLASH_ATTR cgiPolimexSerial(HttpdConnData *connData)
{
  char buff_1[10];
  uint8_t cvt[3];
  int len_1, modif;

  modif = 0;
	len_1 = httpdFindArg(connData->getArgs, "s", buff_1, sizeof(buff_1));
	if(len_1==6) 
	{
	  if(isdigit(buff_1[0]) && isdigit(buff_1[1]))
	  {
	    cvt[0] = (buff_1[0] - '0')<< 4 | (buff_1[1] - '0');
  	  if(isdigit(buff_1[2]) && isdigit(buff_1[3]))
  	  {
  	    cvt[1] = (buff_1[2] - '0')<< 4 | (buff_1[3] - '0');
    	  if(isdigit(buff_1[4]) && isdigit(buff_1[5]))
    	  {
    	    cvt[2] = (buff_1[4] - '0')<< 4 | (buff_1[5] - '0');
        	flashConfig.convertor[0] = cvt[0];
        	flashConfig.convertor[1] = cvt[1];
        	flashConfig.convertor[2] = cvt[2];
        	modif = 1;
        }
  	  }
  	}
	}

  if(modif)
  {
    if (configSave()) 
    { 
      NODE_DBG("iCON Serial number saved\n"); 
      httpdStartResponse(connData, 200);
      httpdEndHeaders(connData);
      httpdSend(connData, "Config saved", -1);
    } 
    else 
    {
      NODE_DBG("*** Failed to save config ***\n");
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  else
  {
    NODE_DBG("iCON serial number - no modifications\n"); 
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, "Nothing changed", -1);
  }
  return HTTPD_CGI_DONE;
}

// URL == /sdk/setkey.json
int ICACHE_FLASH_ATTR cgiPolimexKey(HttpdConnData *connData)
{
  char buff_1[10];
  uint8_t cvt[4];
  int i, len_1, modif;

  modif = 0;
	len_1 = httpdFindArg(connData->getArgs, "k", buff_1, sizeof(buff_1));
	if(len_1==4) 
	{
	  i = 1;
	  for(len_1=0; len_1<=3; len_1++)
	  {
	    if(buff_1[len_1] >= '0' && buff_1[len_1] <= '9') cvt[len_1] = buff_1[len_1] - '0';
	    else if(buff_1[len_1] >= 'a' && buff_1[len_1] <= 'f') cvt[len_1] = buff_1[len_1] - 'a' + 10;
	    else if(buff_1[len_1] >= 'A' && buff_1[len_1] <= 'F') cvt[len_1] = buff_1[len_1] - 'A' + 10;
	    else i = 0;
	  }
	  if(i)
	  {
    	flashConfig.key_data[0] = cvt[0]<<4 | cvt[1];
    	flashConfig.key_data[1] = cvt[2]<<4 | cvt[3];
    	modif = 1;
    }
	}

  if(modif)
  {
    if (configSave()) 
    { 
      NODE_DBG("iCON Key saved\n"); 
      httpdStartResponse(connData, 200);
      httpdEndHeaders(connData);
      httpdSend(connData, "Config saved", -1);
    } 
    else 
    {
      NODE_DBG("*** Failed to save config ***\n");
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  else
  {
    NODE_DBG("iCON key - no modifications\n"); 
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, "Nothing changed", -1);
  }
  return HTTPD_CGI_DONE;
}

// URL == /sdk/details.json
int ICACHE_FLASH_ATTR cgiPolimexDetail(HttpdConnData *connData)
{
  int len;
	unsigned long long u;
	uint32_t t = sntp_start;

  NODE_DBG("SDK details\n");
 	jsonHeader(connData, 200);
 	
	if(t==0)
	{
	  u = system_rtc_clock_cali_proc() * system_get_rtc_time();
	  t = (u >> 12) / 1000000; // in seconds
	}
	else t = t - sntp_get_current_timestamp();
  len = os_sprintf(udp_reply, "{ \"convertor\": %02X%02X%02X,"
    "\"sdk\": {"
    "\"sdkVersion\": 1.2.%s,"
    "\"sdkHardware\": 1.2,"
    "\"TCPStackVersion\": \"%s\","
    "\"ConnectionType\": 2,"
    "\"cpuUsage\": 0,"
    "\"isDeviceScan\": %d,"
    "\"isEventScan\": %d,"
    "\"isEventPause\": %d,"
    "\"isCmdWaiting\": 1,"
    "\"isCmdExecute\": %d,"
    "\"isBrigdeActive\": %d,"
    "\"isServerToSendDown\": %d,"
    "\"maxDevInList\": %d,"
    "\"devFound\": %d,"
    "\"scanIDfrom\": %d,"
    "\"scanIDto\": %d,"
    "\"scanIDprogress\": %d,"
    "\"heartBeatCounter\": %d,"
    "\"heartBeatTimeOut\": %lu,"
    "\"upTime\": \"%lud %lu:%lu:%lu\"}}",

    flashConfig.convertor[0],flashConfig.convertor[1],flashConfig.convertor[2],
    esp_link_build, system_get_sdk_version(),
    icon_state==ICS_SCAN ? 1 : 0,
    icon_state==ICS_EVENT ? 1 : 0, 
    icon_state==ICS_EVENT ? 0 : 1,
    icon_state==ICS_CMD ? 1 : 0, 
    bridge_active, json_timeout ? 1 : 0,
    MAX_ICON, icon_count, icon_start_adr, icon_stop_adr, icon_scan_adr,
    flashConfig.heartbeat, (system_get_time() - heart_start) / 1000000,
    t/(24*3600), (t/(3600))%24, (t/60)%60, t%60
  );
	httpdSend(connData, udp_reply, len); 
  return HTTPD_CGI_DONE;
}

// URL == /sdk/status.json
int ICACHE_FLASH_ATTR cgiPolimexStatus(HttpdConnData *connData)
{
  return HTTPD_CGI_DONE;
}

// URL == /sdk/out.json
int ICACHE_FLASH_ATTR cgiPolimexOut(HttpdConnData *connData)
{
  return HTTPD_CGI_DONE;
}

// URL == /sdk/cmd.json
int ICACHE_FLASH_ATTR cgiPolimexCmd(HttpdConnData *connData)
{
  return HTTPD_CGI_DONE;
}
