#include <esp8266.h>
#include "cgiwifi.h"
#include "cgi.h"
#include "config.h"
#include "sntp.h"

LOCAL os_timer_t sntp_timer;
uint32_t sntp_start; // UNIX timestamp

char* rst_codes[7] = {
  "normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};

char* flash_maps[7] = {
  "512KB (256+256)", "256KB", "1MB (512+512)", "2MB (512+512)", "4MB (512+512)",
  "2MB (1024+1024)", "4MB (1024+1024)"
};

static ETSTimer reassTimer;

// Cgi to update system info (hostname)
int ICACHE_FLASH_ATTR cgiSystemSet(HttpdConnData *connData) {
  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  int8_t n = getStringArg(connData, "name", flashConfig.hostname, sizeof(flashConfig.hostname));

  if (n < 0) return HTTPD_CGI_DONE; // getStringArg has produced an error response

  if (n > 0) {
    // schedule hostname change-over
    os_timer_disarm(&reassTimer);
    os_timer_setfn(&reassTimer, configWifiIP, NULL);
    os_timer_arm(&reassTimer, 1000, 0); // 1 second for the response of this request to make it
  }

  if (configSave()) {
    httpdStartResponse(connData, 204);
    httpdEndHeaders(connData);
  }
  else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
  return HTTPD_CGI_DONE;
}

// Cgi to return various System information
int ICACHE_FLASH_ATTR cgiSystemInfo(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  uint8 part_id = system_upgrade_userbin_check();
  uint32_t fid = spi_flash_get_id();
  struct rst_info *rst_info = system_get_rst_info();

  os_sprintf(buff,
    "{ "
      "\"name\": \"%s\", "
      "\"reset_cause\": \"%d=%s\", "
      "\"size\": \"%s\", "
      "\"id\": \"0x%02lX, 0x%04lX\", "
      "\"partition\": \"%s\", "
      "\"baud\": \"%d\" "
    " }",
    flashConfig.hostname,
    rst_info->reason,
    rst_codes[rst_info->reason],
    flash_maps[system_get_flash_size_map()],
    fid & 0xff, 
    (fid & 0xff00) | ((fid >> 16) & 0xff),
    part_id ? "user2.bin" : "user1.bin",
    flashConfig.baud_rate
    );

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR cgiServicesSNTPInit() {
  
  if (flashConfig.sntp_server[0] != '\0') {    
    sntp_stop();
    if (true == sntp_set_timezone(flashConfig.timezone_offset)) {
      sntp_setservername(0, flashConfig.sntp_server);  
      sntp_startup_stamp = 0;
      sntp_init();
      os_timer_disarm(&sntp_timer);
      os_timer_setfn(&sntp_timer, (os_timer_func_t *)user_sntp_stamp, NULL);
      os_timer_arm(&sntp_timer, 100, 0);
    }
    NODE_DBG("SNTP timesource set to %s with offset %d\n", flashConfig.sntp_server, flashConfig.timezone_offset);
  }
}

void ICACHE_FLASH_ATTR user_sntp_stamp(void *arg)
{
  uint32 current_stamp;

  current_stamp = sntp_get_current_timestamp();
  if(current_stamp == 0) os_timer_arm(&sntp_timer, 100, 0);
  else
  {
    os_timer_disarm(&sntp_timer);
    sntp_start = current_stamp;
  }
}

int ICACHE_FLASH_ATTR cgiServicesInfo(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  os_sprintf(buff, 
    "{ "
      "\"timezone_offset\": %d, "
      "\"sntp_server\": \"%s\" "
    " }",    
    flashConfig.timezone_offset,
    flashConfig.sntp_server
    );

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiServicesSet(HttpdConnData *connData) {
  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  int8_t sntp = 0;
  sntp |= getInt8Arg(connData, "timezone_offset", &flashConfig.timezone_offset);
  if (sntp < 0) return HTTPD_CGI_DONE;
  sntp |= getStringArg(connData, "sntp_server", flashConfig.sntp_server, sizeof(flashConfig.sntp_server));
  if (sntp < 0) return HTTPD_CGI_DONE;

  if (sntp > 0) cgiServicesSNTPInit();

  if (configSave()) {
    httpdStartResponse(connData, 204);
    httpdEndHeaders(connData);
  }
  else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
  return HTTPD_CGI_DONE;
}
