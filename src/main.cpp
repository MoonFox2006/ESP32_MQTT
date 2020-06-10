#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LED_PIN 22
#define LED_LEVEL LOW

#ifdef LED_PIN
#include "Led.h"
#endif

const char WIFI_SSID[] = "******";
const char WIFI_PSWD[] = "******";
const char MQTT_SERVER[] = "******";
const uint16_t MQTT_PORT = 1883;
const char MQTT_CLIENT[] = "ESP32_MQTT";

enum flags_t : uint8_t { FLAG_WIFI = 1, FLAG_MQTT = 2 };

EventGroupHandle_t flags;
portMUX_TYPE serialMux = portMUX_INITIALIZER_UNLOCKED;

#ifdef LED_PIN
const ledmode_t LED_WIFI_CONNECTING = LED_2HZ;
const ledmode_t LED_WIFI_CONNECTED = LED_1HZ;
const ledmode_t LED_MQTT_CONNECTING = LED_4HZ;
const ledmode_t LED_MQTT_CONNECTED = LED_BREATH;

LedHandle_t led;
#endif

static void wifiTaskHandler(void *pvParam) {
  const uint32_t WIFI_TIMEOUT = 30000; // 30 sec.

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  for (;;) {
    if (! WiFi.isConnected()) {
      xEventGroupClearBits(flags, FLAG_WIFI);
      WiFi.begin(WIFI_SSID, WIFI_PSWD);
      portENTER_CRITICAL(&serialMux);
      Serial.print("Connecting to SSID \"");
      Serial.print(WIFI_SSID);
      Serial.println("\"...");
      portEXIT_CRITICAL(&serialMux);
#ifdef LED_PIN
      led_set(led, LED_WIFI_CONNECTING);
#endif
      {
        uint32_t start = millis();

        while ((! WiFi.isConnected()) && (millis() - start < WIFI_TIMEOUT)) {
          vTaskDelay(pdMS_TO_TICKS(500));
        }
      }
      if (WiFi.isConnected()) {
        portENTER_CRITICAL(&serialMux);
        Serial.print("Connected to WiFi with IP ");
        Serial.println(WiFi.localIP());
        portEXIT_CRITICAL(&serialMux);
#ifdef LED_PIN
        led_set(led, LED_WIFI_CONNECTED);
#endif
        xEventGroupSetBits(flags, FLAG_WIFI);
      } else {
        WiFi.disconnect();
        Serial.println("Failed to connect to WiFi!");
#ifdef LED_PIN
        led_set(led, LED_OFF);
#endif
        vTaskDelay(pdMS_TO_TICKS(WIFI_TIMEOUT));
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

static void mqttTaskHandler(void *pvParam) {
  const uint32_t MQTT_TIMEOUT = 30000; // 30 sec.
  const uint32_t MQTT_INTERVAL = 5000; // 5 sec.

  WiFiClient client;
  PubSubClient mqtt(client);
  uint32_t lastPublish = 0;

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback([](char *topic, uint8_t *payload, unsigned int len) {
    portENTER_CRITICAL(&serialMux);
    Serial.print("MQTT topic \"");
    Serial.print(topic);
    Serial.print("\" with value \"");
    for (uint16_t i = 0; i < len; ++i)
      Serial.print((char)payload[i]);
    Serial.print("\" (");
    Serial.print(len);
    Serial.println(" byte(s)) received");
    portEXIT_CRITICAL(&serialMux);
  });
  for (;;) {
    if (xEventGroupWaitBits(flags, FLAG_WIFI, pdFALSE, pdTRUE, portMAX_DELAY) & FLAG_WIFI) {
      if (! mqtt.connected()) {
        xEventGroupClearBits(flags, FLAG_MQTT);
        portENTER_CRITICAL(&serialMux);
        Serial.print("Connecting to MQTT broker \"");
        Serial.print(MQTT_SERVER);
        Serial.println("\"...");
        portEXIT_CRITICAL(&serialMux);
#ifdef LED_PIN
        led_set(led, LED_MQTT_CONNECTING);
#endif
        if (mqtt.connect(MQTT_CLIENT)) {
          Serial.println("Connected to MQTT broker");
          xEventGroupSetBits(flags, FLAG_MQTT);
          if (! mqtt.subscribe("#")) {
            Serial.println("Error subscribing to all topics!");
          }
#ifdef LED_PIN
          led_set(led, LED_MQTT_CONNECTED);
#endif
        } else {
          Serial.println("Failed to connect to MQTT!");
#ifdef LED_PIN
          led_set(led, LED_WIFI_CONNECTED);
#endif
          vTaskDelay(pdMS_TO_TICKS(MQTT_TIMEOUT));
        }
      } else {
        uint32_t time;

        mqtt.loop();
        time = millis();
        if ((! lastPublish) || (time - lastPublish >= MQTT_INTERVAL)) {
          char str[11];

          if (! mqtt.publish("/uptime", ultoa(time, str, 10))) {
            Serial.println("Error publishing uptime topic!");
          }
          lastPublish = time;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

static void halt(const char *msg) {
#ifdef LED_PIN
  led_deinit(led);
#endif
  Serial.println(msg);
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial.println();

#ifdef LED_PIN
  {
    ledconfig_t ledconfig = LED_CONFIG(LED_PIN, LED_LEVEL);

    led = led_init(&ledconfig);
    if (! led)
      halt("Error initialization led!");
  }
#endif
  flags = xEventGroupCreate();
  if (! flags)
    halt("Error creating flags event group!");
  if (xTaskCreate(wifiTaskHandler, "WiFiTask", 8192, NULL, 1, NULL) != pdPASS)
    halt("Error creating WiFi task!");
  if (xTaskCreate(mqttTaskHandler, "MqttTask", 8192, NULL, 1, NULL) != pdPASS)
    halt("Error creating MQTT task!");
}

void loop() {
  vTaskDelete(NULL);
}
