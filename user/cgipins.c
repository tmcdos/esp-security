
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serbridge.h"
#include "relay.h"

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[128];
  int len;

  len = os_sprintf(buff,
    "{ \"r1\":%d, \"r2\":%d, \"r3\":%d, \"r4\":%d, \"swap\":%d, \"rxpup\":%d }",
    flashConfig.rele_pin[0], flashConfig.rele_pin[1], flashConfig.rele_pin[2],
    flashConfig.rele_pin[3], flashConfig.flags & F_SWAP_UART ? 1 : 0, flashConfig.flags & F_RX_PULLUP ? 1 : 1);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }

  int8_t ok = 0;
  int8_t r1, r2, r3, r4;
  uint8_t swap, rxpup;
  ok |= getInt8Arg(connData, "r1", &r1);
  ok |= getInt8Arg(connData, "r2", &r2);
  ok |= getInt8Arg(connData, "r3", &r3);
  ok |= getInt8Arg(connData, "r4", &r4);
  ok |= getBoolArg(connData, "swap", &swap);
  ok |= getBoolArg(connData, "rxpup", &rxpup);
  if (ok < 0) return HTTPD_CGI_DONE;

  char *coll;
  if (ok > 0) {
    // check whether two pins collide
    uint16_t pins = 0;
    if (r1 >= 0) pins = 1 << r1;
    if (r2 >= 0) {
      if (pins & (1<<r2)) { coll = "Relay-2"; goto collision; }
      pins |= 1 << r2;
    }
    if (r3 >= 0) {
      if (pins & (1<<r3)) { coll = "Relay-3"; goto collision; }
      pins |= 1 << r3;
    }
    if (r4 >= 0) {
      if (pins & (1<<r4)) { coll = "Relay-4"; goto collision; }
      pins |= 1 << r4;
    }
    if (swap) {
      if (pins & (1<<15)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<13)) { coll = "Uart RX"; goto collision; }
    } else {
      if (pins & (1<<1)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<3)) { coll = "Uart RX"; goto collision; }
    }

    // we're good, set flashconfig
    flashConfig.rele_pin[0] = r1;
    flashConfig.rele_pin[1] = r2;
    flashConfig.rele_pin[2] = r3;
    flashConfig.rele_pin[3] = r4;
    if(!!(flashConfig.flags & F_SWAP_UART) != swap) flashConfig.flags ^= F_SWAP_UART;
    if(!!(flashConfig.flags & F_RX_PULLUP) != rxpup) flashConfig.flags ^= F_RX_PULLUP;
    NODE_DBG("Pins changed: Relay_1=%d Relay_2=%d Relay_3=%d Relay_4=%d swap=%d rx-pup=%d\n",
	    r1, r2, r3, r4, swap, rxpup);

    // apply the changes
    serbridgeInitPins();
    relay_init();

    // save to flash
    if (configSave()) {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    } else {
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  return HTTPD_CGI_DONE;

collision: {
    char buff[128];
    os_sprintf(buff, "Pin assignment for %s collides with another assignment", coll);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }
}

// Get GPIO names assigned to relays
int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (connData->requestType == HTTPD_METHOD_GET)
    return cgiPinsGet(connData);
  else if (connData->requestType == HTTPD_METHOD_POST)
    return cgiPinsSet(connData);
  else 
  {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
} 

// Cgi to return state of relays (on/off)
int ICACHE_FLASH_ATTR cgiRelayGet(HttpdConnData *connData) {

	char buff[128];
  int len;

  // print current status
  len = os_sprintf(buff, "{ \"relay_1\":%s, \"relay_2\":%s, \"relay_3\":%s, \"relay_4\":%s }", 
    relay_status[0] ? "true" : "false", relay_status[1] ? "true" : "false", 
    relay_status[2] ? "true" : "false", relay_status[3] ? "true" : "false");

	jsonHeader(connData, 200);
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

// Cgi to change state of relays (on/off)
int ICACHE_FLASH_ATTR cgiRelaySet(HttpdConnData *connData) {

  int b;
  char buff_1[8];
	int len_1 = httpdFindArg(connData->getArgs, "relay_1", buff_1, sizeof(buff_1));
  char buff_2[8];
	int len_2 = httpdFindArg(connData->getArgs, "relay_2", buff_2, sizeof(buff_2));
  char buff_3[8];
	int len_3 = httpdFindArg(connData->getArgs, "relay_3", buff_3, sizeof(buff_3));
  char buff_4[8];
	int len_4 = httpdFindArg(connData->getArgs, "relay_4", buff_4, sizeof(buff_4));

	if (len_1 <= 0 && len_2 <= 0 && len_3 <= 0 && len_4 <= 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }

  if(len_1>0) 
  {
    b = atoi(buff_1);
    if(b==0) relay_set_state(0,0);
    else if(b==1) relay_set_state(0,1);
    else relay_toggle_state(0);
    NODE_DBG("Relay 0 = %d\n",(int)flashConfig.rele_stat[0]);
  }
  if(len_2>0) 
  {
    b = atoi(buff_2);
    if(b==0) relay_set_state(1,0);
    else if(b==1) relay_set_state(1,1);
    else relay_toggle_state(1);
    NODE_DBG("Relay 1 = %d\n",(int)flashConfig.rele_stat[1]);
  }
  if(len_3>0) 
  {
    b = atoi(buff_3);
    if(b==0) relay_set_state(2,0);
    else if(b==1) relay_set_state(2,1);
    else relay_toggle_state(2);
    NODE_DBG("Relay 2 = %d\n",(int)flashConfig.rele_stat[2]);
  }
  if(len_4>0) 
  {
    b = atoi(buff_4);
    if(b==0) relay_set_state(3,0);
    else if(b==1) relay_set_state(3,1);
    else relay_toggle_state(3);
    NODE_DBG("Relay 3 = %d\n",(int)flashConfig.rele_stat[3]);
  }

	return cgiRelayGet(connData);
}

// Get or Set relay state
int ICACHE_FLASH_ATTR cgiRelay(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	if (connData->requestType == HTTPD_METHOD_GET)
		return cgiRelayGet(connData);
	else if (connData->requestType == HTTPD_METHOD_POST)
		return cgiRelaySet(connData);
	else 
	{
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}

// Save current relay state as default for power-up
int ICACHE_FLASH_ATTR cgiDefRelay(HttpdConnData *connData) 
{
  int i;
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	if (connData->requestType == HTTPD_METHOD_POST) 
	{
	  for(i=0;i<=4;i++) flashConfig.rele_stat[i] = relay_status[i];
    if (configSave()) {
      NODE_DBG("New config saved\n");
      httpdStartResponse(connData, 200);
      httpdEndHeaders(connData);
      httpdSend(connData, "Config saved", -1);
    } else {
      NODE_DBG("*** Failed to save config ***\n");
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
	} 
	else 
	{
		jsonHeader(connData, 404);
	}
	return HTTPD_CGI_DONE;
}
