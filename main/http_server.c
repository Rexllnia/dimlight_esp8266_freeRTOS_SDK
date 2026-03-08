/* Persistent Sockets Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "persistent_config.h"
#include <esp_http_server.h>
#include <ctype.h>

static const char *TAG="APP";

/* Function to free context */
void adder_free_func(void *ctx)
{
    ESP_LOGI(TAG, "/adder Free Context function called");
    free(ctx);
}

static const char *wifi_form_html =
"<html>"
"<body>"
"<h2>WiFi Config</h2>"
"<form id=\"wifiForm\" onsubmit=\"saveConfig(); return false;\">"

"SSID:<br>"
"<input type=\"text\" id=\"ssid\"><br>"

"PASSWORD:<br>"
"<input type=\"password\" id=\"password\"><br>"

"Server IPv6:<br>"
"<input type=\"text\" id=\"server_ipv6\" placeholder=\"240e:xxxx::1\"><br>"

"Server Port:<br>"
"<input type=\"number\" id=\"port\" value=\"12345\"><br><br>"

"<input type=\"submit\" value=\"Save\">"
"</form>"

"<script>"
"function saveConfig(){"
" var ssid=document.getElementById('ssid').value;"
" var password=document.getElementById('password').value;"
" var ipv6=document.getElementById('server_ipv6').value;"
" var port=document.getElementById('port').value;"

" var body='ssid='+encodeURIComponent(ssid)"
"+'&password='+encodeURIComponent(password)"
"+'&server_ipv6='+encodeURIComponent(ipv6)"
"+'&port='+encodeURIComponent(port);"

" fetch('/wifi',{"
" method:'POST',"
" headers:{'Content-Type':'application/x-www-form-urlencoded'},"
" body:body"
" }).then(r=>r.text()).then(d=>alert('Saved'));"
"}"
"</script>"
"</body>"
"</html>";

void url_decode(char *dst, const char *src)
{
    char a, b;

    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {

            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';

            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';

            *dst++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+') {   // 空格
            *dst++ = ' ';
            src++;
        }
        else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}
esp_err_t wifi_page_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, wifi_form_html, strlen(wifi_form_html));
    return ESP_OK;
}
/* Maintain a variable which stores the number of times
 * the "/adder" URI has been visited */
esp_err_t wifi_save_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;
    int offset = 0;

    if (remaining >= sizeof(buf)) {
        return ESP_FAIL;
    }

    /* 读取完整 POST 数据 */
    while (remaining > 0) {

        ret = httpd_req_recv(req, buf + offset, remaining);

        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }

        offset += ret;
        remaining -= ret;
    }

    buf[offset] = '\0';

    ESP_LOGI(TAG, "POST DATA: %s", buf);

    char ssid[32] = {0};
    char password[64] = {0};
    char server_ipv6[64] = {0};
    char port_str[8] = {0};



    /* 解析表单 */
    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "password", password, sizeof(password));
    httpd_query_key_value(buf, "server_ipv6", server_ipv6, sizeof(server_ipv6));
    httpd_query_key_value(buf, "port", port_str, sizeof(port_str));

    uint16_t port = atoi(port_str);
    /* URL decode */
    char ssid_decoded[32];
    char pass_decoded[64];
    char server_ipv6_decoded[64];

    

    url_decode(ssid_decoded, ssid);
    url_decode(pass_decoded, password);
    url_decode(server_ipv6_decoded, server_ipv6);

    ESP_LOGI(TAG, "SSID=%s", ssid_decoded);
    ESP_LOGI(TAG, "PASS=%s", pass_decoded);
    ESP_LOGI(TAG, "server_ipv6=%s:%d", server_ipv6_decoded,port);


    /* 保存到 flash */
    wifi_cfg_save(ssid_decoded, pass_decoded);
    server_addr_cfg_save(server_ipv6_decoded,port);

    /* 清除配置标志 */
    save_flag(0);

    /* 返回网页 */
    const char resp[] = "WiFi Saved. Reboot device.";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

httpd_uri_t wifi_page = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = wifi_page_get_handler,
};
httpd_uri_t wifi_save = {
    .uri       = "/wifi",
    .method    = HTTP_POST,
    .handler   = wifi_save_post_handler,
};
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    httpd_handle_t server;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &wifi_page);
        httpd_register_uri_handler(server, &wifi_save);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static httpd_handle_t server = NULL;

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void http_server()
{
    server = start_webserver();
}
