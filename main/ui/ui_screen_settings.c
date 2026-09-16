/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "ui/ui_screen_settings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lvgl.h"

#include "app_config.h"
#include "drivers/display_init.h"
#include "settings/runtime_settings.h"
#include "ui/fonts/app_text_fonts.h"
#include "ui/ui_i18n.h"
#include "ui/ui_screen_saver.h"
#include "ui/ui_slider_touch.h"

#define SETTINGS_BG lv_color_hex(0x1B1E23)
#define SETTINGS_ROW_BG lv_color_hex(0x24282F)
#define SETTINGS_ACCENT lv_color_hex(0x2F6FED)
#define SETTINGS_TEXT lv_color_hex(0xE6E8EC)
#define SETTINGS_TEXT_DIM lv_color_hex(0x8A929C)

#if LV_FONT_MONTSERRAT_24
#define SETTINGS_ICON_FONT (&lv_font_montserrat_24)
#elif LV_FONT_MONTSERRAT_20
#define SETTINGS_ICON_FONT (&lv_font_montserrat_20)
#else
#define SETTINGS_ICON_FONT LV_FONT_DEFAULT
#endif

static void ui_screen_settings_close_cb(lv_event_t *e);
static lv_obj_t *ui_screen_settings_make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                                               lv_color_t color, int16_t x, int16_t y);

static const uint32_t SCREENSAVER_TIMEOUTS[] = {15, 30, 60, 120, 300};
static const uint32_t SCREEN_OFF_TIMEOUTS[] = {30, 60, 120, 300, 600};

/* Rows are stacked from the boxes LVGL actually created instead of hardcoded y
 * offsets, so a new row can never land on top of the previous one whatever the
 * font metrics are. Vertical scrolling stays enabled as a safety net, so a
 * shorter display scrolls instead of clipping the last row. */
#define SETTINGS_ROW_FIRST_Y 70
#define SETTINGS_LABEL_TO_SLIDER_GAP 0
#define SETTINGS_AFTER_SLIDER_GAP 12 /* clear of the 6 px knob overhang */
#define SETTINGS_AFTER_TOGGLE_GAP 4
#define SETTINGS_AFTER_CHIP_GAP 2

static runtime_settings_t s_cfg;
static lv_obj_t *s_page = NULL;
static lv_obj_t *s_brightness_value = NULL;
static lv_obj_t *s_saver_brightness_value = NULL;
static lv_obj_t *s_saver_timeout_label = NULL;
static lv_obj_t *s_off_timeout_label = NULL;
static int16_t s_row_y = SETTINGS_ROW_FIRST_Y;

static void ui_screen_settings_format_timeout(uint32_t seconds, char *buf, size_t len)
{
    if (seconds % 60 == 0) {
        snprintf(buf, len, "%u min", (unsigned)(seconds / 60));
    } else {
        snprintf(buf, len, "%u s", (unsigned)seconds);
    }
}

static void ui_screen_settings_style_chip(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, SETTINGS_ROW_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_top(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, SETTINGS_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(obj, APP_FONT_TEXT_22, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, SETTINGS_ACCENT, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(obj, 12);
}

static void ui_screen_settings_update_timeout_labels(void)
{
    char buf[24] = {0};

    if (s_saver_timeout_label != NULL) {
        ui_screen_settings_format_timeout(s_cfg.display_screensaver_timeout_sec, buf, sizeof(buf));
        lv_label_set_text(s_saver_timeout_label, buf);
    }

    if (s_off_timeout_label != NULL) {
        ui_screen_settings_format_timeout(s_cfg.display_screen_off_timeout_sec, buf, sizeof(buf));
        lv_label_set_text(s_off_timeout_label, buf);
    }
}

static void ui_screen_settings_apply(void)
{
    /* ui_screen_saver_* owns the backlight: it re-applies the menu level while
     * the panel is awake and the dimmed level while the clock is on screen, so
     * the settings page must not force a value on top of that. */
    ui_screen_saver_apply_settings(&s_cfg);
}

/* Advance the row cursor below the taller of the two widgets of the row. */
static void ui_screen_settings_row_advance(lv_obj_t *first, lv_obj_t *second, int16_t gap)
{
    if (s_page == NULL) {
        return;
    }

    lv_obj_update_layout(s_page);

    int16_t bottom = 0;
    if (first != NULL) {
        bottom = (int16_t)lv_obj_get_y2(first);
    }
    if (second != NULL) {
        const int16_t other = (int16_t)lv_obj_get_y2(second);
        if (other > bottom) {
            bottom = other;
        }
    }

    s_row_y = (int16_t)(bottom + 1 + gap);
}

static uint32_t ui_screen_settings_next_timeout(const uint32_t *values, size_t count, uint32_t current)
{
    for (size_t i = 0; i < count; i++) {
        if (values[i] == current) {
            return values[(i + 1) % count];
        }
    }
    return values[0];
}

static void ui_screen_settings_brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    uint8_t value = (uint8_t)lv_slider_get_value(slider);

    s_cfg.display_brightness = value;
    char buf[8] = {0};
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)value);
    lv_label_set_text(s_brightness_value, buf);
    ui_screen_settings_apply();
}

static void ui_screen_settings_saver_brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    uint8_t value = (uint8_t)lv_slider_get_value(slider);

    s_cfg.display_saver_brightness = value;
    char buf[8] = {0};
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)value);
    if (s_saver_brightness_value != NULL) {
        lv_label_set_text(s_saver_brightness_value, buf);
    }

    ui_screen_settings_apply();

    /* Dim the panel live while dragging so the level can be judged without
     * waiting for the screensaver. Released restores the menu level. */
    (void)display_set_brightness_percent((int)s_cfg.display_saver_brightness);
}

static void ui_screen_settings_saver_brightness_released_cb(lv_event_t *e)
{
    (void)e;
    ui_screen_settings_apply();
}

static void ui_screen_settings_saver_toggle_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_cfg.display_screensaver_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ui_screen_settings_apply();
}

static void ui_screen_settings_saver_timeout_cb(lv_event_t *e)
{
    (void)e;
    s_cfg.display_screensaver_timeout_sec = ui_screen_settings_next_timeout(
        SCREENSAVER_TIMEOUTS,
        sizeof(SCREENSAVER_TIMEOUTS) / sizeof(SCREENSAVER_TIMEOUTS[0]),
        s_cfg.display_screensaver_timeout_sec);
    ui_screen_settings_update_timeout_labels();
    ui_screen_settings_apply();
}

static void ui_screen_settings_off_toggle_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_cfg.display_screen_off_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ui_screen_settings_apply();
}

static void ui_screen_settings_off_timeout_cb(lv_event_t *e)
{
    (void)e;
    s_cfg.display_screen_off_timeout_sec = ui_screen_settings_next_timeout(
        SCREEN_OFF_TIMEOUTS,
        sizeof(SCREEN_OFF_TIMEOUTS) / sizeof(SCREEN_OFF_TIMEOUTS[0]),
        s_cfg.display_screen_off_timeout_sec);
    ui_screen_settings_update_timeout_labels();
    ui_screen_settings_apply();
}

static void ui_screen_settings_clock_24h_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_cfg.display_clock_24h = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ui_screen_settings_apply();
}

static void ui_screen_settings_seconds_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_cfg.display_saver_show_seconds = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ui_screen_settings_apply();
}

static void ui_screen_settings_date_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    s_cfg.display_saver_show_date = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ui_screen_settings_apply();
}

static lv_obj_t *ui_screen_settings_make_toggle(lv_obj_t *parent, const char *text, bool checked,
                                                int16_t y, lv_event_cb_t cb)
{
    ui_screen_settings_make_label(parent, text, APP_FONT_TEXT_24, SETTINGS_TEXT, 24, y);

    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -24, y);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

static lv_obj_t *ui_screen_settings_make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                                               lv_color_t color, int16_t x, int16_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void ui_screen_settings_close_cb(lv_event_t *e)
{
    (void)e;

    if (runtime_settings_save(&s_cfg) != ESP_OK) {
        /* Keep going: the panel keeps the in-memory values either way. */
    }

    if (s_page != NULL) {
        lv_obj_del(s_page);
        s_page = NULL;
    }
}

void ui_screen_settings_handle_screen_clean(void)
{
    /* The settings page lives on the active screen: lv_obj_clean() just deleted
     * it, so only our handles have to go. */
    s_page = NULL;
    s_brightness_value = NULL;
    s_saver_brightness_value = NULL;
    s_saver_timeout_label = NULL;
    s_off_timeout_label = NULL;
}

void ui_screen_settings_open(void)
{
    runtime_settings_set_defaults(&s_cfg);
    if (runtime_settings_load(&s_cfg) != ESP_OK) {
        runtime_settings_set_defaults(&s_cfg);
    }

    lv_obj_t *screen = lv_scr_act();
    if (screen == NULL) {
        return;
    }

    if (s_page != NULL) {
        lv_obj_del(s_page);
        s_page = NULL;
    }

    s_page = lv_obj_create(screen);
    lv_obj_remove_style_all(s_page);
    lv_obj_set_size(s_page, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    lv_obj_set_pos(s_page, 0, 0);
    lv_obj_set_style_bg_color(s_page, SETTINGS_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_page, 0, LV_PART_MAIN);
    /* Keep vertical scrolling (rows are measured, the display may be short)
     * but drop the elastic bounce so the page cannot wobble. */
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_page, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_style_pad_bottom(s_page, 10, LV_PART_MAIN);

    const char *title_text = ui_i18n_get("screen.title", "Ustawienia ekranu");
    (void)ui_screen_settings_make_label(s_page, title_text, APP_FONT_DISPLAY_28, SETTINGS_TEXT, 24, 18);

    /* Close button */
    lv_obj_t *close_btn = lv_label_create(s_page);
    lv_label_set_text(close_btn, LV_SYMBOL_CLOSE);
    ui_screen_settings_style_chip(close_btn);
    lv_obj_set_style_text_font(close_btn, SETTINGS_ICON_FONT, LV_PART_MAIN);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_add_event_cb(close_btn, ui_screen_settings_close_cb, LV_EVENT_CLICKED, NULL);

    s_row_y = SETTINGS_ROW_FIRST_Y;

    /* Brightness row */
    const char *brightness_text = ui_i18n_get("screen.brightness", "Jasnosc");
    lv_obj_t *brightness_label = ui_screen_settings_make_label(s_page, brightness_text, APP_FONT_TEXT_24,
                                                               SETTINGS_TEXT, 24, s_row_y);
    s_brightness_value = ui_screen_settings_make_label(s_page, "100%", APP_FONT_TEXT_24,
                                                       SETTINGS_TEXT_DIM, 0, s_row_y);
    lv_obj_set_width(s_brightness_value, LV_SIZE_CONTENT);
    lv_obj_align(s_brightness_value, LV_ALIGN_TOP_RIGHT, -24, s_row_y);
    ui_screen_settings_row_advance(brightness_label, s_brightness_value, SETTINGS_LABEL_TO_SLIDER_GAP);

    lv_obj_t *brightness_slider = lv_slider_create(s_page);
    lv_obj_set_width(brightness_slider, APP_SCREEN_WIDTH - 48);
    lv_obj_set_pos(brightness_slider, 24, s_row_y);
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_slider_set_value(brightness_slider, s_cfg.display_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, ui_screen_settings_brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);
    ui_slider_touch_enable(brightness_slider);
    ui_screen_settings_row_advance(brightness_slider, NULL, SETTINGS_AFTER_SLIDER_GAP);

    /* Screensaver row */
    const char *saver_text = ui_i18n_get("screen.screensaver", "Wygaszacz");
    lv_obj_t *saver_label = ui_screen_settings_make_label(s_page, saver_text, APP_FONT_TEXT_24,
                                                          SETTINGS_TEXT, 24, s_row_y);

    lv_obj_t *saver_switch = lv_switch_create(s_page);
    lv_obj_align(saver_switch, LV_ALIGN_TOP_RIGHT, -24, s_row_y);
    if (s_cfg.display_screensaver_enabled) {
        lv_obj_add_state(saver_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(saver_switch, ui_screen_settings_saver_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
    ui_screen_settings_row_advance(saver_label, saver_switch, SETTINGS_AFTER_TOGGLE_GAP);

    s_saver_timeout_label = lv_label_create(s_page);
    ui_screen_settings_style_chip(s_saver_timeout_label);
    lv_obj_set_pos(s_saver_timeout_label, 24, s_row_y);
    lv_obj_add_event_cb(s_saver_timeout_label, ui_screen_settings_saver_timeout_cb, LV_EVENT_CLICKED, NULL);
    ui_screen_settings_row_advance(s_saver_timeout_label, NULL, SETTINGS_AFTER_CHIP_GAP);

    /* Screensaver brightness row: how dark the clock dims the panel. It sits
     * below the screensaver timeout so the clock options stay together. */
    const char *saver_brightness_text = ui_i18n_get("screen.saver_brightness", "Jasnosc zegara");
    lv_obj_t *saver_brightness_label = ui_screen_settings_make_label(s_page, saver_brightness_text,
                                                                     APP_FONT_TEXT_24, SETTINGS_TEXT, 24, s_row_y);
    s_saver_brightness_value = ui_screen_settings_make_label(s_page, "20%", APP_FONT_TEXT_24,
                                                             SETTINGS_TEXT_DIM, 0, s_row_y);
    lv_obj_set_width(s_saver_brightness_value, LV_SIZE_CONTENT);
    lv_obj_align(s_saver_brightness_value, LV_ALIGN_TOP_RIGHT, -24, s_row_y);
    ui_screen_settings_row_advance(saver_brightness_label, s_saver_brightness_value,
                                   SETTINGS_LABEL_TO_SLIDER_GAP);

    lv_obj_t *saver_brightness_slider = lv_slider_create(s_page);
    lv_obj_set_width(saver_brightness_slider, APP_SCREEN_WIDTH - 48);
    lv_obj_set_pos(saver_brightness_slider, 24, s_row_y);
    lv_slider_set_range(saver_brightness_slider, 1, 100);
    lv_slider_set_value(saver_brightness_slider, s_cfg.display_saver_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(saver_brightness_slider, ui_screen_settings_saver_brightness_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(saver_brightness_slider, ui_screen_settings_saver_brightness_released_cb,
                        LV_EVENT_RELEASED, NULL);
    ui_slider_touch_enable(saver_brightness_slider);
    ui_screen_settings_row_advance(saver_brightness_slider, NULL, SETTINGS_AFTER_SLIDER_GAP);

    /* Screen-off row */
    const char *off_text = ui_i18n_get("screen.screen_off", "Wylacz ekran");
    lv_obj_t *off_label = ui_screen_settings_make_label(s_page, off_text, APP_FONT_TEXT_24,
                                                        SETTINGS_TEXT, 24, s_row_y);

    lv_obj_t *off_switch = lv_switch_create(s_page);
    lv_obj_align(off_switch, LV_ALIGN_TOP_RIGHT, -24, s_row_y);
    if (s_cfg.display_screen_off_enabled) {
        lv_obj_add_state(off_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(off_switch, ui_screen_settings_off_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
    ui_screen_settings_row_advance(off_label, off_switch, SETTINGS_AFTER_TOGGLE_GAP);

    s_off_timeout_label = lv_label_create(s_page);
    ui_screen_settings_style_chip(s_off_timeout_label);
    lv_obj_set_pos(s_off_timeout_label, 24, s_row_y);
    lv_obj_add_event_cb(s_off_timeout_label, ui_screen_settings_off_timeout_cb, LV_EVENT_CLICKED, NULL);
    ui_screen_settings_row_advance(s_off_timeout_label, NULL, SETTINGS_AFTER_CHIP_GAP);

    /* Clock / content rows */
    const char *clock_24h_text = ui_i18n_get("screen.clock_24h", "Format 24h");
    lv_obj_t *clock_24h_switch = ui_screen_settings_make_toggle(s_page, clock_24h_text, s_cfg.display_clock_24h,
                                                               s_row_y, ui_screen_settings_clock_24h_cb);
    ui_screen_settings_row_advance(clock_24h_switch, NULL, SETTINGS_AFTER_TOGGLE_GAP);

    const char *seconds_text = ui_i18n_get("screen.show_seconds", "Sekundy");
    lv_obj_t *seconds_switch = ui_screen_settings_make_toggle(s_page, seconds_text, s_cfg.display_saver_show_seconds,
                                                             s_row_y, ui_screen_settings_seconds_cb);
    ui_screen_settings_row_advance(seconds_switch, NULL, SETTINGS_AFTER_TOGGLE_GAP);

    const char *date_text = ui_i18n_get("screen.show_date", "Data");
    lv_obj_t *date_switch = ui_screen_settings_make_toggle(s_page, date_text, s_cfg.display_saver_show_date,
                                                          s_row_y, ui_screen_settings_date_cb);
    ui_screen_settings_row_advance(date_switch, NULL, SETTINGS_AFTER_TOGGLE_GAP);

    ui_screen_settings_update_timeout_labels();
    char brightness_buf[8] = {0};
    snprintf(brightness_buf, sizeof(brightness_buf), "%u%%", (unsigned)s_cfg.display_brightness);
    lv_label_set_text(s_brightness_value, brightness_buf);

    char saver_brightness_buf[8] = {0};
    snprintf(saver_brightness_buf, sizeof(saver_brightness_buf), "%u%%",
             (unsigned)s_cfg.display_saver_brightness);
    lv_label_set_text(s_saver_brightness_value, saver_brightness_buf);

    lv_obj_move_foreground(s_page);
}
