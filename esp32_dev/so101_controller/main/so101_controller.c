#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_system.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "so_101_types.h"

static const char *TAG = "SO101_CTRL";

/* --- Technical Constants --- */
#define PHASE_INCREMENT    0.0157f  // 0.5Hz oscillation @ 200Hz sampling
#define CENTER_VALUE       2048
#define AMPLITUDE          500
#define TARGET_PERIOD_US   5000     // 5ms cycle time

/* --- Network Addressing --- */
static uint8_t gateway_mac[]   = {0x44, 0x1D, 0x64, 0xF6, 0xFF, 0xFC};
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* --- Global Diagnostic Accumulators --- */
static volatile uint32_t total_unicast_intent = 0; 
static volatile uint32_t total_unicast_fails  = 0; 

/**
 * @brief Hardware Acknowledgement Callback.
 * Triggers upon receipt of 802.11 ACK or reaching max retry limit.
 */
void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, gateway_mac, 6) == 0) {
        if (status != ESP_NOW_SEND_SUCCESS) {
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
    
    // Physical Layer Configuration
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // Disable Power Save for low-latency
    
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer = { .channel = 1, .ifidx = WIFI_IF_STA, .encrypt = false };
    
    memcpy(peer.peer_addr, gateway_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    memcpy(peer.peer_addr, broadcast_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void control_loop_task(void *pvParameters) {
    dual_telemetry_t bundled_data = {
        .leader = { .origin_id = SOURCE_LEADER, .sequence_id = 0 },
        .follower = { .origin_id = SOURCE_FOLLOWER, .sequence_id = 0 }
    };
    
    char debug_str[100];
    float theta = 0.0f;
    uint32_t cycle_idx = 0;
    
    /* Snapshot buffers for Delta-PLR calculation */
    uint32_t last_intent = 0;
    uint32_t last_fails  = 0;
    uint32_t max_jitter  = 0;
    uint32_t max_exec    = 0;
    uint32_t min_heap    = 0xFFFFFFFF;
    
    esp_reset_reason_t reason = esp_reset_reason();
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint64_t last_time_us = esp_timer_get_time();

    while(1) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5));
        uint64_t start_time = esp_timer_get_time();
        
        /* 1. Jitter Analysis */
        int64_t actual_delta = (int64_t)(start_time - last_time_us);
        int32_t signed_jitter = (int32_t)(actual_delta - TARGET_PERIOD_US);
        uint32_t abs_jitter = (uint32_t)(signed_jitter < 0 ? -signed_jitter : signed_jitter);
        if (abs_jitter > max_jitter) max_jitter = abs_jitter;
        last_time_us = start_time;

        /* 2. Kinematic Synthesis */
        theta += PHASE_INCREMENT;
        if (theta > 2.0f * (float)M_PI) theta -= 2.0f * (float)M_PI;
        for(int i = 0; i < 6; i++) {
            bundled_data.leader.raw_positions[i] = (uint16_t)(CENTER_VALUE + (AMPLITUDE * sinf(theta + (i * 0.5f))));
            bundled_data.follower.raw_positions[i] = (uint16_t)(CENTER_VALUE + (AMPLITUDE * sinf(theta + (i * 0.5f) - 0.2f)));
        }

        /* 3. TDM Scheduling */
        if (cycle_idx % 4 == 0) { // Telemetry Slot (50Hz)
            uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
            bundled_data.leader.timestamp_ms = bundled_data.follower.timestamp_ms = now;
            bundled_data.leader.sequence_id++;
            bundled_data.follower.sequence_id++;

            total_unicast_intent++;
            esp_err_t err = esp_now_send(gateway_mac, (uint8_t *)&bundled_data, sizeof(bundled_data));
            if (err != ESP_OK) total_unicast_fails++;
        } 
        else if (cycle_idx == 2) { // Diagnostic Slot (1Hz)
            uint32_t current_intent = total_unicast_intent;
            uint32_t current_fails  = total_unicast_fails;
            
            uint32_t delta_intent = current_intent - last_intent;
            uint32_t delta_fails  = current_fails - last_fails;
            
            float plr = (delta_intent > 0) ? ((float)delta_fails / delta_intent) * 100.0f : 100.0f;
            uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
            uint32_t heap_now = esp_get_free_heap_size();

            snprintf(debug_str, sizeof(debug_str), "UP:%lus|J:%lu|E:%lu|H:%lu|R:%d|L:%.1f%%", 
                     uptime_sec, max_jitter, max_exec, heap_now, (int)reason, plr);
            
            esp_now_send(broadcast_mac, (uint8_t *)debug_str, strlen(debug_str));
            ESP_LOGI(TAG, "Health: %s", debug_str);

            last_intent = current_intent;
            last_fails = current_fails;
            max_jitter = 0; 
            max_exec = 0;
        }

        /* 4. Performance Profiling */
        uint32_t exec_us = (uint32_t)(esp_timer_get_time() - start_time);
        if (exec_us > max_exec) max_exec = exec_us;
        if (++cycle_idx >= 200) cycle_idx = 0;
    }
}

void app_main(void) {
    init_wifi_espnow();
    xTaskCreatePinnedToCore(control_loop_task, "CTRL_TASK", 4096, NULL, 15, NULL, 1);
}