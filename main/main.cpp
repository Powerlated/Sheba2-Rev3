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

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
const size_t index_html_size = (index_html_end - index_html_start);

NetworkServer server(80);

#define STACK_SIZE 8192
StaticTask_t task_blinky_tcb;
StackType_t task_blinky_stack[STACK_SIZE];
StaticTask_t task_motors_tcb;
StackType_t task_motors_stack[STACK_SIZE];

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
    } else {
      motor_power(&motor_left, 0);
      motor_power(&motor_right, 0);
    }
    vTaskDelay(1);
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

  while (WiFi.status() != WL_CONNECTED) {
    ESP_LOGI("main", "Connecting...");
    vTaskDelay(1000);
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
    (TaskFunction_t)task_motors,       /* Function that implements the task. */
    "motors",          /* Text name for the task. */
    STACK_SIZE,      /* Number of indexes in the xStack array. */
    NULL,    /* Parameter passed into the task. */
    tskIDLE_PRIORITY, /* Priority at which the task is created. */
    task_motors_stack,          /* Array to use as the task's stack. */
    &task_motors_tcb);  /* Variable to hold the task's data structure. */
}

void setup() {
  init_peripherals();
  connect_to_wifi();
  server.begin();
  ESP_LOGI("main", "Server started");
}

enum HttpState {
  AwaitingRequest,
  POST,
  POST_DATA,
  GET,
};

String headerLine1 = "";
String line = "";        // make a String to hold incoming data from the client
String postData = "";

const int TIMEOUT_MS = 1000;

void loop() {
  NetworkClient client = server.accept();  // listen for incoming clients

  if (client) {                     // if you get a client,
    // ESP_LOGI("http", "New client.");  // print a message out the serial port

    line = "";
    HttpState state = AwaitingRequest;

    uint32_t lastDataTick = xTaskGetTickCount();

    while (client.connected()) {    // loop while the client's connected
      if (client.available()) {     // if there's bytes to read from the client,
        char c = client.read();     // read a byte, then
        if (c == '\n') {            // if the byte is a newline character
          switch (state) {
          case AwaitingRequest:
            if (line.startsWith("GET")) {
              state = GET;
              ESP_LOGI("http", "GET request");
            }
            else if (line.startsWith("POST")) {
              state = POST;
              // ESP_LOGI("http", "POST request");
            }
            else {
              ESP_LOGI("http", "Invalid request: %s", line.c_str());
            }

            headerLine1 = line;
            break;
          case GET:
          case POST:
            if (line.length() == 0) {
              if (state == GET) {
                // if the current line is blank, you got two newline characters in a row.
                // that's the end of the client HTTP request, so send a response:

                ESP_LOGI("http", "GET data");

                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();

                // the content of the HTTP response follows the header:
                client.write(index_html_start, index_html_size);

                // The HTTP response ends with another blank line:
                client.println();

                client.stop();
              }
              else if (state == POST) {
                // ESP_LOGI("http", "POST data");
                state = POST_DATA;

                postData = "";
              }
              else {
                ESP_LOGE("http", "Bad request");

                client.println("HTTP/1.1 400 Bad Request");
                client.println("Content-type:text/html");
                client.println();
                client.println();

                client.stop();
              }
            }
            break;
          case POST_DATA:
            if (line.length() == 0) {
              float x, y;
              sscanf(postData.c_str(), "%f %f", &x, &y);

              if (headerLine1.startsWith("POST /drive")) {
                ESP_LOGI("http", "Drive: %f %f", x, y);
                drive(x, y);
              }
              else if (headerLine1.startsWith("POST /turn")) {
                ESP_LOGI("http", "Turn: %f %f", x, y);
                turn(x, y);
              }

              client.println("HTTP/1.1 201 Created");
              client.println("Content-type:text/html");
              client.println();
              client.println();

              client.stop();
            }
            else {
              // ESP_LOGI("http", "POST data received");
              postData += line;
            }
            break;
          }

          // ESP_LOGI("http", "%s", line.c_str());

          // if you got a newline, then clear currentLine:
          line = "";
        }
        else if (c != '\r') {  // if you got anything else but a carriage return character,
          line += c;      // add it to the end of the currentLine
        }

        lastDataTick = xTaskGetTickCount();
      }
      else {
        if (xTaskGetTickCount() > lastDataTick + TIMEOUT_MS) {
          ESP_LOGW("http", "Request timed out.");
          ESP_LOGW("http", "Line remaining: %s", line.c_str());
          break;
        }
      }
    }
    // close the connection:
    client.stop();
    // ESP_LOGI("main", "Client disconnected.");
  }

  vTaskDelay(1);
}