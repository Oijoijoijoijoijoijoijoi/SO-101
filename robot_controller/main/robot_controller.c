#include <string.h>
#include <inttypes.h>  // Required for PRIu32 macro
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"   // Required for MACSTR and MAC2STR macros
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "so_101_types.h"

static const char *TAG = "ROBOT_CONTROLLER";
static uint32_t failure_count = 0;

/** * Target Gateway MAC Address. 
 * Ensure this matches the address output by the telemetry_gateway logs.
 */
static uint8_t gateway_mac[] = {0x3c, 0x71, 0xbf, 0x86, 0x7b, 0x18};

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
            failure_count++;
            // Rate limit error logging to approximately once every 50 failures (~1 second)
            if (failure_count % 200 == 0) {
                ESP_LOGE(TAG, "Periodic Transmission Failure (Total: %" PRIu32 ") for Peer: " MACSTR, 
                        failure_count, MAC2STR(mac_addr));
            }
        }
}

void init_wifi_espnow(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, gateway_mac, 6);
    peer_info.channel = 0; 
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

void app_main(void) {
    init_wifi_espnow(); 
    telemetry_data_t mock_data = { .sequence_id = 0 };
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // f = 50Hz, T = 20ms

    while(1) {
        // Deterministic delay to prevent cumulative drift
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        mock_data.sequence_id++;
        for(int i = 0; i < 6; i++) {
            mock_data.raw_positions[i] = 2048 + (i * 100); 
        }
        mock_data.timestamp_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        
        esp_now_send(gateway_mac, (uint8_t *)&mock_data, sizeof(mock_data));

        // Decimated Debug Print: Executes once per 50 cycles (1Hz)
        if (mock_data.sequence_id % 200 == 0) {
            ESP_LOGI(TAG, "Heartbeat: Packet %" PRIu32 " dispatched at %" PRIu32 " ms", 
                     mock_data.sequence_id, 
                     mock_data.timestamp_ms);
        }
    }
}