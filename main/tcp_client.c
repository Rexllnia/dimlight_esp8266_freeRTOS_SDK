#include "tcp_client.h"

static void tcp_client_connect();
static void reconnect_timer_start();
static void _reconnect_cb(void * arg);


static char rx_buffer[128];
static char addr_str[128];
static int addr_family;
static int ip_protocol;
static int sock;
static bool is_init =false;
static volatile bool need_reconnect =false;
static uint8_t brightness = 0;
static esp_timer_handle_t timer; 
static esp_timer_create_args_t timer_conf = {
        .callback = _reconnect_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reconnect_rm"
};
static SemaphoreHandle_t mutex = NULL;
static SemaphoreHandle_t need_reconnect_mutex = NULL;
static SemaphoreHandle_t gpio_set_mutex = NULL;


static void set_init_flag(bool flag)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    is_init = flag;
    xSemaphoreGive(mutex);
}

static void _reconnect_cb(void * arg)
{
    // 这个函数会在定时器触发时被调用
    printf("Timer fired!\n");
    xSemaphoreTake(need_reconnect_mutex,portMAX_DELAY);
    need_reconnect = true;
    xSemaphoreGive(need_reconnect_mutex);
}

void tcp_client_connect() 
{
    

#ifdef CONFIG_EXAMPLE_IPV4
    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
    struct sockaddr_in6 destAddr;
    char ipv6_addr[64];
    int port = 0;
    memset(ipv6_addr,0,sizeof(ipv6_addr));
    server_addr_cfg_load(ipv6_addr,&port);
    
    inet6_aton(ipv6_addr, &destAddr.sin6_addr);
    destAddr.sin6_family = AF_INET6;
    destAddr.sin6_port = htons(port);
    destAddr.sin6_scope_id = tcpip_adapter_get_netif_index(TCPIP_ADAPTER_IF_STA);
    addr_family = AF_INET6;
    ip_protocol = IPPROTO_IPV6;
    inet6_ntoa_r(destAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif
    ESP_LOGI(TAG, "tcp_client_connect");
    sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        set_init_flag(false);
        reconnect_timer_start();
        close(sock);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        shutdown(sock, 0);
        close(sock);
        set_init_flag(false);
        reconnect_timer_start();
        return;
    }
    xSemaphoreTake(gpio_set_mutex,portMAX_DELAY);
    frame_t packet;
    memset(&packet,0,sizeof(packet));
    packet.sn = get_chip_id32();
    printf("packet.sn %d \n",packet.sn);
    packet.msg_type = REGISTER;
    packet.payload.state.capability = CAP_BRIGHTNESS;
    packet.payload.state.value.brightness = brightness;

    int sent = send(sock, &packet,sizeof(packet), 0);
    ESP_LOGI(TAG, "Successfully connected");
    xSemaphoreGive(gpio_set_mutex);

    set_init_flag(true);
    return;
}


void tcp_client_gpio_toggle_report()
{
     
    frame_t packet;
    if (is_init == false) {
        return;
    }

    xSemaphoreTake(gpio_set_mutex,portMAX_DELAY);
    if (brightness == 0) {
        brightness = 100;
    } else {
        brightness = 0;
    }
    ledc_set_brightness(brightness);

    memset(&packet,0,sizeof(packet));
    packet.sn = get_chip_id32();
    printf("packet.sn %d \n",packet.sn);
    packet.msg_type = STATE;
    packet.payload.state.capability = CAP_BRIGHTNESS;
    packet.payload.state.value.brightness = brightness;
    int sent = send(sock, &packet,sizeof(packet), 0);

    ESP_LOGI(TAG, "Successfully connected");
    xSemaphoreGive(gpio_set_mutex);
}
void tcp_client_init()
{
    mutex = xSemaphoreCreateMutex();
    gpio_set_mutex = xSemaphoreCreateMutex();
    need_reconnect_mutex = xSemaphoreCreateMutex();
    ledc_init();

    esp_err_t err = esp_timer_create(&timer_conf, &timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer");
        return;
    }
  
    tcp_client_connect();
}




static void reconnect_timer_start() 
{
    int ret = 0;
    // 启动定时器
    printf("reconnect_timer_start\n");

    esp_timer_stop(timer);

    ret = esp_timer_start_once(timer, 3000*1000U);
    printf("esp_timer_start_once ret %d\n",ret);
}

void tcp_client_recv()
{
    int len = 0;
    frame_t packet; 
    uint8_t mac[6];

    if (is_init == false) {
        xSemaphoreTake(need_reconnect_mutex,portMAX_DELAY);
        if (need_reconnect == true) {
            need_reconnect = false;
            tcp_client_connect();
        }
        xSemaphoreGive(need_reconnect_mutex);
        return;
    }

    memset(&packet,0,sizeof(packet));

    len = recv(sock, &packet, sizeof(frame_t), 0);

    if (len < 0) {
        ESP_LOGE(TAG, "recv failed: errno %d", errno);
        set_init_flag(false);
        shutdown(sock, 0);
        close(sock);
        reconnect_timer_start();
        return;
    }

    ESP_LOGE(TAG,"recv msg_type%d\n",packet.msg_type);
    // Error occured during receiving
    
    // Data received
    
    switch (packet.msg_type)
    {   
    case CTRL:
        xSemaphoreTake(gpio_set_mutex,portMAX_DELAY);
        
        brightness = packet.payload.state.value.brightness;
        ledc_set_brightness(brightness);
        frame_t packet;
        memset(&packet,0,sizeof(packet));
        packet.sn = get_chip_id32();
        printf("packet.sn %d \n",packet.sn);
        printf("brightness %d \n",brightness);
        packet.payload.state.capability = CAP_BRIGHTNESS;
        packet.msg_type = STATE;
        packet.payload.state.value.brightness = brightness;
        int sent = send(sock, &packet,sizeof(packet), 0);
        xSemaphoreGive(gpio_set_mutex);
        break;      
    default:
        break;
    }
    
}
void tcp_client_send(void *payload,size_t payload_len)
{
    int err = send(sock, payload, payload_len, 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
        goto out;
    }

out:   
    return;
}
void tcp_client_close() 
{
    ESP_LOGE(TAG, "Shutting down socket and restarting...");
    shutdown(sock, 0);
    close(sock);
}
uint32_t get_chip_id32(void)
{
    uint8_t mac[6];

    // 读取芯片 eFuse 中的 base MAC
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // 取后 4 字节拼成 32 位
    return ((uint32_t)mac[2] << 24) |
           ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] << 8)  |
            mac[5];
}