# The Wifi station configuration can be hard-coded here, which makes esp-link come up in STA+AP
# mode trying to connect to the specified AP *only* if the flash wireless settings are empty!
# This happens on a full serial flash and avoids having to hunt for the AP...
# STA_SSID ?=
# STA_PASS ?= 

# The SOFTAP configuration can be hard-coded here, the minimum parameters to set are AP_SSID && AP_PASS
# The AP SSID has to be at least 8 characters long, same for AP PASSWORD
# The AP AUTH MODE can be set to:
#  0 = AUTH_OPEN, 
#  1 = AUTH_WEP, 
#  2 = AUTH_WPA_PSK, 
#  3 = AUTH_WPA2_PSK, 
#  4 = AUTH_WPA_WPA2_PSK
# SSID hidden default 0, ( 0 | 1 ) 
# Max connections default 4, ( 1 ~ 4 )
# Beacon interval default 100, ( 100 ~ 60000ms )
#
# AP_SSID ?=esp_link_test
# AP_PASS ?=esp_link_test
# AP_AUTH_MODE ?=4
# AP_SSID_HIDDEN ?=0
# AP_MAX_CONN ?=4
# AP_BEACON_INTERVAL ?=100

# If CHANGE_TO_STA is set to "yes" the esp-link module will switch to station mode
# once successfully connected to an access point. Else it will stay in STA+AP mode.
CHANGE_TO_STA ?= yes

# hostname or IP address for wifi flashing
ESP_HOSTNAME        ?= esp-link
