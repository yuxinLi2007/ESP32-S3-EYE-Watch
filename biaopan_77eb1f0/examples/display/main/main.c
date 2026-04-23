/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file
 * @brief BSP Display Clock Example
 * @details Display analog and digital clock on ESP32-S3-EYE (LVGL)
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"

extern void example_clock_ui(lv_obj_t *scr);

void app_main(void)
{
    bsp_display_start();

    ESP_LOGI("example", "Display Clock UI");
    bsp_display_lock(0);
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    example_clock_ui(scr);

    bsp_display_unlock();
    bsp_display_backlight_on();
}
