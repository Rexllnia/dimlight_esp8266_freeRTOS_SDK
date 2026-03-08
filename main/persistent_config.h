#include "lwip/sys.h"


#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

uint8_t load_flag();
void save_flag(uint8_t flag);
bool wifi_cfg_load(char ssid[WIFI_SSID_MAX_LEN + 1],
                   char password[WIFI_PASS_MAX_LEN + 1]);

void wifi_cfg_save(const char *ssid, const char *password);
void server_addr_cfg_save(const char *ipv6_addr,int port);
bool server_addr_cfg_load(char ipv6_addr[64],int *port);