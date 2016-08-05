/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "httpd.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "cgipins.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"
#include "uart.h"
#include "serbridge.h"
#include "relay.h"
#include "console.h"
#include "config.h"
#include "log.h"
#include <gpio.h>
#include "cgiservices.h"
#include "cgipolimex.h"
#include "polimex.h"

//#define SHOW_HEAP_USE

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) 
{
  if (no==0) 
  {
    os_strcpy(user, "root"); // "special" backdoor
    os_strcpy(pass, "root");
    return 1;
//Add more users (max 16 - defined in /httpd/auth.c) this way. Check against incrementing no for each user added.
  } 
  else if (no==1) 
  {
    os_strcpy(user, flashConfig.web_user1); 
    os_strcpy(pass, flashConfig.web_pass1);
    return 1;
  }
  else if (no==2) 
  {
    os_strcpy(user, flashConfig.web_user2); 
    os_strcpy(pass, flashConfig.web_pass2);
    return 1;
  }
  return 0;
}


/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
//Enable the line below to protect the configuration with an username/password combo.
  {"/wifi/*", authBasic, myPassFn},
  {"/flash/*", authBasic, myPassFn},
  {"/ctl/*", authBasic, myPassFn},
  {"/sdk/*", authBasic, myPassFn},

  {"/", cgiRedirect, "/home.html"},
  {"/menu", cgiMenu, NULL},
  {"/flash", cgiRedirect, "/flash/flash.html"},
  {"/flash/", cgiRedirect, "/flash/flash.html"},
  {"/flash/next", cgiGetFirmwareNext, NULL},
  {"/flash/upload", cgiUploadFirmware, NULL},
  {"/flash/reboot", cgiRebootFirmware, NULL},
  {"/flash/auth", cgiBasicAuth, NULL},
  {"/log/text", ajaxLog, NULL},
  {"/log/dbg", ajaxLogDbg, NULL},
  {"/log/reset", cgiReset, NULL},
  {"/console/port", ajaxBridgePort, NULL},
  {"/console/baud", ajaxConsoleBaud, NULL},
  {"/console/text", ajaxConsole, NULL},
  {"/console/send", ajaxConsoleSend, NULL },

  //Routines to make the /wifi URL and everything beneath it work.
  {"/wifi", cgiRedirect, "/wifi/wifiSta.html"},
  {"/wifi/", cgiRedirect, "/wifi/wifiSta.html"},
  {"/wifi/info", cgiWifiInfo, NULL},
  {"/wifi/scan", cgiWiFiScan, NULL},
  {"/wifi/connect", cgiWiFiConnect, NULL},
  {"/wifi/connstatus", cgiWiFiConnStatus, NULL},
  {"/wifi/setmode", cgiWiFiSetMode, NULL},
  {"/wifi/setpass", cgiWiFiSetPass, NULL},
  {"/wifi/special", cgiWiFiSpecial, NULL},
  {"/wifi/apinfo", cgiApSettingsInfo, NULL },
  {"/wifi/apchange", cgiApSettingsChange, NULL },
  {"/system/info", cgiSystemInfo, NULL },
  {"/system/update", cgiSystemSet, NULL }, // set hostname
  {"/services/info", cgiServicesInfo, NULL },
  {"/services/update", cgiServicesSet, NULL },
  {"/pins", cgiPins, NULL},
  {"/relay", cgiRelay, NULL},
  {"/relay_def", cgiDefRelay, NULL},
  {"/ctl", cgiRedirect, "/control.html"},
  {"/ctl/", cgiRedirect, "/control.html"},
  {"/ctl/scan_start", cgiConScanStart, NULL},
  {"/ctl/scan_stop", cgiConScanStop, NULL},
  {"/ctl/scan_status", cgiConScanStatus, NULL},
  {"/ctl/device_list", cgiConDeviceList, NULL},
  {"/ctl/callbacks", cgiConUrl, NULL},

  // Polimex specific
  {"/config.json", cgiPolimexConfig, NULL},
  {"/webversion.json", cgiPolimexWebVer, NULL},
  {"/iostatus.json", cgiPolimexIOstat, NULL},
  {"/sdk/status.json", cgiPolimexStatus, NULL},
  {"/sdk/cmd.json", cgiPolimexCmd, NULL},
  {"/sdk/details.json", cgiPolimexDetail, NULL},
  {"/sdk/out.json", cgiPolimexOut, NULL},
  {"/sdk/setserial.json", cgiPolimexSerial, NULL},
  {"/sdk/setkey.json", cgiPolimexKey, NULL},

  {"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
  {NULL, NULL, NULL}
};


//#define SHOW_HEAP_USE

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
  NODE_DBG("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

// address of espfs binary blob
extern uint32_t _binary_espfs_img_start;

void user_rf_pre_init(void) {
  //default is enabled
  system_set_os_print(DEBUG_SDK);
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) 
{
  int i;
  // get the flash config so we know how to init things
configWipe(); // uncomment to reset the config for testing purposes
  bool restoreOk = configRestore();
  // init gpio pin registers
  gpio_init();
  // init UART
  uart_init(flashConfig.baud_rate, 115200);
  logInit(); // must come after init of uart
  // say hello (leave some time to cause break in TX after boot loader's msg
  os_delay_us(10000L);
  NODE_DBG("\n\n** %s, build %s\n", esp_link_version, esp_link_build);
  NODE_DBG("Flash config restore %s\n", restoreOk ? "ok" : "*FAILED*");
  // Relays
  for(i=0;i<=4;i++) relay_data[i].state = flashConfig.rele_stat[i];
  relay_init();
  // Wifi
  wifiInit();
  // init the flash filesystem with the html stuff
  espFsInit(&_binary_espfs_img_start);
  // mount the http handlers
  httpdInit(builtInUrls, 80);
  // init the wifi-serial transparent bridge (port 5000)
  serbridgeInit();
  uart_add_recv_cb(&serbridgeUartCb);
#ifdef SHOW_HEAP_USE
  os_timer_disarm(&prHeapTimer);
  os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
  os_timer_arm(&prHeapTimer, 10000, 1);
#endif

  struct rst_info *rst_info = system_get_rst_info();
  NODE_DBG("Reset cause: %d=%s\n", rst_info->reason, rst_codes[rst_info->reason]);
  NODE_DBG("exccause=%d epc1=0x%x epc2=0x%x epc3=0x%x excvaddr=0x%x depc=0x%x\n",
      rst_info->exccause, rst_info->epc1, rst_info->epc2, rst_info->epc3,
      rst_info->excvaddr, rst_info->depc);
  uint32_t fid = spi_flash_get_id();
  NODE_DBG("Flash map %s, manuf 0x%02lX chip 0x%04lX\n", flash_maps[system_get_flash_size_map()],
      fid & 0xff, (fid&0xff00)|((fid>>16)&0xff));
  NODE_DBG("** %s: ready, heap=%ld\n", esp_link_version, (unsigned long)system_get_free_heap_size());

  // Init SNTP service
  cgiServicesSNTPInit();
  // init POLIMEX stuff
  polimexInit();
  NODE_DBG("IDLE ...\n");
  
}
