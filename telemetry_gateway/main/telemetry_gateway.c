#include <string.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "so_101_types.h"

// micro-ROS headers
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <sensor_msgs/msg/joint_state.h>
#include <rmw_microros/rmw_microros.h>

static const char *TAG = "GATEWAY_BRIDGE";

/* --- Kinematic Constants --- */
#define RAW_TO_RAD (2.0f * 3.14159265f / 4096.0f)
#define RAW_CENTER 2048
#define SO101_DOMAIN_ID 100
#define SO101_CLIENT_KEY 0xBA5EBA11

/* --- Error Handling Macros --- */
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){ \
    esp_log_level_set("RCCHECK", ESP_LOG_ERROR); \
    ESP_LOGE("RCCHECK", "Failed status on line %d: %d. Aborting.", __LINE__, (int)temp_rc); \
    vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){ \
    esp_log_level_set("RCSOFTCHECK", ESP_LOG_WARN); \
    ESP_LOGW("RCSOFTCHECK", "Failed status on line %d: %d. Continuing.", __LINE__, (int)temp_rc);}}

/* --- ROS 2 Global State --- */
static rcl_publisher_t leader_pub;
static rcl_publisher_t follower_pub;
static sensor_msgs__msg__JointState joint_state_msg;
static QueueHandle_t telemetry_queue;

#define NUM_JOINTS 6
static double pos_values[NUM_JOINTS];
static rosidl_runtime_c__String name_values[NUM_JOINTS];
static char* joint_names[] = {"joint_0", "joint_1", "joint_2", "joint_3", "joint_4", "joint_5"};

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(telemetry_data_t)) {
        if (xQueueSend(telemetry_queue, data, 0) != pdTRUE) {
            // Queue full: congestion drop
        }
    }
}

void microros_task(void * arg) {
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();

    // 1. Configure Persistence and Domain
    RCCHECK(rcl_init_options_init(&init_options, allocator));
    RCCHECK(rcl_init_options_set_domain_id(&init_options, SO101_DOMAIN_ID));
    
    rmw_init_options_t* rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    RCCHECK(rmw_uros_options_set_client_key(SO101_CLIENT_KEY, rmw_options));

    // 2. Deterministic Connection Retry Loop
    while (1) {
        if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) == RCL_RET_OK) {
            break; 
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 3. Node and Publisher Initialization
    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, "so101_gateway", "", &support));

    RCCHECK(rclc_publisher_init_default(&leader_pub, &node, 
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState), "leader/joint_states"));
    RCCHECK(rclc_publisher_init_default(&follower_pub, &node, 
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState), "follower/joint_states"));

    // 4. JointState Message Binding
    joint_state_msg.position.data = pos_values;
    joint_state_msg.position.size = NUM_JOINTS;
    joint_state_msg.position.capacity = NUM_JOINTS;
    
    joint_state_msg.name.data = name_values;
    joint_state_msg.name.size = NUM_JOINTS;
    joint_state_msg.name.capacity = NUM_JOINTS;
    for (int i = 0; i < NUM_JOINTS; i++) {
        joint_state_msg.name.data[i].data = joint_names[i];
        joint_state_msg.name.data[i].size = strlen(joint_names[i]);
        joint_state_msg.name.data[i].capacity = strlen(joint_names[i]) + 1;
    }

    // 5. Clock Synchronization
    rmw_uros_sync_session(1000);

    telemetry_data_t raw_data;
    while(1) {
        if (xQueueReceive(telemetry_queue, &raw_data, portMAX_DELAY)) {
            // High-resolution timestamping
            int64_t ns = rmw_uros_epoch_nanos();
            joint_state_msg.header.stamp.sec = (int32_t)(ns / 1000000000);
            joint_state_msg.header.stamp.nanosec = (uint32_t)(ns % 1000000000);
            
            for (int i = 0; i < NUM_JOINTS; i++) {
                joint_state_msg.position.data[i] = (double)((int32_t)raw_data.raw_positions[i] - RAW_CENTER) * RAW_TO_RAD;
            }

            if (raw_data.origin_id == SOURCE_LEADER) {
                joint_state_msg.header.frame_id.data = "leader_base";
                RCSOFTCHECK(rcl_publish(&leader_pub, &joint_state_msg, NULL));
            } else {
                joint_state_msg.header.frame_id.data = "follower_base";
                RCSOFTCHECK(rcl_publish(&follower_pub, &joint_state_msg, NULL));
            }
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
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void app_main(void) {
    // Suppress all UART logging to preserve binary protocol integrity
    esp_log_level_set("*", ESP_LOG_NONE); 

    telemetry_queue = xQueueCreate(20, sizeof(telemetry_data_t));
    init_wifi_espnow();
    xTaskCreatePinnedToCore(microros_task, "uros_task", 12288, NULL, 5, NULL, 0);
}