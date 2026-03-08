/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "driver/gpio.h"
#include "ledc.h"
#include "persistent_config.h"
#ifdef CONFIG_EXAMPLE_IPV4
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#else

#endif
/*
static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;
static const uint8_t D9   = 3;
static const uint8_t D10  = 1;
*/
#define GPIO_OUTPUT_IO_0    0
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)
#define GPIO_INPUT_IO_1     2
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_1)

#define PORT 12345

#define CAP_BRIGHTNESS  (1 << 0)
#define CAP_RGB         (1 << 1)

enum {
    REGISTER,
    STATE,
    CTRL,
    CONFIRM
};
typedef struct {
    uint8_t  msg_type;     // REGISTER / STATE / CTRL / CONFIRM
    uint32_t  sn;    // 设备SN
     union {
        struct {
            uint32_t capability;   // 能力位图
            union {
                uint8_t switch_on;      // 开关模式
                uint8_t brightness;     // 亮度模式
                struct {
                    uint8_t r;
                    uint8_t g;
                    uint8_t b;
                } rgb;                  // RGB模式
            } value;
        } state;
        struct {
            uint8_t reserved[24];
        } confirm;
    } payload;
} frame_t;

static const char *TAG = "example";

void tcp_client_init();
void tcp_client_send(void *payload,size_t payload_len);
void tcp_client_recv();
uint32_t get_chip_id32(void);
void set_send_gpio_level(bool gpio_level);
void tcp_client_gpio_toggle_report();