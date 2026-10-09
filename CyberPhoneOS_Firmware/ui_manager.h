#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>

// Sets up TFT_eSPI, LVGL, touch input and builds the home screen.
// Call once from setup() on Core 1.
void ui_init();

// Pumps lv_timer_handler() and drains the inter-core queues. Call this
// every loop() iteration on Core 1. Non-blocking.
void ui_handler();

#endif // UI_MANAGER_H
