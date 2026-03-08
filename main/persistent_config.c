#include "persistent_config.h"
#include "nvs.h"
#include "nvs_flash.h"

uint8_t load_flag()
{
    nvs_handle_t handle;
    uint8_t flag = 0;

    if (nvs_open("config", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "flag", &flag);
        nvs_close(handle);
    }

    return flag;
}

void save_flag(uint8_t flag)
{
    nvs_handle_t handle;

    nvs_open("config", NVS_READWRITE, &handle);

    nvs_set_u8(handle, "flag", flag);

    nvs_commit(handle);

    nvs_close(handle);
}

bool wifi_cfg_load(char ssid[WIFI_SSID_MAX_LEN + 1],
                   char password[WIFI_PASS_MAX_LEN + 1])
{
    nvs_handle_t handle;

    if (nvs_open("config", NVS_READONLY, &handle) != ESP_OK)
        return false;

    size_t len;

    len = WIFI_SSID_MAX_LEN + 1;
    if (nvs_get_str(handle, "wifi_ssid", ssid, &len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    len = WIFI_PASS_MAX_LEN + 1;
    if (nvs_get_str(handle, "wifi_pass", password, &len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

void wifi_cfg_save(const char *ssid, const char *password)
{
    nvs_handle_t handle;

    if (nvs_open("config", NVS_READWRITE, &handle) != ESP_OK)
        return;

    nvs_set_str(handle, "wifi_ssid", ssid);
    nvs_set_str(handle, "wifi_pass", password);

    nvs_commit(handle);
    nvs_close(handle);
}

void server_addr_cfg_save(const char *ipv6_addr,int port)
{
    nvs_handle_t handle;

    if (nvs_open("config", NVS_READWRITE, &handle) != ESP_OK)
        return;

    nvs_set_str(handle, "ipv6", ipv6_addr);
    nvs_set_i32(handle, "port", port);

    nvs_commit(handle);
    nvs_close(handle);
}

bool server_addr_cfg_load(char ipv6_addr[64],int*port)
{
    nvs_handle_t handle;

    if (nvs_open("config", NVS_READONLY, &handle) != ESP_OK)
        return false;

    size_t len;

    len = 64;
    if (nvs_get_str(handle, "ipv6", ipv6_addr, &len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    if (nvs_get_i32(handle, "port", port) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    
    nvs_close(handle);
    return true;
}