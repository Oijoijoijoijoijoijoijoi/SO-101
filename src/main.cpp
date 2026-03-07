#include <Arduino.h>
#include <micro_ros_platformio.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <sensor_msgs/msg/joint_state.h>

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

// Global ROS 2 Entities
rcl_publisher_t publisher;
sensor_msgs__msg__JointState msg;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

#define LED_PIN 2

void error_loop() {
  while(1) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void setup() {
  // 1. Initialize Hardware Serial
  Serial.begin(921600);

  // 2. Initialize micro-ROS Serial Transport using the Hardware Serial object
  set_microros_serial_transports(Serial);

  pinMode(LED_PIN, OUTPUT);
  allocator = rcl_get_default_allocator();

  // 3. Initialize support system
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // 4. Initialize Node
  RCCHECK(rclc_node_init_default(&node, "so101_bridge", "", &support));

  // 5. Initialize Publisher
  RCCHECK(rclc_publisher_init_default(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState),
    "joint_states"));

  // 6. Pre-allocate memory for the JointState message
  static double pos_data[6];
  msg.position.capacity = 6;
  msg.position.data = pos_data;
  msg.position.size = 6;
}

void loop() {
  static float iteration = 0;
  
  // Create dummy sine wave data for each joint
  for(int i = 0; i < 6; i++) {
    msg.position.data[i] = sin(iteration + (i * 0.5));
  }
  iteration += 0.1;

  // Publish the message
  RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));

  delay(50); // 20Hz update frequency
}