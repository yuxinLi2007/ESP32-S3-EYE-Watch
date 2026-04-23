/*
 * @file clock_ui.c
 * @brief Analog Clock UI for ESP32-S3-EYE
 * @details Pointer-style analog clock display using LVGL
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "clock_ui";

// Screen dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define CENTER_X      120
#define CENTER_Y      120
#define CLOCK_RADIUS  110

// UI elements
static lv_obj_t *hour_hand = NULL;
static lv_obj_t *minute_hand = NULL;
static lv_obj_t *second_hand = NULL;
static lv_obj_t *center_dot = NULL;
static lv_timer_t *update_timer = NULL;

/**
 * @brief Create white circular clock background with black border
 */
static void create_clock_background(lv_obj_t *parent)
{
    // Create main clock circle (white background)
    lv_obj_t *clock_bg = lv_obj_create(parent);
    lv_obj_set_size(clock_bg, CLOCK_RADIUS * 2, CLOCK_RADIUS * 2);
    lv_obj_align(clock_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(clock_bg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(clock_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(clock_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(clock_bg, 3, 0);
    lv_obj_set_style_radius(clock_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(clock_bg, 0, 0);
}

/**
 * @brief Create colored hour numbers (1-12)
 */
static void create_hour_numbers(lv_obj_t *parent)
{
    // Define RGB color components for each hour number
    static const uint8_t color_r[12] = {255, 255, 255,   0,   0,   0, 128, 255, 255,   0,   0, 128};
    static const uint8_t color_g[12] = {  0, 127, 255, 255, 255,   0,   0,   0, 102,   0, 128, 128};
    static const uint8_t color_b[12] = {  0,   0,   0,   0, 255, 255, 255, 255,   0, 128,   0, 128};
    
    // Hour numbers (1-12)
    const char *numbers[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"};
    
    // Number radius: position just inside the hour tick marks
    // Hour ticks at CLOCK_RADIUS - 15 = 95, so use 82 for numbers
    const int number_radius = 82;
    
    for (int i = 0; i < 12; i++) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, numbers[i]);
        lv_obj_set_style_text_color(label, lv_color_make(color_r[i], color_g[i], color_b[i]), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        
        // Calculate position using polar coordinates
        // Angle: 0 degrees at 12 o'clock, increasing clockwise
        float angle = i * 30 * M_PI / 180.0;
        
        // Calculate offset from center
        int offset_x = (int)(number_radius * sinf(angle));
        int offset_y = -(int)(number_radius * cosf(angle));
        
        // Get label dimensions for centering
        int label_w = lv_obj_get_width(label);
        int label_h = lv_obj_get_height(label);
        
        // Set absolute position (center the label at the calculated position)
        lv_obj_set_pos(label, CENTER_X + offset_x - label_w / 2, 
                              CENTER_Y + offset_y - label_h / 2);
    }
}

/**
 * @brief Draw hour and minute tick marks
 */
static void create_tick_marks(lv_obj_t *parent)
{
    // Hour tick marks (12 marks)
    for (int i = 0; i < 12; i++) {
        lv_obj_t *tick = lv_obj_create(parent);
        float angle = i * 30; // 30 degrees per hour
        float rad = angle * M_PI / 180.0;
        
        // Position at the edge of clock
        int x = CENTER_X + (CLOCK_RADIUS - 15) * sin(rad);
        int y = CENTER_Y - (CLOCK_RADIUS - 15) * cos(rad);
        
        lv_obj_set_size(tick, 4, 10);
        lv_obj_set_pos(tick, x - 2, y - 5);
        lv_obj_set_style_bg_color(tick, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tick, 0, 0);
        lv_obj_set_style_shadow_width(tick, 0, 0);
    }
    
    // Minute tick marks (48 marks, between hour marks)
    for (int i = 0; i < 60; i++) {
        if (i % 5 != 0) { // Skip positions where hour marks are
            lv_obj_t *tick = lv_obj_create(parent);
            float angle = i * 6; // 6 degrees per minute
            float rad = angle * M_PI / 180.0;
            
            int x = CENTER_X + (CLOCK_RADIUS - 8) * sin(rad);
            int y = CENTER_Y - (CLOCK_RADIUS - 8) * cos(rad);
            
            lv_obj_set_size(tick, 2, 5);
            lv_obj_set_pos(tick, x - 1, y - 2);
            lv_obj_set_style_bg_color(tick, lv_color_hex(0x888888), 0);
            lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(tick, 0, 0);
            lv_obj_set_style_shadow_width(tick, 0, 0);
        }
    }
}

/**
 * @brief Create hour hand (shorter, thicker)
 */
static void create_hour_hand(lv_obj_t *parent)
{
    hour_hand = lv_obj_create(parent);
    lv_obj_set_size(hour_hand, 4, 50); // width x length
    lv_obj_set_style_bg_color(hour_hand, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hour_hand, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hour_hand, 2, 0);
    lv_obj_set_style_shadow_width(hour_hand, 0, 0);
    
    // Position at center, with pivot at bottom of hand
    lv_obj_align(hour_hand, LV_ALIGN_CENTER, 0, -25);
    lv_obj_set_style_transform_pivot_x(hour_hand, 2, 0); // Center of width
    lv_obj_set_style_transform_pivot_y(hour_hand, 47, 0); // Near bottom (rotation point)
}

/**
 * @brief Create minute hand (longer, medium thickness)
 */
static void create_minute_hand(lv_obj_t *parent)
{
    minute_hand = lv_obj_create(parent);
    lv_obj_set_size(minute_hand, 3, 70); // width x length
    lv_obj_set_style_bg_color(minute_hand, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(minute_hand, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(minute_hand, 2, 0);
    lv_obj_set_style_shadow_width(minute_hand, 0, 0);
    
    // Position at center
    lv_obj_align(minute_hand, LV_ALIGN_CENTER, 0, -35);
    lv_obj_set_style_transform_pivot_x(minute_hand, 1, 0); // Center of width
    lv_obj_set_style_transform_pivot_y(minute_hand, 67, 0); // Near bottom (rotation point)
}

/**
 * @brief Create second hand (longest, thinnest, red)
 */
static void create_second_hand(lv_obj_t *parent)
{
    second_hand = lv_obj_create(parent);
    lv_obj_set_size(second_hand, 2, 85); // width x length
    lv_obj_set_style_bg_color(second_hand, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(second_hand, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(second_hand, 1, 0);
    lv_obj_set_style_shadow_width(second_hand, 0, 0);
    
    // Position at center
    lv_obj_align(second_hand, LV_ALIGN_CENTER, 0, -42);
    lv_obj_set_style_transform_pivot_x(second_hand, 1, 0); // Center of width
    lv_obj_set_style_transform_pivot_y(second_hand, 82, 0); // Near bottom (rotation point)
}

/**
 * @brief Create center dot
 */
static void create_center_dot(lv_obj_t *parent)
{
    center_dot = lv_obj_create(parent);
    lv_obj_set_size(center_dot, 10, 10);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(center_dot, 0, 0);
}

/**
 * @brief Update clock hands position based on current time
 */
static void update_clock(lv_timer_t *timer)
{
    (void)timer;
    
    // Get current time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);
    
    // Calculate angles
    int seconds = timeinfo.tm_sec;
    int minutes = timeinfo.tm_min;
    int hours = timeinfo.tm_hour;
    
    // Second hand angle (6 degrees per second)
    int second_angle = seconds * 6;
    
    // Minute hand angle (6 degrees per minute + adjustment for seconds)
    int minute_angle = minutes * 6 + seconds * 6 / 60;
    
    // Hour hand angle (30 degrees per hour + adjustment for minutes)
    int hour_angle = (hours % 12) * 30 + minutes * 30 / 60;
    
    // Update hand rotations
    if (second_hand != NULL) {
        lv_obj_set_style_transform_angle(second_hand, second_angle * 10, 0);
    }
    if (minute_hand != NULL) {
        lv_obj_set_style_transform_angle(minute_hand, minute_angle * 10, 0);
    }
    if (hour_hand != NULL) {
        lv_obj_set_style_transform_angle(hour_hand, hour_angle * 10, 0);
    }
}

/**
 * @brief Create analog clock UI
 */
void example_clock_ui(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Creating analog clock UI...");
    
    // Set black background for screen
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    // Create clock components
    create_clock_background(scr);
    create_tick_marks(scr);
    create_hour_numbers(scr);
    create_hour_hand(scr);
    create_minute_hand(scr);
    create_second_hand(scr);
    create_center_dot(scr);
    
    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    
    // Create update timer (1 second interval)
    update_timer = lv_timer_create(update_clock, 1000, NULL);
    lv_timer_set_repeat_count(update_timer, -1);
    
    // Initial update
    update_clock(NULL);
    
    ESP_LOGI(TAG, "Analog clock UI created");
}

/**
 * @brief Delete clock UI
 */
void example_clock_ui_delete(void)
{
    if (update_timer != NULL) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    hour_hand = NULL;
    minute_hand = NULL;
    second_hand = NULL;
    center_dot = NULL;
    ESP_LOGI(TAG, "Clock UI deleted");
}