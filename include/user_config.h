#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_
#include <c_types.h>
#ifdef __WIN32__
#include <_mingw.h>
#endif

# undef SHOW_HEAP_USE
#define DEBUG_SDK true

// If defined, the default hostname for DHCP will include the chip ID to make it unique
#undef CHIP_IN_HOSTNAME

extern char esp_link_version[];
extern char esp_link_date[];
extern char esp_link_time[];
extern char esp_link_build[];
extern uint8_t UTILS_StrToIP(const char* str, void *ip);

// ===================================================================

#define DEBUG_LOG
# define DEBUGIP

#ifdef DEBUG_LOG
#define NODE_DBG(...) os_printf_plus( __VA_ARGS__ )
#else
#define NODE_DBG
#endif

#endif
