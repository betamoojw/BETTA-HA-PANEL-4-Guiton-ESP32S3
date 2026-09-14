/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "ui/ui_screen_saver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "app_config.h"
#include "drivers/display_init.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui/fonts/app_text_fonts.h"
#include "ui/ui_i18n.h"

#define TAG_SCREEN_SAVER "screen_saver"

/* Poll interval: the screensaver reacts within one tick of the idle timeout. */
#define SCREEN_SAVER_TICK_MS 1000

/* Grace period after a real touch during which an MQTT "wake" is ignored.
 * This keeps a motion sensor from yanking the menu away the moment the user
 * touches the panel, while a wake outside this window shows the clock at
 * once (so the manual button always has an immediate effect). */
#define WAKE_GRACE_MS 30000

static uint8_t s_brightness = APP_DISPLAY_ACTIVE_BRIGHTNESS_PERCENT;
static bool s_screensaver_enabled = true;
static uint32_t s_screensaver_timeout_sec = APP_DISPLAY_SCREENSAVER_TIMEOUT_SEC;
static bool s_screen_off_enabled = true;
static uint32_t s_screen_off_timeout_sec = APP_DISPLAY_SCREEN_OFF_TIMEOUT_SEC;
static bool s_clock_24h = APP_DISPLAY_CLOCK_24H;
static bool s_show_seconds = APP_DISPLAY_SAVER_SHOW_SECONDS;
static bool s_show_date = APP_DISPLAY_SAVER_SHOW_DATE;
static uint32_t s_clock_color = APP_DISPLAY_SAVER_CLOCK_COLOR;
static uint32_t s_date_color = APP_DISPLAY_SAVER_DATE_COLOR;

static bool s_dimmed = false;
static bool s_overlay_shown = false;
/* Set when an MQTT/API wake command (e.g. a motion sensor) wakes the panel:
 * the screensaver clock shows immediately and stays until the dim timeout
 * dims the backlight. A real touch clears it (full UI). */
static bool s_wake_to_screensaver = false;
static lv_timer_t *s_timer = NULL;
static lv_obj_t *s_overlay = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_bg_image = NULL;
static uint8_t *s_wallpaper_data = NULL;
static lv_image_dsc_t s_wallpaper_dsc = {0};
static bool s_wallpaper_loaded = false;

static void ui_screen_saver_hide_overlay(void)
{
    if (s_overlay != NULL) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    s_overlay_shown = false;
}

static void ui_screen_saver_update_clock(void)
{
    if (s_time_label == NULL || s_date_label == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm info = {0};
    localtime_r(&now, &info);

    int hour = info.tm_hour;
    if (!s_clock_24h) {
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }

    char time_buf[16] = {0};
    if (s_show_seconds) {
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", hour, info.tm_min, info.tm_sec);
    } else {
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, info.tm_min);
    }

    lv_label_set_text(s_time_label, time_buf);

    if (s_show_date) {
        char date_buf[32] = {0};
        snprintf(
            date_buf,
            sizeof(date_buf),
            "%02d.%02d.%04d",
            info.tm_mday,
            info.tm_mon + 1,
            info.tm_year + 1900);
        lv_label_set_text(s_date_label, date_buf);
        lv_obj_clear_flag(s_date_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_date_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_text_color(s_time_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(s_date_color), LV_PART_MAIN);
}

static void ui_screen_saver_free_wallpaper(void)
{
    if (s_wallpaper_data != NULL) {
        heap_caps_free(s_wallpaper_data);
        s_wallpaper_data = NULL;
    }
    memset(&s_wallpaper_dsc, 0, sizeof(s_wallpaper_dsc));
    s_wallpaper_loaded = false;

    if (s_bg_image != NULL) {
        lv_image_set_src(s_bg_image, NULL);
        lv_obj_add_flag(s_bg_image, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_screen_saver_load_wallpaper(void)
{
    ui_screen_saver_free_wallpaper();

    FILE *f = fopen(APP_WALLPAPER_PATH, "rb");
    if (f == NULL) {
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }
    long len = ftell(f);
    if (len != (long)APP_WALLPAPER_BYTES) {
        ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper size %ld != expected %u", len, (unsigned)APP_WALLPAPER_BYTES);
        fclose(f);
        return;
    }
    rewind(f);

#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
    s_wallpaper_data = (uint8_t *)heap_caps_malloc(APP_WALLPAPER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (s_wallpaper_data == NULL) {
        s_wallpaper_data = (uint8_t *)heap_caps_malloc(APP_WALLPAPER_BYTES, MALLOC_CAP_8BIT);
    }
    if (s_wallpaper_data == NULL) {
        ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper buffer alloc failed");
        fclose(f);
        return;
    }

    size_t got = fread(s_wallpaper_data, 1, APP_WALLPAPER_BYTES, f);
    fclose(f);
    if (got != APP_WALLPAPER_BYTES) {
        heap_caps_free(s_wallpaper_data);
        s_wallpaper_data = NULL;
        ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper read short: %u/%u", (unsigned)got, (unsigned)APP_WALLPAPER_BYTES);
        return;
    }

    s_wallpaper_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_wallpaper_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_wallpaper_dsc.header.flags = 0;
    s_wallpaper_dsc.header.w = APP_SCREEN_WIDTH;
    s_wallpaper_dsc.header.h = APP_SCREEN_HEIGHT;
    s_wallpaper_dsc.header.stride = APP_SCREEN_WIDTH * 2;
    s_wallpaper_dsc.data_size = APP_WALLPAPER_BYTES;
    s_wallpaper_dsc.data = s_wallpaper_data;
    s_wallpaper_loaded = true;

    if (s_bg_image != NULL) {
        lv_image_set_src(s_bg_image, &s_wallpaper_dsc);
        lv_obj_clear_flag(s_bg_image, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen_saver_reload_wallpaper(void)
{
    ui_screen_saver_load_wallpaper();
}

static void ui_screen_saver_ensure_overlay(void)
{
    if (s_overlay != NULL) {
        return;
    }

    lv_obj_t *screen = lv_scr_act();
    if (screen == NULL) {
        return;
    }

    s_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);

    s_bg_image = lv_image_create(s_overlay);
    lv_obj_remove_style_all(s_bg_image);
    lv_obj_set_size(s_bg_image, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    lv_obj_set_pos(s_bg_image, 0, 0);
    lv_obj_add_flag(s_bg_image, LV_OBJ_FLAG_HIDDEN);
    if (s_wallpaper_loaded) {
        lv_image_set_src(s_bg_image, &s_wallpaper_dsc);
        lv_obj_clear_flag(s_bg_image, LV_OBJ_FLAG_HIDDEN);
    }

    s_time_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_time_label, APP_FONT_CLOCK_84, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(s_time_label, LV_SIZE_CONTENT);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -22);

    s_date_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_date_label, APP_FONT_TEXT_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(s_date_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_date_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(s_date_label, LV_SIZE_CONTENT);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 42);
}

static void ui_screen_saver_show_overlay(void)
{
    ui_screen_saver_ensure_overlay();
    if (s_overlay == NULL) {
        return;
    }

    ui_screen_saver_update_clock();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
    s_overlay_shown = true;
}

/* Runs from display_note_activity() on any real interaction (touch, boot,
 * API activity). Clears wake-to-screensaver so a touch dismisses the clock. */
static void ui_screen_saver_on_activity(void)
{
    s_wake_to_screensaver = false;
    s_dimmed = false;
}

static void ui_screen_saver_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    int64_t idle_ms = display_ms_since_activity();

    if (s_screen_off_enabled && s_screen_off_timeout_sec > 0 &&
        idle_ms >= (int64_t)s_screen_off_timeout_sec * 1000) {
        /* Instead of turning the backlight off, dim it so the panel always
         * stays active. Keep the clock visible while the screensaver is on. */
        s_dimmed = true;
        s_wake_to_screensaver = false;
        if (s_screensaver_enabled) {
            ui_screen_saver_show_overlay();
        } else {
            ui_screen_saver_hide_overlay();
        }
        (void)display_set_brightness_percent(APP_DISPLAY_DIM_BRIGHTNESS_PERCENT);
        ESP_LOGI(TAG_SCREEN_SAVER, "dim (idle=%d ms)", (int)idle_ms);
        return;
    }

    if (!s_screensaver_enabled) {
        /* Screensaver fully disabled: hide the clock and never auto-show it. */
        ui_screen_saver_hide_overlay();
        s_wake_to_screensaver = false;
        s_dimmed = false;
        return;
    }

    if (s_wake_to_screensaver) {
        /* Motion-sensor wake: keep the clock overlay visible until the
         * dim timeout above dims the backlight. */
        ui_screen_saver_show_overlay();
        ESP_LOGI(TAG_SCREEN_SAVER, "wake-to-screensaver shown (idle=%d ms)", (int)idle_ms);
        return;
    }

    if (s_screensaver_timeout_sec > 0 &&
        idle_ms >= (int64_t)s_screensaver_timeout_sec * 1000) {
        ui_screen_saver_show_overlay();
    } else {
        ui_screen_saver_hide_overlay();
    }
}

void ui_screen_saver_apply_settings(const runtime_settings_t *settings)
{
    if (settings == NULL) {
        return;
    }

    bool brightness_changed = (settings->display_brightness != s_brightness);
    bool off_just_disabled = (s_screen_off_enabled && !settings->display_screen_off_enabled);

    s_brightness = settings->display_brightness;
    s_screensaver_enabled = settings->display_screensaver_enabled;
    s_screensaver_timeout_sec = settings->display_screensaver_timeout_sec;
    s_screen_off_enabled = settings->display_screen_off_enabled;
    s_screen_off_timeout_sec = settings->display_screen_off_timeout_sec;
    s_clock_24h = settings->display_clock_24h;
    s_show_seconds = settings->display_saver_show_seconds;
    s_show_date = settings->display_saver_show_date;
    s_clock_color = settings->display_saver_clock_color;
    s_date_color = settings->display_saver_date_color;

    /* Keep the wake/touch restore brightness in sync with the user setting. */
    display_set_active_brightness_percent((int)s_brightness);

    if (s_dimmed && (brightness_changed || off_just_disabled)) {
        /* Wake the panel so the user sees the effect of the change. */
        s_dimmed = false;
        display_note_activity();
    } else if (!s_dimmed) {
        (void)display_set_brightness_percent((int)s_brightness);
    }

    if (!s_screensaver_enabled) {
        /* Clock disabled: stop forcing it and let the timer hide the overlay. */
        s_wake_to_screensaver = false;
    }
}

void ui_screen_saver_wake(void)
{
    if (s_dimmed) {
        /* Panel is dimmed: brighten it back to full and show the clock,
         * restarting the idle timer so the dim timeout counts from now. */
        s_dimmed = false;
        display_note_activity(); /* restores brightness + clears flag via cb */
        s_wake_to_screensaver = s_screensaver_enabled;
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: dimmed -> %s (brightness=%d%%)",
                 s_wake_to_screensaver ? "screensaver" : "menu", (int)s_brightness);
        return;
    }

    /* Panel already on. */
    if (!s_screensaver_enabled) {
        /* No clock configured: nothing to force on top of the UI. */
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: ignored (screensaver disabled)");
        return;
    }

    int64_t idle_ms = display_ms_since_activity();

    /* Ignore only during the short grace period right after a real touch so
     * a motion sensor cannot yank the menu away. Outside that window every
     * wake (manual button or PIR) shows the clock immediately. */
    if (idle_ms < WAKE_GRACE_MS) {
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: ignored (just touched, idle=%d ms)", (int)idle_ms);
        return;
    }

    if (s_wake_to_screensaver) {
        /* Clock already showing: keep it. Do NOT reset the idle timer so
         * repeated PIR motion cannot postpone the auto screen-off. */
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: idle refresh (idle=%d ms)", (int)idle_ms);
    } else {
        s_wake_to_screensaver = true;
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: show screensaver now (idle=%d ms)", (int)idle_ms);
    }
}

void ui_screen_saver_init(void)
{
    runtime_settings_t settings;
    runtime_settings_set_defaults(&settings);
    if (runtime_settings_load(&settings) != ESP_OK) {
        runtime_settings_set_defaults(&settings);
    }

    s_brightness = settings.display_brightness;
    s_screensaver_enabled = settings.display_screensaver_enabled;
    s_screensaver_timeout_sec = settings.display_screensaver_timeout_sec;
    s_screen_off_enabled = settings.display_screen_off_enabled;
    s_screen_off_timeout_sec = settings.display_screen_off_timeout_sec;
    s_clock_24h = settings.display_clock_24h;
    s_show_seconds = settings.display_saver_show_seconds;
    s_show_date = settings.display_saver_show_date;
    s_clock_color = settings.display_saver_clock_color;
    s_date_color = settings.display_saver_date_color;

    /* Sync wake/touch restore brightness and register the activity callback
     * so a real touch dismisses a wake-to-screensaver overlay. */
    display_set_active_brightness_percent((int)s_brightness);
    display_set_activity_callback(ui_screen_saver_on_activity);

    ui_screen_saver_load_wallpaper();

    if (s_timer == NULL) {
        s_timer = lv_timer_create(ui_screen_saver_timer_cb, SCREEN_SAVER_TICK_MS, NULL);
    }
}
