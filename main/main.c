#include "tcp_client.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi.h"
#include "http_server.h"
#include "persistent_config.h"
#include "ledc.h"

static xQueueHandle gpio_evt_queue = NULL;
static SemaphoreHandle_t gpio_sem;

#define EXAMPLE_ESP_WIFI_SSID      "hackwifi"
#define EXAMPLE_ESP_WIFI_PASS      "password"
#define EXAMPLE_MAX_STA_CONN       1

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}


static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // hw_timer_init(hw_timer_callback3, arg);
    // gpio_set_intr_type(gpio_num,GPIO_INTR_DISABLE);

    // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    xSemaphoreGiveFromISR(gpio_sem, NULL);

    // ESP_LOGI(TAG, "Set hw_timer timing time 1ms with one-shot");
    // hw_timer_alarm_us(50000, 0);
}
// void hw_timer_callback3(void *arg)
// {
//     uint32_t gpio_num = (uint32_t) arg;
//     if (gpio_get_level(gpio_num) == 0) {
//         xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
//     }
// }
void task(void *pvParameters) {

    tcp_client_init();

    while (1)
    {
        tcp_client_recv();
    }

    vTaskDelete(NULL);
}
enum {
    ButtonRelease,
    ButtonReleaseCheck,
    ButtonPress,
};
static void gpio_task_example(void *arg)
{
    static int count = 0,state = ButtonRelease;
    for (;;) {
        if (xSemaphoreTake(gpio_sem, portMAX_DELAY)) {
            state = ButtonPress;
            while (state != ButtonRelease) {
                vTaskDelay(500 / portTICK_RATE_MS);

                if (gpio_get_level(GPIO_INPUT_IO_1) == 0) {
                    ESP_LOGI(TAG, "count %d \n", count);
                    state = ButtonPress;
                    count++;
                }
                if (gpio_get_level(GPIO_INPUT_IO_1) == 1) {
                    state = ButtonReleaseCheck;
                    vTaskDelay(500 / portTICK_RATE_MS);
                    if (gpio_get_level(GPIO_INPUT_IO_1) == 1) {
                        if (count < 6 && count > 0) {
                            ESP_LOGI(TAG, "GPIO[%d] intr \n", GPIO_INPUT_IO_1);
                            tcp_client_gpio_toggle_report();   
                            count=0;
                            state = ButtonRelease;
                            break;
                        }
                        if (count >= 6) {
                            ESP_LOGI(TAG, "wifi config mode \n");
                            save_flag(1);
                            count=0;
                            state = ButtonRelease;
                            break;
                        }
                        
                    } else {
                        state = ButtonPress;
                    }   
                }
            }
        }
    }
}


void app_main()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_IO_0,0);
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    // gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_sem = xSemaphoreCreateBinary();
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 1024, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *) GPIO_INPUT_IO_1);


    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    if (load_flag()) {
        wifi_init_softap();
        http_server();

    } else {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        char ssid[WIFI_SSID_MAX_LEN];
        char password[WIFI_PASS_MAX_LEN];
        wifi_cfg_load(ssid,password);
        example_set_connection_info(ssid,password);

        ESP_ERROR_CHECK(example_connect());


        xTaskCreate(task, "tcp_client", 2048, NULL, 2, NULL);
    }


}
