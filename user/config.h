#ifndef CONFIG_H
#define CONFIG_H

#define F_SWAP_UART  1 // swap uart0 to gpio 13 & 15
#define F_RX_PULLUP  2 // internal pull-up on RX pin
#define F_AUTOSCAN   4 // scan iCON bus on power-up
#define F_SSL_PUSH   8 // use HTTPS for JSON callbacks
#define F_HTTP_AUTH 16 // require HTTP authentication
#define F_HEARTBEAT 32 // enable heart-beat
#define F_EVENT_PUSH 64 // enable JSON posting for iCON events
#define F_IO_PUSH 128 // enable JSON posting for ESP-8266 GPIO changes
#define F_SDK_PULL 256 // enable execution of SDK commands
#define F_BRIDGE 512 // enable TCP-UART bridge
#define F_IPFILTER 1024 // allow connections only from a given IP

// Flash configuration settings. When adding new items always add them at the end and formulate
// them such that a value of zero is an appropriate default or backwards compatible. Existing
// modules that are upgraded will have zero in the new fields. This ensures that an upgrade does
// not wipe out the old settings.
typedef struct {
  uint32_t seq;                        // flash write sequence number
  uint16_t magic, crc;
  uint16_t baud_rate;
  uint16_t bridge_port;                // TCP port for serial bridge
  uint32_t staticip, netmask, gateway; // using DHCP if staticip==0
  uint32_t dns_1,dns_2;                // only valid with static IP
  uint8_t  rele_pin[4];                // GPIO for each relay
  uint8_t  rele_stat[4];               // relay status (ON/OFF)
  char     hostname[32];               // if using DHCP
  char     ssid[32];                   // SoftAP
  char     password[64];               // SoftAP
  uint16_t url_port;
  char     icon_host[32];              // host to send iCON events
  char     icon_url[64];               // URL to send iCON events info (with leading slash)
  uint8_t  log_mode;                   // UART log debug mode
  char     sntp_server[32];
  int8_t   timezone_offset;
  char     web_user1[24];               // HTTP basic authentication for web interface - limited by AUTH_MAX_USER_LEN (32)
  char     web_pass1[24];               // HTTP basic authentication for web interface - limited by AUTH_MAX_PASS_LEN (32)
  char     web_user2[24];
  char     web_pass2[24];
  uint16_t flags;
  uint32_t filter_ip;                   // only connections from this IP allowed (0 = disabled)
  uint8_t  convertor[3];                // type and serial number of this WiFi converter
  uint8_t  key_data[2];                 // only Polimex knows
  uint16_t heartbeat;                   // heart beat interval in seconds
} FlashConfig;
extern FlashConfig flashConfig;

bool configSave(void);
bool configRestore(void);
void configWipe(void);
const size_t getFlashSize();

#endif
