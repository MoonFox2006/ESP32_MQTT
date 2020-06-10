#include <Arduino.h>
#include "Led.h"

struct __attribute__((__packed__)) led_t {
  TaskHandle_t task;
  uint8_t pin;
  bool level : 1;
  ledmode_t mode : 7;
  uint8_t ledc_channel;
};

static const char TAG[] = "Led";

static void ledTask(void *pvParam) {
  const uint32_t LED_PULSE = 25; // 25 ms.
  const uint32_t FADE_STEP = 38; // 38 ms.
  const uint16_t LEDC_FREQ = 1000; // 1 kHz

  int16_t pwm;

  pinMode(((led_t*)pvParam)->pin, OUTPUT);
  if (((led_t*)pvParam)->mode <= LED_TOGGLE) {
    digitalWrite(((led_t*)pvParam)->pin, ((led_t*)pvParam)->mode == LED_ON ? ((led_t*)pvParam)->level : ! ((led_t*)pvParam)->level);
  } else if (((led_t*)pvParam)->mode >= LED_FADEIN) {
    ledcSetup(((led_t*)pvParam)->ledc_channel, LEDC_FREQ, 8);
    ledcAttachPin(((led_t*)pvParam)->pin, ((led_t*)pvParam)->ledc_channel);
    pwm = 0;
  }
  for (;;) {
    uint32_t notifyValue;

    if (xTaskNotifyWait(0, 0, &notifyValue, ((led_t*)pvParam)->mode <= LED_TOGGLE ? portMAX_DELAY : 0) == pdPASS) {
      if ((ledmode_t)notifyValue != ((led_t*)pvParam)->mode) {
        if (((led_t*)pvParam)->mode < LED_FADEIN) {
          if ((ledmode_t)notifyValue >= LED_FADEIN) {
            ledcSetup(((led_t*)pvParam)->ledc_channel, LEDC_FREQ, 8);
            ledcAttachPin(((led_t*)pvParam)->pin, ((led_t*)pvParam)->ledc_channel);
            pwm = 0;
          }
        } else {
          if ((ledmode_t)notifyValue < LED_FADEIN) {
            ledcDetachPin(((led_t*)pvParam)->pin);
          }
        }
        ((led_t*)pvParam)->mode = (ledmode_t)notifyValue;
      }
    }
    switch (((led_t*)pvParam)->mode) {
      case LED_OFF:
        digitalWrite(((led_t*)pvParam)->pin, ! ((led_t*)pvParam)->level);
        break;
      case LED_ON:
        digitalWrite(((led_t*)pvParam)->pin, ((led_t*)pvParam)->level);
        break;
      case LED_TOGGLE:
        digitalWrite(((led_t*)pvParam)->pin, ! digitalRead(((led_t*)pvParam)->pin));
        break;
      case LED_05HZ:
      case LED_1HZ:
      case LED_2HZ:
      case LED_4HZ:
        digitalWrite(((led_t*)pvParam)->pin, ((led_t*)pvParam)->level);
        vTaskDelay(pdMS_TO_TICKS(LED_PULSE));
        digitalWrite(((led_t*)pvParam)->pin, ! ((led_t*)pvParam)->level);
        vTaskDelay(pdMS_TO_TICKS((((led_t*)pvParam)->mode == LED_05HZ ? 2000 : ((led_t*)pvParam)->mode == LED_1HZ ? 1000 : ((led_t*)pvParam)->mode == LED_2HZ ? 500 : 250) - LED_PULSE));
        break;
      case LED_FADEIN:
        ledcWrite(((led_t*)pvParam)->ledc_channel, ((led_t*)pvParam)->level ? abs(pwm) : 255 - abs(pwm));
        pwm += 5;
        if (pwm > 255)
          pwm = 0;
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP));
        break;
      case LED_FADEOUT:
        ledcWrite(((led_t*)pvParam)->ledc_channel, ((led_t*)pvParam)->level ? abs(pwm) : 255 - abs(pwm));
        pwm -= 5;
        if (pwm < 0)
          pwm = 255;
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP));
        break;
      case LED_BREATH:
        ledcWrite(((led_t*)pvParam)->ledc_channel, ((led_t*)pvParam)->level ? abs(pwm) : 255 - abs(pwm));
        pwm += 5;
        if (pwm > 255)
          pwm = -250;
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP));
        break;
    }
  }
}

LedHandle_t led_init(const ledconfig_t *ledconfig) {
  led_t *result;

  result = (led_t*)pvPortMalloc(sizeof(led_t));
  if (result) {
    result->pin = ledconfig->pin;
    result->level = ledconfig->level;
    result->mode = ledconfig->mode;
    result->ledc_channel = ledconfig->ledc_channel;
    if (xTaskCreate(ledTask, "Led", 1024, result, ledconfig->priority, &result->task) != pdPASS) {
      vPortFree(result);
      result = NULL;
      ESP_LOGE(TAG, "Error creating led task!");
    }
  } else
    ESP_LOGE(TAG, "Error allocating led handle!");
  return result;
}

void led_deinit(LedHandle_t handle) {
  if (handle) {
    vTaskDelete(handle->task);
    if (handle->mode >= LED_FADEIN) {
      ledcDetachPin(handle->pin);
    }
    digitalWrite(handle->pin, ! handle->level);
    vPortFree(handle);
  }
}

ledmode_t led_get(LedHandle_t handle) {
  return handle->mode;
}

void led_set(LedHandle_t handle, ledmode_t mode) {
  if (handle) {
    xTaskNotify(handle->task, mode, eSetValueWithOverwrite);
  }
}
