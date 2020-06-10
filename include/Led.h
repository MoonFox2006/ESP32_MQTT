#ifndef __LED_H
#define __LED_H

#include <inttypes.h>

enum ledmode_t { LED_OFF, LED_ON, LED_TOGGLE, LED_05HZ, LED_1HZ, LED_2HZ, LED_4HZ, LED_FADEIN, LED_FADEOUT, LED_BREATH };

struct __attribute__((__packed__)) ledconfig_t {
  uint8_t pin;
  bool level : 1;
  ledmode_t mode : 7;
  uint8_t ledc_channel : 4;
  uint8_t priority : 4;
};

typedef struct led_t *LedHandle_t;

#define LED_CONFIG(p, l) { .pin = (p), .level = (l), .mode = LED_OFF, .ledc_channel = 15, .priority = 1 }

LedHandle_t led_init(const ledconfig_t *ledconfig);
void led_deinit(LedHandle_t handle);
ledmode_t led_get(LedHandle_t handle);
void led_set(LedHandle_t handle, ledmode_t mode);

#endif
