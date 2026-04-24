#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_littlefs.h"
#include <cJSON.h>
#include "so_101_types.h"

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <sensor_msgs/msg/joint_state.h>
#include <rmw_microros/rmw_microros.h>

/* Macro to safely absorb warn_unused_result attributes */
#define IGNORE_RET(x) do { rcl_ret_t _rc = (x); (void)_rc; } while(0)

static const int uart_port = UART_NUM_0;

/* --- ROS 2 Global Entities --- */
static rcl_publisher_t leader_pub;
static rcl_publisher_t follower_pub;

static sensor_msgs__msg__JointState leader_msg;
static sensor_msgs__msg__JointState follower_msg;
static char leader_topic[64];
static char follower_topic[64];

static QueueHandle_t telemetry_queue;
static rclc_support_t support;
static rcl_node_t node;
static rcl_allocator_t allocator;

#define SO101_DOMAIN_ID 100
#define SO101_CLIENT_KEY 0xBA5EBA11
#define NUM_JOINTS 6

typedef enum { WAITING_AGENT, AGENT_AVAILABLE, AGENT_CONNECTED, AGENT_DISCONNECTED } agent_state_t;

/* --- Transport Layer --- */
bool transport_open(struct uxrCustomTransport * transport) {
    uart_config_t uart_config = {
        .baud_rate = 921600, 
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    if (uart_param_config(uart_port, &uart_config) != ESP_OK) return false;
    if (!uart_is_driver_installed(uart_port)) {
        if (uart_driver_install(uart_port, 1024 * 4, 0, 0, NULL, 0) != ESP_OK) return false;
    }
    return true;
}

bool transport_close(struct uxrCustomTransport * transport) { return uart_driver_delete(uart_port) == ESP_OK; }
size_t transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err) {
    return uart_write_bytes(uart_port, (const char*) buf, len);
}
size_t transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err) {
    int rxBytes = uart_read_bytes(uart_port, buf, len, pdMS_TO_TICKS(timeout));
    return (rxBytes >= 0) ? rxBytes : 0;
}

/* --- ESP-NOW Receiver --- */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(telemetry_data_t)) {
        xQueueSend(telemetry_queue, data, 0);
    }
}

/* --- JSON Configuration Parser --- */
static void populate_entity_msg(cJSON *entity, sensor_msgs__msg__JointState *msg, char *topic_out) {
    cJSON *topic_json = cJSON_GetObjectItem(entity, "topic");
    cJSON *joints_json = cJSON_GetObjectItem(entity, "joints");
    int count = cJSON_GetArraySize(joints_json);

    strncpy(topic_out, topic_json->valuestring, 63);
    topic_out[63] = '\0';

    msg->name.data = (rosidl_runtime_c__String *)malloc(count * sizeof(rosidl_runtime_c__String));
    msg->position.data = (double *)malloc(count * sizeof(double));
    msg->name.size = count;
    msg->name.capacity = count;
    msg->position.size = count;
    msg->position.capacity = count;

    for (int i = 0; i < count; i++) {
        char* j_name = cJSON_GetArrayItem(joints_json, i)->valuestring;
        msg->name.data[i].data = strdup(j_name);
        msg->name.data[i].size = strlen(j_name);
        msg->name.data[i].capacity = msg->name.data[i].size + 1;
    }
}

void init_kinematics_from_json(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .format_if_mount_failed = false,
        .dont_mount = false  // Corrected member name
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    FILE* f = fopen("/storage/robot-joints.json", "r");
    if (!f) {
        ESP_LOGE("FS", "Failed to open /storage/robot-joints.json");
        return;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(fsize + 1);
    fread(data, 1, fsize, f);
    fclose(f);
    data[fsize] = 0;

    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE("FS", "JSON Parse Error");
        free(data);
        return;
    }

    populate_entity_msg(cJSON_GetObjectItem(root, "leader"), &leader_msg, leader_topic);
    populate_entity_msg(cJSON_GetObjectItem(root, "follower"), &follower_msg, follower_topic);

    cJSON_Delete(root);
    free(data);
    ESP_LOGI("FS", "Kinematic metadata loaded and buffers allocated.");
}

/* --- Lifecycle Management --- */
void teardown_microros() {
    IGNORE_RET(rcl_publisher_fini(&leader_pub, &node));
    IGNORE_RET(rcl_publisher_fini(&follower_pub, &node));
    IGNORE_RET(rcl_node_fini(&node));
    (void) rclc_support_fini(&support);
}

void microros_task(void * arg) {
    allocator = rcl_get_default_allocator();
    agent_state_t state = WAITING_AGENT;
    telemetry_data_t raw_data;

    while(1) {
        switch(state) {
            case WAITING_AGENT:
                if (rmw_uros_ping_agent(100, 1) == RCL_RET_OK) {
                    state = AGENT_AVAILABLE;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                break;

            case AGENT_AVAILABLE: {
                rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
                IGNORE_RET(rcl_init_options_init(&init_options, allocator));
                IGNORE_RET(rcl_init_options_set_domain_id(&init_options, SO101_DOMAIN_ID));
                rmw_init_options_t* rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
                (void) rmw_uros_options_set_client_key(SO101_CLIENT_KEY, rmw_options);

                if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) == RCL_RET_OK) {
                    if (rclc_node_init_default(&node, "so101_gateway", "", &support) == RCL_RET_OK) {
                        IGNORE_RET(rclc_publisher_init_default(&leader_pub, &node, 
                                ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState), leader_topic));
                        IGNORE_RET(rclc_publisher_init_default(&follower_pub, &node, 
                                ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState), follower_topic));
                        
                        // Force clock synchronization with host
                        rmw_uros_sync_session(1000);
                        state = AGENT_CONNECTED;
                    } else {
                        state = WAITING_AGENT;
                    }
                } else {
                    state = WAITING_AGENT;
                }
                IGNORE_RET(rcl_init_options_fini(&init_options));
                break;
            }

            case AGENT_CONNECTED:
                if (xQueueReceive(telemetry_queue, &raw_data, pdMS_TO_TICKS(100))) {
                    sensor_msgs__msg__JointState *active_msg = (raw_data.origin_id == SOURCE_LEADER) ? &leader_msg : &follower_msg;
                    rcl_publisher_t *active_pub = (raw_data.origin_id == SOURCE_LEADER) ? &leader_pub : &follower_pub;

                    int64_t ns = rmw_uros_epoch_nanos();
                    active_msg->header.stamp.sec = (int32_t)(ns / 1000000000);
                    active_msg->header.stamp.nanosec = (uint32_t)(ns % 1000000000);
                    
                    active_msg->header.frame_id.data = (char*)((raw_data.origin_id == SOURCE_LEADER) ? "leader_base" : "follower_base");
                    active_msg->header.frame_id.size = strlen(active_msg->header.frame_id.data);
                    active_msg->header.frame_id.capacity = active_msg->header.frame_id.size + 1;

                    for (int i = 0; i < NUM_JOINTS; i++) {
                        active_msg->position.data[i] = (double)((int32_t)raw_data.raw_positions[i] - 2048) * (2.0 * M_PI / 4096.0);
                    }

                    IGNORE_RET(rcl_publish(active_pub, active_msg, NULL));
                } else {
                    if (rmw_uros_ping_agent(50, 1) != RCL_RET_OK) state = AGENT_DISCONNECTED;
                }
                break;

            case AGENT_DISCONNECTED:
                teardown_microros();
                state = WAITING_AGENT;
                break;
        }
    }
}

/* --- Initialization --- */
void init_wifi_espnow(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_NONE); 
    
    // VFS MUST be mounted before FreeRTOS task scheduling begins
    init_kinematics_from_json();

    rmw_uros_set_custom_transport(true, NULL, transport_open, transport_close, transport_write, transport_read);
    telemetry_queue = xQueueCreate(40, sizeof(telemetry_data_t));
    init_wifi_espnow();
    xTaskCreatePinnedToCore(microros_task, "uros_task", 12288, NULL, 5, NULL, 1);
}