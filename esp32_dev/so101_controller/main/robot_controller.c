#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "so_101_types.h"

static const char *TAG = "ROBOT_CONTROLLER";

/* --- Link Layer Configuration --- */
static uint8_t gateway_mac[]   = {0x44, 0x1D, 0x64, 0xF6, 0xFF, 0xFC};
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint32_t total_unicast_fails = 0;

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        if (memcmp(mac_addr, gateway_mac, 6) == 0) {
            total_unicast_fails++;
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

    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer = { .channel = 1, .encrypt = false, .ifidx = WIFI_IF_STA };
    
    memcpy(peer.peer_addr, gateway_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    memcpy(peer.peer_addr, broadcast_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void control_loop_task(void *pvParameters) {
    telemetry_data_t leader_data = { .origin_id = SOURCE_LEADER, .sequence_id = 0 };
    telemetry_data_t follower_data = { .origin_id = SOURCE_FOLLOWER, .sequence_id = 0 };
    
    char debug_str[64]; // ASCII Buffer
    float theta = 0.0f;
    const float phase_inc = 0.0628f;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);
    uint64_t last_time_us = esp_timer_get_time();

    while(1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        uint64_t start_time = esp_timer_get_time();
        uint32_t jitter = (uint32_t)(start_time - last_time_us - 20000);
        last_time_us = start_time;

        theta += phase_inc;
        if (theta > 2.0f * (float)M_PI) theta -= 2.0f * (float)M_PI;

        leader_data.sequence_id++;
        for(int i = 0; i < 6; i++) {
            leader_data.raw_positions[i] = (uint16_t)(2048 + (500.0f * sinf(theta + (i * 0.5f))));
        }

        esp_now_send(gateway_mac, (uint8_t *)&leader_data, sizeof(leader_data));

        /* --- ASCII Debug Shout every 200 cycles (~4 seconds) --- */
        if (leader_data.sequence_id % 200 == 0) {
            int len = snprintf(debug_str, sizeof(debug_str), 
                               "ID:%lu FAIL:%lu JIT:%luus THET:%.2f", 
                               leader_data.sequence_id, total_unicast_fails, jitter, theta);
            
            esp_now_send(broadcast_mac, (uint8_t *)debug_str, len);
        }
    }
}

void app_main(void) {
    init_wifi_espnow();
    xTaskCreatePinnedToCore(control_loop_task, "CONTROL_TASK", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 1);
}