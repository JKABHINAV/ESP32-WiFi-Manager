#include <string.h>
#include <stdlib.h> // Required for atoi()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

static const char *TAG = "WIFI_MGR";

void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
    }
}

bool get_wifi_creds(const char* path, char* ssid, char* pwd) {
    FILE* f = fopen(path, "r");
    if (f == NULL) return false;
    // Notice the added \r inside the quotes!
    if (fgets(ssid, 32, f)) ssid[strcspn(ssid, "\r\n")] = 0;
    if (fgets(pwd, 64, f))  pwd[strcspn(pwd, "\r\n")] = 0;
    fclose(f);
    return strlen(ssid) > 0;
}

// Handler for the Root page (Loads your HTML)
esp_err_t root_get_handler(httpd_req_t *req) {
    FILE* f = fopen("/spiffs/html/index.html", "r");
    if (f == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// NEW: Status API that returns a JSON string of the current connection
esp_err_t status_get_handler(httpd_req_t *req) {
    wifi_ap_record_t ap_info;
    char json_response[128];

    // Check if the Station (STA) is actually connected to a router
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        // It is connected! Send back the name of the network.
        snprintf(json_response, sizeof(json_response), 
                 "{\"status\":\"connected\", \"ssid\":\"%s\"}", ap_info.ssid);
    } else {
        // It is disconnected or trying to connect.
        snprintf(json_response, sizeof(json_response), 
                 "{\"status\":\"disconnected\", \"ssid\":\"none\"}");
    }

    // Tell the browser we are sending JSON data, not HTML
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler that performs the actual network switch
esp_err_t switch_get_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len;
    char   param[32];
    int    id = 1;

    // 1. Figure out which button was pressed (?id=1 or ?id=2)
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK) {
                id = atoi(param);
            }
        }
        free(buf);
    }

    char ssid[32], pwd[64];
    const char* file_path = (id == 2) ? "/spiffs/wifi/wifi2.txt" : "/spiffs/wifi/wifi1.txt";

    // 2. Read the credentials and trigger the switch
    if (get_wifi_creds(file_path, ssid, pwd)) {
        ESP_LOGI(TAG, "User clicked Manual Switch to Slot %d: %s", id, ssid);
        
        wifi_config_t sta_cfg = {0};
        strncpy((char*)sta_cfg.sta.ssid, ssid, 32);
        strncpy((char*)sta_cfg.sta.password, pwd, 64);
        
        esp_wifi_disconnect(); // Drop current connection
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_connect();    // Connect to new network

        // 3. Send a success message to the browser that auto-refreshes
        char resp[256];
        snprintf(resp, sizeof(resp), 
            "<html><head><meta http-equiv='refresh' content='3;url=/'></head>"
            "<body style='font-family: Arial; text-align: center; margin-top: 50px;'>"
            "<h2>Switching to %s...</h2><p>Please wait. Redirecting back to home...</p></body></html>", 
            ssid);
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send(req, "Error: Could not read credentials from SPIFFS.", HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

void wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = { .ssid = "ESP32_Config", .password = "12345678", .authmode = WIFI_AUTH_WPA_WPA2_PSK, .max_connection = 4 }
    };

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
}

void connect_logic() {
    char ssid[32], pwd[64];
    const char* files[] = {"/spiffs/wifi/wifi1.txt", "/spiffs/wifi/wifi2.txt"};

    for (int i = 0; i < 2; i++) {
        if (get_wifi_creds(files[i], ssid, pwd)) {
            ESP_LOGI(TAG, "Attempting Slot %d: %s", i+1, ssid);
            wifi_config_t sta_cfg = {0};
            strncpy((char*)sta_cfg.sta.ssid, ssid, 32);
            strncpy((char*)sta_cfg.sta.password, pwd, 64);
            
            esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
            esp_wifi_connect();
            vTaskDelay(pdMS_TO_TICKS(10000)); // 10s wait
            
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                ESP_LOGI(TAG, "Connected to %s!", ssid);
                return;
            }
        }
    }
    ESP_LOGE(TAG, "All WiFi failed. AP active at 192.168.4.1");
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    init_spiffs();
    wifi_init();
    
    // Start Webserver and register the routes
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        
        // Register the Root Page "/"
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &root);

        // Register the Switch action "/switch"
        httpd_uri_t switch_uri = { .uri = "/switch", .method = HTTP_GET, .handler = switch_get_handler };
        httpd_register_uri_handler(server, &switch_uri);

        // NEW: Register the Status API "/status"
        httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(server, &status_uri);
    }
    
    connect_logic();
}