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
#include "esp_timer.h"
#include "ui/fonts/app_text_fonts.h"
#include "ui/ui_i18n.h"
#include "ui/ui_page_style.h"

#define TAG_SCREEN_SAVER "screen_saver"

/* Poll interval: the screensaver reacts within one tick of the idle timeout. */
#define SCREEN_SAVER_TICK_MS 1000

/* Timeout for taking the display lock while swapping the wallpaper buffer. */
#define WALLPAPER_RELOAD_LOCK_MS 1000

/* Copying a whole frame takes a moment; the lock is held for that long. */
#define WALLPAPER_SYNC_LOCK_MS 3000

/* Grace period after a real touch during which an MQTT "wake" is ignored.
 * This keeps a motion sensor from yanking the menu away the moment the user
 * touches the panel, while a wake outside this window shows the clock at
 * once (so the manual button always has an immediate effect). */
#define WAKE_GRACE_MS 30000

static uint8_t s_brightness = APP_DISPLAY_ACTIVE_BRIGHTNESS_PERCENT;
static uint8_t s_saver_brightness = APP_DISPLAY_SAVER_BRIGHTNESS_PERCENT;
static bool s_screensaver_enabled = true;
static uint32_t s_screensaver_timeout_sec = APP_DISPLAY_SCREENSAVER_TIMEOUT_SEC;
static bool s_screen_off_enabled = APP_DISPLAY_SCREEN_OFF_ENABLED ? true : false;
static uint32_t s_screen_off_timeout_sec = APP_DISPLAY_SCREEN_OFF_TIMEOUT_SEC;
static bool s_clock_24h = APP_DISPLAY_CLOCK_24H;
static bool s_show_seconds = APP_DISPLAY_SAVER_SHOW_SECONDS;
static bool s_show_date = APP_DISPLAY_SAVER_SHOW_DATE;
static uint32_t s_clock_color = APP_DISPLAY_SAVER_CLOCK_COLOR;
static uint32_t s_date_color = APP_DISPLAY_SAVER_DATE_COLOR;
static bool s_night_enabled = false;
static uint16_t s_night_start_min = APP_DISPLAY_NIGHT_START_MIN;
static uint16_t s_night_end_min = APP_DISPLAY_NIGHT_END_MIN;
static uint8_t s_night_brightness = APP_DISPLAY_NIGHT_BRIGHTNESS_PERCENT;
static uint16_t s_night_wake_sec = APP_DISPLAY_NIGHT_WAKE_SEC;

static bool s_screen_off = false;
static bool s_overlay_shown = false;
/* Night schedule state: s_night_active is true inside the configured window,
 * s_night_wake_until_ms holds the deadline of the temporary wake granted by a
 * touch (0 = none). */
static bool s_night_active = false;
static int64_t s_night_wake_until_ms = 0;
/* Set when an MQTT/API wake command (e.g. a motion sensor) wakes the panel:
 * the screensaver clock shows immediately and stays until the screen-off
 * timeout turns the backlight off. A real touch clears it (full UI). */
static bool s_wake_to_screensaver = false;
static lv_timer_t *s_timer = NULL;
static lv_obj_t *s_overlay = NULL;
static lv_obj_t *s_time_label = NULL;
/* AM/PM badge: the same two letters drawn on top of each other with a 1 px
 * offset. LVGL only thickens outlines for vector fonts, so stacking copies is
 * how a bitmap font gets a bold look here. Index 0 is the base copy. */
#define AMPM_COPY_COUNT 5
static lv_obj_t *s_ampm_copies[AMPM_COPY_COUNT] = {0};
static const int8_t s_ampm_copy_ofs[AMPM_COPY_COUNT][2] = {
    {0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1},
};
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_bg_image = NULL;
static uint8_t *s_wallpaper_data = NULL;
static lv_image_dsc_t s_wallpaper_dsc = {0};
static bool s_wallpaper_loaded = false;

/* Flip clock: one card per digit (HHMM, leading zero kept) with the top half of
 * the digit drawn on its own clipped layer so it can be folded down over the
 * centre seam while the time changes. */
#define FLIP_DIGIT_COUNT 4
#define FLIP_DIGIT_UNSET 0xFF

typedef struct {
    lv_obj_t *top_clip;     /* folds down over the seam */
    lv_obj_t *top_label;    /* digit drawn inside top_clip  */
    lv_obj_t *bottom_label; /* static lower half of the digit */
    uint8_t shown;          /* digit on screen, FLIP_DIGIT_UNSET = nothing drawn yet */
} flip_digit_t;

static uint8_t s_clock_style = APP_DISPLAY_SAVER_CLOCK_STYLE_DEFAULT;
/* Set from the HTTP task, consumed by the LVGL task: the overlay must not be
 * destroyed by the request handler. */
static bool s_style_pending = false;
static uint32_t s_flip_color_applied = 0xFFFFFFFF;
static lv_obj_t *s_flip_row = NULL;
static int32_t s_flip_row_w = 0;
/* Last AM/PM state drawn: 0 = AM, 1 = PM, 2 = not drawn (24 h or unknown).
 * Keeps the badge from being re-laid out every second. */
static uint8_t s_ampm_state = 2;
static flip_digit_t s_flip_digits[FLIP_DIGIT_COUNT];

static void ui_screen_saver_flip_scale_cb(void *var, int32_t value)
{
    lv_obj_t *flap = (lv_obj_t *)var;
    lv_obj_set_style_transform_scale_y(flap, value, LV_PART_MAIN);
    /* Darken the flap a little while it folds away: without it the fold reads
     * as a shrink instead of a card turning over. */
    const uint32_t folded = (uint32_t)(LV_SCALE_NONE - value);
    lv_obj_set_style_opa(flap, (lv_opa_t)(255U - (folded * 90U) / LV_SCALE_NONE), LV_PART_MAIN);
}

/* Drops every flip handle. Must run before the objects it points at are
 * deleted: a running fold animation would otherwise write into freed memory. */
static void ui_screen_saver_flip_reset(void)
{
    for (uint32_t i = 0; i < FLIP_DIGIT_COUNT; i++) {
        if (s_flip_digits[i].top_clip != NULL) {
            lv_anim_delete(s_flip_digits[i].top_clip, ui_screen_saver_flip_scale_cb);
        }
        memset(&s_flip_digits[i], 0, sizeof(s_flip_digits[i]));
        s_flip_digits[i].shown = FLIP_DIGIT_UNSET;
    }
    s_flip_row = NULL;
    s_flip_row_w = 0;
    s_flip_color_applied = 0xFFFFFFFF;
}

static void ui_screen_saver_flip_digit_text(uint8_t digit, char *buf, size_t len)
{
    snprintf(buf, len, "%u", (unsigned)digit);
}

/* Second half of the fold: the digit has already been swapped in, the top flap
 * grows back to its full height. */
static void ui_screen_saver_flip_unfold(flip_digit_t *digit)
{
    char buf[4];
    ui_screen_saver_flip_digit_text(digit->shown, buf, sizeof(buf));
    lv_label_set_text(digit->top_label, buf);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, digit->top_clip);
    lv_anim_set_exec_cb(&anim, ui_screen_saver_flip_scale_cb);
    lv_anim_set_values(&anim, 0, LV_SCALE_NONE);
    lv_anim_set_duration(&anim, APP_DISPLAY_SAVER_FLIP_ANIM_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static void ui_screen_saver_flip_folded_cb(lv_anim_t *anim)
{
    ui_screen_saver_flip_unfold((flip_digit_t *)anim->user_data);
}

static void ui_screen_saver_flip_set_digit(flip_digit_t *digit, uint8_t value)
{
    if (digit->top_clip == NULL || digit->shown == value) {
        return;
    }

    char buf[4];
    ui_screen_saver_flip_digit_text(value, buf, sizeof(buf));

    bool first_fill = (digit->shown == FLIP_DIGIT_UNSET);
    digit->shown = value;

    if (first_fill) {
        /* Nothing to fold on the first fill: show the digit straight away. */
        lv_label_set_text(digit->top_label, buf);
        lv_label_set_text(digit->bottom_label, buf);
        lv_obj_set_style_transform_scale_y(digit->top_clip, LV_SCALE_NONE, LV_PART_MAIN);
        lv_obj_set_style_opa(digit->top_clip, LV_OPA_COVER, LV_PART_MAIN);
        return;
    }

    /* The lower half shows the new digit immediately, the upper flap folds down
     * over the seam and unfolds again with the new digit on it. */
    lv_label_set_text(digit->bottom_label, buf);

    lv_anim_delete(digit->top_clip, ui_screen_saver_flip_scale_cb);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, digit->top_clip);
    lv_anim_set_exec_cb(&anim, ui_screen_saver_flip_scale_cb);
    lv_anim_set_values(&anim, LV_SCALE_NONE, 0);
    lv_anim_set_duration(&anim, APP_DISPLAY_SAVER_FLIP_ANIM_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_user_data(&anim, digit);
    lv_anim_set_completed_cb(&anim, ui_screen_saver_flip_folded_cb);
    lv_anim_start(&anim);
}

static lv_obj_t *ui_screen_saver_flip_create_half(lv_obj_t *card, int32_t clip_y, int32_t label_y,
    lv_obj_t **label_out)
{
    lv_obj_t *clip = lv_obj_create(card);
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, APP_DISPLAY_SAVER_FLIP_CARD_W, APP_DISPLAY_SAVER_FLIP_CARD_H / 2);
    lv_obj_set_pos(clip, 0, clip_y);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(clip);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(label, "0");
    lv_obj_set_style_text_font(label, APP_FONT_CLOCK_84, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    /* A full card tall label inside a half card tall clip: the clip cuts it on
     * the seam, which is what makes the two halves of one digit line up. */
    lv_obj_set_size(label, APP_DISPLAY_SAVER_FLIP_CARD_W, APP_DISPLAY_SAVER_FLIP_CARD_H);
    lv_obj_set_pos(label, 0, label_y);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);

    *label_out = label;
    return clip;
}

static void ui_screen_saver_flip_create_card(uint32_t index, int32_t x)
{
    const int32_t half_h = APP_DISPLAY_SAVER_FLIP_CARD_H / 2;

    lv_obj_t *card = lv_obj_create(s_flip_row);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, APP_DISPLAY_SAVER_FLIP_CARD_W, APP_DISPLAY_SAVER_FLIP_CARD_H);
    lv_obj_set_pos(card, x, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(card, APP_DISPLAY_SAVER_FLIP_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(APP_DISPLAY_SAVER_FLIP_CARD_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);

    flip_digit_t *digit = &s_flip_digits[index];
    digit->shown = FLIP_DIGIT_UNSET; /* the first fill must not animate */
    digit->top_clip = ui_screen_saver_flip_create_half(card, 0, APP_DISPLAY_SAVER_FLIP_LABEL_Y,
        &digit->top_label);
    /* Fold line: the bottom edge of the upper flap. */
    lv_obj_set_style_transform_pivot_x(digit->top_clip, APP_DISPLAY_SAVER_FLIP_CARD_W / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(digit->top_clip, half_h, LV_PART_MAIN);
    ui_screen_saver_flip_create_half(card, half_h, APP_DISPLAY_SAVER_FLIP_LABEL_Y - half_h,
        &digit->bottom_label);

    /* Seam between the two halves, like the hinge of a real flip card. */
    lv_obj_t *seam = lv_obj_create(card);
    lv_obj_remove_style_all(seam);
    lv_obj_set_size(seam, APP_DISPLAY_SAVER_FLIP_CARD_W, 2);
    lv_obj_set_pos(seam, 0, half_h - 1);
    lv_obj_clear_flag(seam, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(seam, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seam, LV_OPA_40, LV_PART_MAIN);
}

static void ui_screen_saver_flip_build(lv_obj_t *parent)
{
    const int32_t card_w = APP_DISPLAY_SAVER_FLIP_CARD_W;
    const int32_t gap = APP_DISPLAY_SAVER_FLIP_CARD_GAP;
    const int32_t row_w = (card_w * FLIP_DIGIT_COUNT) + (gap * (FLIP_DIGIT_COUNT - 1));
    const int32_t row_y = APP_SCREEN_HEIGHT - APP_DISPLAY_SAVER_CLOCK_BOTTOM_GAP -
        APP_DISPLAY_SAVER_FLIP_CARD_H;

    s_flip_row = lv_obj_create(parent);
    lv_obj_remove_style_all(s_flip_row);
    lv_obj_set_size(s_flip_row, row_w, APP_DISPLAY_SAVER_FLIP_CARD_H);
    s_flip_row_w = row_w;
    lv_obj_set_pos(s_flip_row, (APP_SCREEN_WIDTH - row_w) / 2, row_y);
    lv_obj_clear_flag(s_flip_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (uint32_t i = 0; i < FLIP_DIGIT_COUNT; i++) {
        ui_screen_saver_flip_create_card(i, (int32_t)i * (card_w + gap));
    }

    s_flip_color_applied = s_clock_color;
}

static void ui_screen_saver_flip_update(int hour, int minute)
{
    if (s_flip_digits[0].top_clip == NULL) {
        return;
    }

    if (s_clock_color != s_flip_color_applied) {
        for (uint32_t i = 0; i < FLIP_DIGIT_COUNT; i++) {
            lv_obj_set_style_text_color(s_flip_digits[i].top_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_flip_digits[i].bottom_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
        }
        s_flip_color_applied = s_clock_color;
    }

    const uint8_t digits[FLIP_DIGIT_COUNT] = {
        (uint8_t)(hour / 10), (uint8_t)(hour % 10),
        (uint8_t)(minute / 10), (uint8_t)(minute % 10),
    };
    for (uint32_t i = 0; i < FLIP_DIGIT_COUNT; i++) {
        ui_screen_saver_flip_set_digit(&s_flip_digits[i], digits[i]);
    }
}

/* Height of the clock block on the overlay, used to stack the date caption
 * above it. */
static uint32_t ui_screen_saver_clock_block_height(void)
{
    if (s_clock_style == APP_DISPLAY_SAVER_CLOCK_STYLE_FLIP) {
        return APP_DISPLAY_SAVER_FLIP_CARD_H;
    }
    return lv_font_get_line_height(APP_FONT_CLOCK_84);
}

/* Backlight target for the current screensaver state. While the clock overlay
 * is up the panel drops to the configured screensaver brightness (readable but
 * far darker than the menu); hiding it restores the menu brightness. Never
 * brighter than the active setting, so a high saver value cannot surprise the
 * user with a brighter screen than the menu. */
static void ui_screen_saver_apply_backlight(void)
{
    if (s_screen_off) {
        return; /* the screen-off path owns the backlight */
    }

    int target = s_brightness;
    if (s_overlay_shown) {
        target = (s_saver_brightness < s_brightness) ? s_saver_brightness : s_brightness;
    }
    (void)display_set_brightness_percent(target);
}

static void ui_screen_saver_hide_overlay(void)
{
    if (s_overlay != NULL) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    s_overlay_shown = false;
    ui_screen_saver_apply_backlight();
}

void ui_screen_saver_handle_screen_clean(void)
{
    /* The overlay and its children are attached to the active screen and are
     * deleted by ui_pages_init()'s lv_obj_clean(). Drop every handle here so the
     * next tick rebuilds the overlay instead of writing into freed memory.
     * A running flip animation is cancelled first: it holds pointers into the
     * objects that are about to disappear.
     * The wallpaper buffer/dsc survive: they are owned by this module. */
    ui_screen_saver_flip_reset();
    s_overlay = NULL;
    s_time_label = NULL;
    s_date_label = NULL;
    s_bg_image = NULL;
    s_overlay_shown = false;
}

/* Centres the clock block horizontally, optionally pulled left by half of the
 * AM/PM badge so that the "HH:MM PM" group as a whole stays centred. */
static void ui_screen_saver_align_clock(int32_t shift)
{
    if (s_clock_style == APP_DISPLAY_SAVER_CLOCK_STYLE_FLIP) {
        if (s_flip_row != NULL) {
            lv_obj_set_x(s_flip_row, (APP_SCREEN_WIDTH - s_flip_row_w) / 2 - shift);
        }
    } else if (s_time_label != NULL) {
        lv_obj_align(s_time_label, LV_ALIGN_BOTTOM_MID, -shift, -APP_DISPLAY_SAVER_CLOCK_BOTTOM_GAP);
    }
}

/* One badge copy: same black plate, so the five of them overlap into a single
 * plate with visibly thicker letters. */
static void ui_screen_saver_ampm_style(lv_obj_t *label)
{
    lv_obj_set_style_text_font(label, APP_FONT_DISPLAY_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(label, APP_DISPLAY_SAVER_AMPM_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(label, APP_DISPLAY_SAVER_AMPM_PAD, LV_PART_MAIN);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

/* The badge always hangs off the clock base, so it survives both clock styles
 * and every re-centring of the digits. */
static lv_obj_t *ui_screen_saver_ampm_base(void)
{
    if (s_clock_style == APP_DISPLAY_SAVER_CLOCK_STYLE_FLIP) {
        return s_flip_row;
    }
    return s_time_label;
}

/* Small AM/PM badge for the 12 hour format, right next to the digits on a black
 * plate of its own, so the clock reads like any digital clock even over a bright
 * wallpaper. Hidden in the 24 hour format. */
static void ui_screen_saver_update_ampm(int hour24)
{
    if (s_ampm_copies[0] == NULL) {
        return;
    }

    uint8_t state = s_clock_24h ? 2 : (hour24 < 12 ? 0 : 1);
    if (state == s_ampm_state) {
        for (int i = 0; i < AMPM_COPY_COUNT; i++) {
            lv_obj_set_style_text_color(s_ampm_copies[i], lv_color_hex(s_clock_color), LV_PART_MAIN);
        }
        return;
    }
    s_ampm_state = state;

    if (state == 2) {
        for (int i = 0; i < AMPM_COPY_COUNT; i++) {
            lv_obj_add_flag(s_ampm_copies[i], LV_OBJ_FLAG_HIDDEN);
        }
        ui_screen_saver_align_clock(0);
        return;
    }

    lv_obj_t *base = ui_screen_saver_ampm_base();
    if (base == NULL) {
        return;
    }

    for (int i = 0; i < AMPM_COPY_COUNT; i++) {
        lv_label_set_text(s_ampm_copies[i], state == 0 ? "AM" : "PM");
        ui_screen_saver_ampm_style(s_ampm_copies[i]);
        lv_obj_clear_flag(s_ampm_copies[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* The badge width is only known once the new text has been laid out. */
    lv_obj_update_layout(s_ampm_copies[0]);
    ui_screen_saver_align_clock(
        (lv_obj_get_width(s_ampm_copies[0]) + 2 * APP_DISPLAY_SAVER_AMPM_WEIGHT +
            APP_DISPLAY_SAVER_AMPM_GAP) / 2);

    for (int i = 0; i < AMPM_COPY_COUNT; i++) {
        lv_obj_align_to(s_ampm_copies[i], base, LV_ALIGN_OUT_RIGHT_MID,
            APP_DISPLAY_SAVER_AMPM_GAP + s_ampm_copy_ofs[i][0] * APP_DISPLAY_SAVER_AMPM_WEIGHT,
            s_ampm_copy_ofs[i][1] * APP_DISPLAY_SAVER_AMPM_WEIGHT);
    }
}

static void ui_screen_saver_update_clock(void)
{
    if (s_overlay == NULL || s_date_label == NULL) {
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

    if (s_clock_style == APP_DISPLAY_SAVER_CLOCK_STYLE_FLIP) {
        /* Flipping cards show HH:MM only: a folding seconds digit would repaint
         * the panel once a second and cost more than it is worth. */
        ui_screen_saver_flip_update(hour, info.tm_min);
    } else if (s_time_label != NULL) {
        char time_buf[16] = {0};
        if (s_show_seconds) {
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", hour, info.tm_min, info.tm_sec);
        } else {
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, info.tm_min);
        }

        lv_label_set_text(s_time_label, time_buf);
        lv_obj_set_style_text_color(s_time_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    }

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

    if (s_time_label != NULL) {
        /* A NULL label happens in flip mode, where the digits live in cards. */
        lv_obj_set_style_text_color(s_time_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(s_date_color), LV_PART_MAIN);
    ui_screen_saver_update_ampm(info.tm_hour);
}

/* Deletes the whole clock overlay. Only call from the LVGL task. */
static void ui_screen_saver_destroy_overlay(void)
{
    ui_screen_saver_flip_reset();
    if (s_overlay != NULL) {
        lv_obj_delete(s_overlay);
    }
    s_overlay = NULL;
    s_time_label = NULL;
    for (int i = 0; i < AMPM_COPY_COUNT; i++) {
        s_ampm_copies[i] = NULL;
    }
    s_ampm_state = 2;
    s_date_label = NULL;
    s_bg_image = NULL;
    s_overlay_shown = false;
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

    /* Page backgrounds crop the frame above: without it they must fall back to
     * their plain background colour. */
    ui_page_style_reload_wallpaper();
}

const char *ui_screen_saver_wallpaper_path(void)
{
    return sd_card_is_mounted() ? APP_SD_WALLPAPER_PATH : APP_WALLPAPER_PATH;
}

const char *ui_screen_saver_wallpaper_tmp_path(void)
{
    return sd_card_is_mounted() ? APP_SD_WALLPAPER_TMP_PATH : APP_WALLPAPER_TMP_PATH;
}

bool ui_screen_saver_wallpaper_on_card(void)
{
    return sd_card_is_mounted();
}

bool ui_screen_saver_wallpaper_present(void)
{
    return s_wallpaper_loaded;
}

/* True when the file exists and holds exactly one full frame. */
static bool wallpaper_file_valid(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    bool valid = false;
    if (fseek(f, 0, SEEK_END) == 0 && ftell(f) == (long)APP_WALLPAPER_BYTES) {
        valid = true;
    }
    fclose(f);
    return valid;
}

/* Writes the frame held in PSRAM to `path` through a temporary file: used both
 * for the first copy onto a card and for the rescue copy back to flash. */
static bool wallpaper_store_buffer(const char *tmp_path, const char *path)
{
    if (!s_wallpaper_loaded || s_wallpaper_data == NULL) {
        return false;
    }

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        return false;
    }

    bool ok = fwrite(s_wallpaper_data, 1, APP_WALLPAPER_BYTES, f) == APP_WALLPAPER_BYTES;
    if (fclose(f) != 0) {
        ok = false;
    }

    /* rename() replaces an existing frame, so the target is never missing. */
    if (!ok || rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

static void ui_screen_saver_load_wallpaper(void)
{
    /* The frame is normally on the card, but the storage sync may not have moved
     * it there yet when the UI comes up, so both places are tried. */
    const bool on_card = sd_card_is_mounted();
    FILE *f = fopen(on_card ? APP_SD_WALLPAPER_PATH : APP_WALLPAPER_PATH, "rb");
    if (f == NULL) {
        f = fopen(on_card ? APP_WALLPAPER_PATH : APP_SD_WALLPAPER_PATH, "rb");
    }
    if (f == NULL) {
        ui_screen_saver_free_wallpaper();
        return;
    }

    uint8_t *data = NULL;
    do {
        if (fseek(f, 0, SEEK_END) != 0) {
            break;
        }
        long len = ftell(f);
        if (len != (long)APP_WALLPAPER_BYTES) {
            ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper size %ld != expected %u", len, (unsigned)APP_WALLPAPER_BYTES);
            break;
        }
        rewind(f);

#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
        data = (uint8_t *)heap_caps_malloc(APP_WALLPAPER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
        if (data == NULL) {
            data = (uint8_t *)heap_caps_malloc(APP_WALLPAPER_BYTES, MALLOC_CAP_8BIT);
        }
        if (data == NULL) {
            ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper buffer alloc failed");
            break;
        }

        size_t got = fread(data, 1, APP_WALLPAPER_BYTES, f);
        if (got != APP_WALLPAPER_BYTES) {
            heap_caps_free(data);
            data = NULL;
            ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper read short: %u/%u", (unsigned)got, (unsigned)APP_WALLPAPER_BYTES);
            break;
        }
    } while (0);
    fclose(f);

    if (data == NULL) {
        ui_screen_saver_free_wallpaper();
        return;
    }

    /* Publish the freshly read frame first and only then release the previous
     * buffer: the old frame is still referenced by the LVGL objects (screensaver
     * overlay and page backgrounds) until they are re-pointed below, and freeing
     * it up-front left the render task drawing freed memory. */
    uint8_t *previous = s_wallpaper_data;
    s_wallpaper_data = data;

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

    /* Page backgrounds that use the wallpaper point at a crop of the frame. */
    ui_page_style_reload_wallpaper();

    if (previous != NULL) {
        heap_caps_free(previous);
    }
}

const lv_image_dsc_t *ui_screen_saver_wallpaper_dsc(void)
{
    if (!s_wallpaper_loaded || s_wallpaper_data == NULL) {
        return NULL;
    }
    return &s_wallpaper_dsc;
}

void ui_screen_saver_reload_wallpaper(void)
{
    /* The API handlers run on the HTTP task: take the display lock so the swap
     * of the wallpaper buffer cannot race the LVGL render task. */
    if (!display_lock(WALLPAPER_RELOAD_LOCK_MS)) {
        ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper reload: display lock busy");
        return;
    }
    ui_screen_saver_load_wallpaper();
    display_unlock();
}

void ui_screen_saver_wallpaper_sync(sd_card_event_t event)
{
    /* Serialised against ui_screen_saver_reload_wallpaper(), which replaces the
     * very buffer that is handed to the card here. */
    if (!display_lock(WALLPAPER_SYNC_LOCK_MS)) {
        ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper storage sync skipped: display lock busy");
        return;
    }

    if (event == SD_CARD_EVENT_MOUNTED && sd_card_is_mounted()) {
        if (!wallpaper_file_valid(APP_SD_WALLPAPER_PATH) && s_wallpaper_loaded) {
            if (wallpaper_store_buffer(APP_SD_WALLPAPER_TMP_PATH, APP_SD_WALLPAPER_PATH)) {
                ESP_LOGI(TAG_SCREEN_SAVER, "wallpaper copied to the microSD card");
            } else {
                ESP_LOGW(TAG_SCREEN_SAVER, "wallpaper copy to the microSD card failed");
            }
        }
        /* Single copy: while a card is in, the frame lives on it. */
        if (wallpaper_file_valid(APP_SD_WALLPAPER_PATH) &&
            remove(APP_WALLPAPER_PATH) == 0) {
            ESP_LOGI(TAG_SCREEN_SAVER, "wallpaper dropped from internal flash: the card holds the frame");
        }
    } else if (event == SD_CARD_EVENT_RELEASING && s_wallpaper_loaded) {
        /* The card is still readable here, which is the point of this event:
         * without the copy back a card-less reboot would lose the frame. */
        if (!wallpaper_file_valid(APP_WALLPAPER_PATH) &&
            wallpaper_store_buffer(APP_WALLPAPER_TMP_PATH, APP_WALLPAPER_PATH)) {
            ESP_LOGI(TAG_SCREEN_SAVER, "wallpaper copied back to internal flash");
        }
    }

    display_unlock();
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

    if (s_clock_style == APP_DISPLAY_SAVER_CLOCK_STYLE_FLIP) {
        ui_screen_saver_flip_build(s_overlay);
    } else {
        s_time_label = lv_label_create(s_overlay);
        lv_obj_set_style_text_font(s_time_label, APP_FONT_CLOCK_84, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_time_label, lv_color_hex(s_clock_color), LV_PART_MAIN);
        lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(s_time_label, LV_SIZE_CONTENT);
        /* Clock sits near the bottom edge with the date stacked above it. */
        lv_obj_align(s_time_label, LV_ALIGN_BOTTOM_MID, 0, -APP_DISPLAY_SAVER_CLOCK_BOTTOM_GAP);
    }

    /* 12 hour format badge; positioned and shown by the clock tick. Five copies
     * of the same text, each on its own black plate, one pixel apart: the result
     * is a bold "PM" on a single plate (see ui_screen_saver_ampm_style). */
    for (int i = 0; i < AMPM_COPY_COUNT; i++) {
        lv_obj_t *copy = lv_label_create(s_overlay);
        ui_screen_saver_ampm_style(copy);
        lv_label_set_text(copy, "PM");
        lv_obj_add_flag(copy, LV_OBJ_FLAG_HIDDEN);
        s_ampm_copies[i] = copy;
    }
    s_ampm_state = 2;

    s_date_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_date_label, APP_FONT_TEXT_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(s_date_color), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_date_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(s_date_label, LV_SIZE_CONTENT);
    lv_obj_align(s_date_label, LV_ALIGN_BOTTOM_MID, 0,
        -APP_DISPLAY_SAVER_CLOCK_BOTTOM_GAP - ui_screen_saver_clock_block_height() -
            APP_DISPLAY_SAVER_DATE_GAP);
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
    ui_screen_saver_apply_backlight();
}

/* Runs from display_note_activity() on any real interaction (touch, boot,
 * API activity). Clears wake-to-screensaver so a touch dismisses the clock. */
static void ui_screen_saver_on_activity(void)
{
    s_wake_to_screensaver = false;
}

/* Minutes since midnight, or -1 while the clock has not been synced yet (the
 * night schedule never engages with an unknown time). */
static int ui_screen_saver_minutes_of_day(void)
{
    time_t now = time(NULL);
    struct tm info = {0};
    localtime_r(&now, &info);
    if (info.tm_year < 120) {
        return -1;
    }
    return (info.tm_hour * 60) + info.tm_min;
}

/* True inside [start, end). The window may cross midnight (22:00 -> 06:00). */
static bool ui_screen_saver_night_window_active(int minutes_of_day)
{
    if (!s_night_enabled || minutes_of_day < 0) {
        return false;
    }
    if (s_night_start_min == s_night_end_min) {
        return false;
    }
    if (s_night_start_min < s_night_end_min) {
        return minutes_of_day >= (int)s_night_start_min && minutes_of_day < (int)s_night_end_min;
    }
    return minutes_of_day >= (int)s_night_start_min || minutes_of_day < (int)s_night_end_min;
}

bool ui_screen_saver_night_active(void)
{
    return s_night_active;
}

static void ui_screen_saver_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_style_pending) {
        /* The clock style changed: rebuild the overlay here and not in the
         * request handler, LVGL objects are only touched from this task. */
        s_style_pending = false;
        ui_screen_saver_destroy_overlay();
        ui_screen_saver_apply_backlight();
    }

    int64_t idle_ms = display_ms_since_activity();
    bool activity_recent = idle_ms < SCREEN_SAVER_TICK_MS;

    bool night_now = ui_screen_saver_night_window_active(ui_screen_saver_minutes_of_day());
    if (night_now != s_night_active) {
        s_night_active = night_now;
        s_night_wake_until_ms = 0;
        ESP_LOGI(TAG_SCREEN_SAVER, "night schedule %s (brightness=%u%%, wake=%us)",
                 night_now ? "on" : "off", (unsigned)s_night_brightness, (unsigned)s_night_wake_sec);
        if (!night_now) {
            /* Leaving the window: hand the backlight back to the normal logic. */
            s_screen_off = false;
            display_note_activity();
            ui_screen_saver_apply_backlight();
        }
    }

    if (s_night_active) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (activity_recent) {
            s_night_wake_until_ms = now_ms + ((int64_t)s_night_wake_sec * 1000);
        }
        if (now_ms < s_night_wake_until_ms) {
            /* Temporary touch wake: the full UI stays usable, then the panel
             * drops back to the night level once the window expires. */
            s_screen_off = false;
            ui_screen_saver_hide_overlay();
            return;
        }

        s_night_wake_until_ms = 0;
        if (s_night_brightness == 0) {
            s_screen_off = true;
            ui_screen_saver_hide_overlay();
            (void)display_set_brightness_percent(0);
        } else {
            s_screen_off = false;
            if (s_screensaver_enabled) {
                ui_screen_saver_show_overlay();
            } else {
                ui_screen_saver_hide_overlay();
            }
            (void)display_set_brightness_percent((int)s_night_brightness);
        }
        return;
    }

    if (s_screen_off) {
        if (activity_recent) {
            s_screen_off = false;
            ui_screen_saver_apply_backlight();
            ESP_LOGI(TAG_SCREEN_SAVER, "wake (activity) brightness=%d%%", (int)s_brightness);
        } else {
            return;
        }
    }

    if (s_screen_off_enabled && s_screen_off_timeout_sec > 0 &&
        idle_ms >= (int64_t)s_screen_off_timeout_sec * 1000) {
        s_screen_off = true;
        s_wake_to_screensaver = false;
        ui_screen_saver_hide_overlay();
        (void)display_set_brightness_percent(0);
        ESP_LOGI(TAG_SCREEN_SAVER, "screen off (idle=%d ms)", (int)idle_ms);
        return;
    }

    if (!s_screensaver_enabled) {
        /* Screensaver fully disabled: hide the clock and never auto-show it. */
        ui_screen_saver_hide_overlay();
        s_wake_to_screensaver = false;
        return;
    }

    if (s_wake_to_screensaver) {
        /* Motion-sensor wake: keep the clock overlay visible until the
         * screen-off timeout above turns the backlight off. */
        if (!s_overlay_shown) {
            ESP_LOGI(TAG_SCREEN_SAVER, "wake-to-screensaver shown (idle=%d ms)", (int)idle_ms);
        }
        ui_screen_saver_show_overlay();
        return;
    }

    if (s_screensaver_enabled && s_screensaver_timeout_sec > 0 &&
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
    bool saver_brightness_changed = (settings->display_saver_brightness != s_saver_brightness);
    bool off_just_disabled = (s_screen_off_enabled && !settings->display_screen_off_enabled);
    bool style_changed = (settings->display_saver_clock_style != s_clock_style);

    s_brightness = settings->display_brightness;
    s_saver_brightness = settings->display_saver_brightness;
    s_screensaver_enabled = settings->display_screensaver_enabled;
    s_screensaver_timeout_sec = settings->display_screensaver_timeout_sec;
    s_screen_off_enabled = settings->display_screen_off_enabled;
    s_screen_off_timeout_sec = settings->display_screen_off_timeout_sec;
    s_clock_24h = settings->display_clock_24h;
    s_show_seconds = settings->display_saver_show_seconds;
    s_show_date = settings->display_saver_show_date;
    s_clock_style = settings->display_saver_clock_style;
    s_clock_color = settings->display_saver_clock_color;
    s_date_color = settings->display_saver_date_color;
    s_night_enabled = settings->display_night_mode_enabled;
    s_night_start_min = settings->display_night_start_min;
    s_night_end_min = settings->display_night_end_min;
    s_night_brightness = settings->display_night_brightness;
    s_night_wake_sec = settings->display_night_wake_sec;

    if (style_changed) {
        /* Rebuild on the next LVGL tick; the overlay may be on screen now. */
        s_style_pending = true;
    }

    /* Keep the wake/touch restore brightness in sync with the user setting. */
    display_set_active_brightness_percent((int)s_brightness);

    if (s_screen_off && (brightness_changed || saver_brightness_changed || off_just_disabled)) {
        /* Wake the panel so the user sees the effect of the change. */
        s_screen_off = false;
        display_note_activity();
    } else if (!s_screen_off) {
        /* Screen is on: re-apply the menu or screensaver backlight level. */
        ui_screen_saver_apply_backlight();
    }

    if (!s_screensaver_enabled) {
        /* Clock disabled: stop forcing it and let the timer hide the overlay. */
        s_wake_to_screensaver = false;
    }
}

void ui_screen_saver_wake(void)
{
    if (s_screen_off) {
        /* Panel is off: wake it to the screensaver clock and restart the idle
         * timer so the screen-off timeout counts from this moment. */
        s_screen_off = false;
        display_note_activity(); /* restores brightness + clears flag via cb */
        s_wake_to_screensaver = s_screensaver_enabled;
        ESP_LOGI(TAG_SCREEN_SAVER, "wake: off -> %s (brightness=%d%%)",
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
    s_saver_brightness = settings.display_saver_brightness;
    s_screensaver_enabled = settings.display_screensaver_enabled;
    s_screensaver_timeout_sec = settings.display_screensaver_timeout_sec;
    s_screen_off_enabled = settings.display_screen_off_enabled;
    s_screen_off_timeout_sec = settings.display_screen_off_timeout_sec;
    s_clock_24h = settings.display_clock_24h;
    s_show_seconds = settings.display_saver_show_seconds;
    s_show_date = settings.display_saver_show_date;
    s_clock_style = settings.display_saver_clock_style;
    s_clock_color = settings.display_saver_clock_color;
    s_date_color = settings.display_saver_date_color;
    s_night_enabled = settings.display_night_mode_enabled;
    s_night_start_min = settings.display_night_start_min;
    s_night_end_min = settings.display_night_end_min;
    s_night_brightness = settings.display_night_brightness;
    s_night_wake_sec = settings.display_night_wake_sec;

    /* Sync wake/touch restore brightness and register the activity callback
     * so a real touch dismisses a wake-to-screensaver overlay. */
    display_set_active_brightness_percent((int)s_brightness);
    display_set_activity_callback(ui_screen_saver_on_activity);

    ui_screen_saver_load_wallpaper();

    if (s_timer == NULL) {
        s_timer = lv_timer_create(ui_screen_saver_timer_cb, SCREEN_SAVER_TICK_MS, NULL);
    }
}
