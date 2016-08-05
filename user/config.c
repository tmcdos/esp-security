// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"
#include "crc16.h"

FlashConfig flashConfig;

FlashConfig flashDefault = {
  .seq = 33, .magic = 0, .crc = 0,
  .baud_rate = 9600,
  .bridge_port = 5000, // serial-bridge TCP port
  .staticip = 0, 
  .netmask = 0x00ffffff, 
  .gateway = 0,  // static ip, netmask, gateway
  .rele_pin = {7,6,5,0}, // relay pins
  .rele_stat = {0,0,0,0}, // default relay status
  .hostname = "esp-link\0", // hostname
  .ssid = "ESP_BRIDGE\0", // SoftAP SSID
  .password = "\0", // SoftAP password
  .web_user1 = "admin\0",
  .web_pass1 = "admin\0",
  .web_user2 = "admin\0",
  .web_pass2 = "admin\0",
  .url_port = 80, // TCP port to send iCON events
  .icon_host = "\0", // hostname to send iCON events
  .icon_url = "\0", // URL pathname to send iCON events (without host, but leading slash)
  .log_mode = 3,   // log mode = UART_1
  .flags = F_RX_PULLUP,
  .sntp_server  = "bg.pool.ntp.org\0",
  .timezone_offset = 2,
  .convertor = {0x34,0,0},
  .heartbeat = 60
 
};

typedef union {
  FlashConfig fc;
  uint8_t     block[256]; // must correspond to SizeOf (fc)
} FlashFull;

// magic number to recognize thet these are our flash settings as opposed to some random stuff
#define FLASH_MAGIC  (0xaa55)

// size of the setting sector
#define FLASH_SECT   (4096)

// address where to flash the settings: if we have >512KB flash then there are 16KB of reserved
// space at the end of the first flash partition, we use the upper 8KB (2 sectors). If we only
// have 512KB then that space is used by the SDK and we use the 8KB just before that.
static uint32_t ICACHE_FLASH_ATTR flashAddr(void) {
  enum flash_size_map map = system_get_flash_size_map();
  return map >= FLASH_SIZE_8M_MAP_512_512
    ? FLASH_SECT + FIRMWARE_SIZE + 2*FLASH_SECT // bootloader + firmware + 8KB free
    : FLASH_SECT + FIRMWARE_SIZE - 2*FLASH_SECT;// bootloader + firmware - 8KB (risky...)
}

static int flash_pri; // primary flash sector (0 or 1, or -1 for error)

bool ICACHE_FLASH_ATTR configSave(void) {
  FlashFull ff;
  os_memset(&ff, 0, sizeof(ff));
  os_memcpy(&ff, &flashConfig, sizeof(FlashConfig));
  uint32_t seq = ff.fc.seq+1;
  // erase secondary
  uint32_t addr = flashAddr() + (1-flash_pri)*FLASH_SECT;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    goto fail; // no harm done, give up
  // calculate CRC
  ff.fc.seq = seq;
  ff.fc.magic = FLASH_MAGIC;
  ff.fc.crc = 0;
  //NODE_DBG("cksum of: ");
  //memDump(&ff, sizeof(ff));
  ff.fc.crc = crc16_data((unsigned char *)&ff, sizeof(ff), 0);
  NODE_DBG("cksum is %04x\n", ff.fc.crc);
  // write primary with incorrect seq
  ff.fc.seq = 0xffffffff;
  if (spi_flash_write(addr, (void *)&ff, sizeof(ff)) != SPI_FLASH_RESULT_OK)
    goto fail; // no harm done, give up
  // fill in correct seq
  ff.fc.seq = seq;
  if (spi_flash_write(addr, (void *)&ff, sizeof(uint32_t)) != SPI_FLASH_RESULT_OK)
    goto fail; // most likely failed, but no harm if successful
  // now that we have safely written the new version, erase old primary
  addr = flashAddr() + flash_pri*FLASH_SECT;
  flash_pri = 1-flash_pri;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  // write secondary
  ff.fc.seq = 0xffffffff;
  if (spi_flash_write(addr, (void *)&ff, sizeof(ff)) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  ff.fc.seq = seq;
  spi_flash_write(addr, (void *)&ff, sizeof(uint32_t));
  NODE_DBG("Flash config saved\n");
  return true;
fail:
  NODE_DBG("*** Failed to save config ***\n");
  return false;
}

void ICACHE_FLASH_ATTR configWipe(void) 
{
  spi_flash_erase_sector(flashAddr() >> 12);
  spi_flash_erase_sector((flashAddr() + FLASH_SECT)>>12);
}

static int ICACHE_FLASH_ATTR selectPrimary(FlashFull *fc0, FlashFull *fc1);

bool ICACHE_FLASH_ATTR configRestore(void) {
  FlashFull ff0, ff1;
  uint8_t mac[6];
  
  // read both flash sectors
  if (spi_flash_read(flashAddr(), (void *)&ff0, sizeof(ff0)) != SPI_FLASH_RESULT_OK)
    memset(&ff0, 0, sizeof(ff0)); // clear in case of error
  if (spi_flash_read(flashAddr() + FLASH_SECT, (void *)&ff1, sizeof(ff1)) != SPI_FLASH_RESULT_OK)
    memset(&ff1, 0, sizeof(ff1)); // clear in case of error
  // figure out which one is good
  flash_pri = selectPrimary(&ff0, &ff1);
  // if neither is OK, we revert to defaults
  if (flash_pri < 0) 
  {
    NODE_DBG("Flash config broken - using defaults\n");
    os_memcpy(&flashConfig, &flashDefault, sizeof(FlashConfig));
    wifi_get_macaddr(SOFTAP_IF, mac);
    os_sprintf((char *)flashConfig.ssid, "POLI_%02X%02X%02X", mac[0],mac[1],mac[2]);
#ifdef CHIP_IN_HOSTNAME
    os_sprintf((char *)flashConfig.hostname, "ESP_%02X%02X%02X", mac[0],mac[1],mac[2]);
#endif
    flash_pri = 0;
    return false;
  }
  // copy good one into global var and return
  NODE_DBG("Flash config loaded = %d\n", flash_pri);
  os_memcpy(&flashConfig, flash_pri == 0 ? &ff0.fc : &ff1.fc, sizeof(FlashConfig));
  return true;
}

static int ICACHE_FLASH_ATTR selectPrimary(FlashFull *ff0, FlashFull *ff1) {
  // check CRC of ff0
  uint16_t crc = ff0->fc.crc;
  ff0->fc.crc = 0;
  bool ff0_crc_ok = crc16_data((unsigned char *)ff0, sizeof(FlashFull), 0) == crc;

  // check CRC of ff1
  crc = ff1->fc.crc;
  ff1->fc.crc = 0;
  bool ff1_crc_ok = crc16_data((unsigned char *)ff1, sizeof(FlashFull), 0) == crc;
  NODE_DBG("FLASH chk=0x%04X crc=0x%04x full_sz=%d sz=%d\n",
      crc16_data((unsigned char*)ff0, sizeof(FlashFull), 0),
      crc,
      sizeof(FlashFull),
      sizeof(FlashConfig));

  // decided which we like better
  if (ff0_crc_ok)
    if (!ff1_crc_ok || ff0->fc.seq >= ff1->fc.seq)
      return 0; // use first sector as primary
    else
      return 1; // second sector is newer
  else
    return ff1_crc_ok ? 1 : -1;
}
