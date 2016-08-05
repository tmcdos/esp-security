#define VER_STR2(V) #V
#define VER_STR(V) VER_STR2(V)
char esp_link_version[] = VER_STR(VERSION);
char esp_link_date[] = VER_STR(BUILD_DATE);
char esp_link_time[] = VER_STR(BUILD_TIME);
char esp_link_build[] = VER_STR(BUILD_NUM);
