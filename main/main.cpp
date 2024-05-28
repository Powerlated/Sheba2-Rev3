/*
  WiFiAccessPoint.ino creates a WiFi access point and provides a web server on it.

  Steps:
  1. Connect to the access point "yourAp"
  2. Point your web browser to http://192.168.4.1/H to turn the LED on or http://192.168.4.1/L to turn it off
     OR
     Run raw TCP "GET /H" and "GET /L" on PuTTY terminal with 192.168.4.1 as IP address and 80 as port

  Created for arduino-esp32 on 04 July, 2018
  by Elochukwu Ifediora (fedy0)
*/

#include <WiFi.h>
#include <NetworkClient.h>
#include <ESPAsyncWebServer.h>
#include <WiFiAP.h>
#include <pins.h>
#include <motor.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

// Set these to your desired credentials.
const char* serverSsid = "R2.4GHz";
const char* serverPassword = "rongwazi6511";
const char* ssid = "Sheba2Control";
const char* password = "Sheba2Control";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");
const size_t index_html_size = (index_html_end - index_html_start);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

#define STACK_SIZE 8192
StaticTask_t task_blinky_tcb;
StackType_t task_blinky_stack[STACK_SIZE];
StaticTask_t task_motors_tcb;
StackType_t task_motors_stack[STACK_SIZE];
StaticTask_t task_motor_info_tcb;
StackType_t task_motor_info_stack[STACK_SIZE];


void task_blinky(void) {
  while (true) {
    gpio_set_level((gpio_num_t)BLINKY, 1);
    vTaskDelay(250);
    gpio_set_level((gpio_num_t)BLINKY, 0);
    vTaskDelay(250);
  }
}

int motor_left_power;
int motor_right_power;
float drive_x, drive_y, turn_x, turn_y;
uint32_t motor_power_updated_time;
const uint32_t MOTOR_TIMEOUT_MS = 500;

void task_motors(void) {
  while (true) {
    if (xTaskGetTickCount() < motor_power_updated_time + MOTOR_TIMEOUT_MS) {
      motor_power(&motor_left, motor_left_power);
      motor_power(&motor_right, motor_right_power);
    }
    else {
      motor_power(&motor_left, 0);
      motor_power(&motor_right, 0);
    }
    vTaskDelay(1);
  }
}

void task_motor_info(void) {
  while (true) {
    ESP_LOGI("main", "Drive: %f %f", drive_x, drive_y);
    ESP_LOGI("main", "Turn: %f %f", turn_x, turn_y);
    vTaskDelay(250);
  }
}

void update_motor_power() {
  motor_left_power = drive_y * 255 + turn_x * 255;
  motor_right_power = drive_y * 255 + -turn_x * 255;

  motor_left_power = max(-255, min(255, motor_left_power));
  motor_right_power = max(-255, min(255, motor_right_power));

  motor_power_updated_time = xTaskGetTickCount();
}

void drive(float x, float y) {
  drive_x = x;
  drive_y = y;
  update_motor_power();
}

void turn(float x, float y) {
  turn_x = x;
  turn_y = y;
  update_motor_power();
}

void connect_to_wifi() {
  ESP_LOGI("main", "Connecting to WiFi. SSID: %s", serverSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(serverSsid, serverPassword);

  ESP_LOGI("main", "Waiting for successful connection...");
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(1);
  }

  ESP_LOGI("main", "WiFi connected.");

  ESP_LOGI("main", "Gateway IP address: %s", WiFi.STA.gatewayIP().toString().c_str());
  ESP_LOGI("main", "Client IP address: %s", WiFi.STA.localIP().toString().c_str());
}

void start_wifi_ap() {
  ESP_LOGI("main", "Configuring access point, SSID: %s", ssid);

  // // You can remove the password parameter if you want the AP to be open.
  // // a valid password must have more than 7 characters
  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  IPAddress myIP = WiFi.softAPIP();
  ESP_LOGI("main", "AP IP address: %s", myIP.toString().c_str());
}

void init_peripherals() {
  esp_log_level_set("gpio", ESP_LOG_WARN);

  gpio_config_t gpio = {
  .pin_bit_mask =
      BLINKY_BITMASK |
      M1_FWD_BITMASK |
      M1_REV_BITMASK |
      M2_FWD_BITMASK |
      M2_REV_BITMASK,
  .mode = GPIO_MODE_OUTPUT,
  .pull_up_en = GPIO_PULLUP_ENABLE,
  .pull_down_en = GPIO_PULLDOWN_ENABLE,
  .intr_type = GPIO_INTR_DISABLE
  };
  ESP_ERROR_CHECK(gpio_config(&gpio));

  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 50000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  xTaskCreateStatic(
    (TaskFunction_t)task_blinky,       /* Function that implements the task. */
    "blinky",          /* Text name for the task. */
    STACK_SIZE,      /* Number of indexes in the xStack array. */
    NULL,    /* Parameter passed into the task. */
    tskIDLE_PRIORITY,/* Priority at which the task is created. */
    task_blinky_stack,          /* Array to use as the task's stack. */
    &task_blinky_tcb);  /* Variable to hold the task's data structure. */

  xTaskCreateStatic(
    (TaskFunction_t)task_motors,
    "motors",
    STACK_SIZE,
    NULL,
    tskIDLE_PRIORITY,
    task_motors_stack,
    &task_motors_tcb);

  xTaskCreateStatic(
    (TaskFunction_t)task_motor_info,
    "motor_info",
    STACK_SIZE,
    NULL,
    tskIDLE_PRIORITY,
    task_motor_info_stack,
    &task_motor_info_tcb);
}

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    std::string_view s{ (const char*)data };
    float x, y;

    if (s.starts_with("drive")) {
      sscanf((const char*)data, "drive %f %f", &x, &y);
      drive(x, y);
    }
    else if (s.starts_with("turn")) {
      sscanf((const char*)data, "turn %f %f", &x, &y);
      turn(x, y);
    }
  }
}
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    ESP_LOGI("main", "WebSocket client #%lu connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    ESP_LOGI("main", "WebSocket client #%lu disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void setup() {
  init_peripherals();
  connect_to_wifi();

  /*
   * Web Sockets
   */
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html_start);
    });

  // Start server
  server.begin();

  ESP_LOGI("main", "Server started");
}

void loop() {
  vTaskDelay(1);

  ws.cleanupClients();
}