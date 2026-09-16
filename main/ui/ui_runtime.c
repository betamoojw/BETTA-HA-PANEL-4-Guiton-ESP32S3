/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "ui/ui_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "app_events.h"
#include "app_task.h"
#include "diag/system_log.h"
#include "drivers/display_init.h"
#include "ha/ha_client.h"
#include "ha/ha_model.h"
#include "layout/layout_store.h"
#include "net/wifi_mgr.h"
#include "ui/fonts/mdi_font_registry.h"
#include "ui/ui_energy_page.h"
#include "ui/ui_memory.h"
#include "ui/ui_music_page.h"
#include "ui/ui_pages.h"
#include "ui/ui_page_style.h"
#include "ui/ui_page_transition.h"
#include "ui/ui_press_feedback.h"
#include "ui/ui_theme_router.h"
#include "ui/ui_value_anim.h"
#include "ui/ui_screen_saver.h"
#include "ui/ui_widget_factory.h"
#include "ui/theme/theme_default.h"
#include "util/log_tags.h"

#define UI_MODEL_RECONCILE_INTERVAL_MS 1000

static ui_widget_instance_t *s_widgets = NULL;
static size_t s_widget_count = 0;
static ui_energy_page_instance_t *s_energy_pages = NULL;
static size_t s_energy_page_count = 0;
static ui_music_page_instance_t *s_music_pages = NULL;
static size_t s_music_page_count = 0;

static void ui_runtime_update_widget_visibility(const char *page_id, bool refresh_visible_widgets);

/* Invoked by ui_pages whenever the active page changes.
 * If the newly shown page is an energy dashboard, ask the HA client to
 * refresh the statistics immediately so the user sees fresh kWh values
 * instead of waiting for the next HA_ENERGY_SYNC_INTERVAL_MS tick. */
static void ui_runtime_on_page_shown(const char *page_id, uint16_t index)
{
    (void)index;
    if (page_id == NULL || page_id[0] == '\0') {
        return;
    }
    for (size_t i = 0; i < s_energy_page_count; i++) {
        if (strncmp(s_energy_pages[i].page_id, page_id, APP_MAX_PAGE_ID_LEN) == 0) {
            (void)ha_client_request_energy_refresh();
            (void)ui_energy_page_apply_all_states(&s_energy_pages[i]);
            break;
        }
    }
    for (size_t i = 0; i < s_music_page_count; i++) {
        if (strncmp(s_music_pages[i].page_id, page_id, APP_MAX_PAGE_ID_LEN) == 0) {
            (void)ui_music_page_apply_all_states(&s_music_pages[i]);
            break;
        }
    }
    ui_runtime_update_widget_visibility(page_id, true);
    /* A page may carry its own theme ("page_theme"); switching it is deferred to
     * the UI loop so the pages are never rebuilt from inside this callback. */
    ui_theme_router_notify_page_shown(page_id);
}
static TaskHandle_t s_ui_task = NULL;
static volatile uint32_t s_heartbeat = 0;
static ha_state_t s_state_scratch;
static bool s_initialized = false;
static int64_t s_last_topbar_refresh_ms = 0;
static int64_t s_last_model_reconcile_ms = 0;
static uint32_t s_last_model_revision = 0;
static bool s_model_reconcile_pending = false;
static bool s_pending_state_reconcile = false;
static bool s_pending_topbar_refresh = false;
static uint32_t s_deferred_event_count = 0;
static int64_t s_deferred_event_log_ms = 0;

static esp_err_t ui_runtime_alloc_buffers(void)
{
    if (s_widgets == NULL) {
        s_widgets = ui_calloc_prefer_psram(APP_MAX_WIDGETS_TOTAL, sizeof(*s_widgets));
        if (s_widgets == NULL) {
            ESP_LOGE(TAG_UI, "Failed to allocate widget runtime buffer (%u slots)",
                     (unsigned)APP_MAX_WIDGETS_TOTAL);
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_energy_pages == NULL) {
        s_energy_pages = ui_calloc_prefer_psram(APP_MAX_PAGES, sizeof(*s_energy_pages));
        if (s_energy_pages == NULL) {
            ESP_LOGE(TAG_UI, "Failed to allocate energy page runtime buffer (%u pages)", (unsigned)APP_MAX_PAGES);
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_music_pages == NULL) {
        s_music_pages = ui_calloc_prefer_psram(APP_MAX_PAGES, sizeof(*s_music_pages));
        if (s_music_pages == NULL) {
            ESP_LOGE(TAG_UI, "Failed to allocate music page runtime buffer (%u pages)", (unsigned)APP_MAX_PAGES);
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

typedef struct {
    bool valid;
    int minute;
    int hour;
    int day;
    int month;
    int year;
    bool wifi_connected;
    bool wifi_setup_ap_active;
    bool ha_connected;
    bool ha_initial_sync_done;
} ui_topbar_cache_t;
static ui_topbar_cache_t s_topbar_cache = {0};
/* Set when a layout reload could not take the display lock; the UI task loop
 * retries so a saved layout is never silently dropped. */
static bool s_layout_reload_pending = false;
static int64_t s_layout_reload_retry_ms = 0;
/* Page the next rebuild should show: set by request_layout_reload_on_page() so
 * a theme change does not throw the user back to the first page. */
static char s_restore_page_id[APP_MAX_PAGE_ID_LEN];
static bool s_restore_page_id_valid = false;

/* How long a layout rebuild waits for the display lock. The render task only
 * holds it for one frame, so this is generous. */
#define UI_LAYOUT_LOCK_TIMEOUT_MS 2000
#define UI_LAYOUT_RETRY_INTERVAL_MS 250
#define UI_LAYOUT_PUBLISH_TIMEOUT_MS 200

static esp_err_t ui_runtime_apply_layout_locked(const char *layout_json);

#if APP_UI_TEST_WEATHER_ICON_OVERLAY
static lv_obj_t *s_weather_icon_overlay = NULL;
#endif

typedef struct {
    int min_w;
    int min_h;
    int max_w;
    int max_h;
} ui_widget_size_limits_t;

static ui_widget_size_limits_t ui_runtime_widget_size_limits(const char *type)
{
    ui_widget_size_limits_t limits = {
        .min_w = 60,
        .min_h = 60,
        .max_w = APP_CONTENT_BOX_WIDTH,
        .max_h = APP_CONTENT_BOX_HEIGHT,
    };

    if (type == NULL) {
        return limits;
    }

    if (strcmp(type, "sensor") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 90;
        limits.min_h = 60;
#else
        limits.min_w = 120;
        limits.min_h = 80;
#endif
    } else if (strcmp(type, "binary_sensor") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 90;
        limits.min_h = 60;
#else
        limits.min_w = 120;
        limits.min_h = 80;
#endif
    } else if (strcmp(type, "alarm_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 150;
        limits.min_h = 110;
#else
        limits.min_w = 200;
        limits.min_h = 140;
#endif
    } else if (strcmp(type, "clock_alarm") == 0) {
        /* Must fit the clock plus the optional date row; the date row is
         * dropped automatically on small tiles. */
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 110;
        limits.min_h = 80;
#else
        limits.min_w = 150;
        limits.min_h = 110;
#endif
    } else if (strcmp(type, "button") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 82;
        limits.min_h = 82;
        limits.max_w = 320;
        limits.max_h = 260;
#else
        limits.min_w = 100;
        limits.min_h = 100;
        limits.max_w = 480;
        limits.max_h = 320;
#endif
    } else if (strcmp(type, "slider") == 0) {
        limits.min_w = 100;
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_h = 80;
#else
        limits.min_h = 100;
#endif
    } else if (strcmp(type, "graph") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 150;
        limits.min_h = 100;
#else
        limits.min_w = 220;
        limits.min_h = 140;
#endif
    } else if (strcmp(type, "empty_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 100;
        limits.min_h = 70;
#else
        limits.min_w = 120;
        limits.min_h = 80;
#endif
    } else if (strcmp(type, "light_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 140;
        limits.min_h = 140;
#else
        limits.min_w = 180;
        limits.min_h = 180;
#endif
        limits.max_w = 480;
        limits.max_h = 480;
    } else if (strcmp(type, "heating_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 150;
        limits.min_h = 150;
#else
        limits.min_w = 220;
        limits.min_h = 200;
#endif
        limits.max_w = 480;
        limits.max_h = 480;
    } else if (strcmp(type, "weather_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 160;
        limits.min_h = 150;
#else
        limits.min_w = 220;
        limits.min_h = 200;
#endif
        limits.max_w = 480;
        limits.max_h = 480;
    } else if (strcmp(type, "weather_3day") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 300;
        limits.min_h = 190;
#else
        limits.min_w = 260;
        limits.min_h = 220;
#endif
        limits.max_w = 640;
        limits.max_h = 480;
    } else if (strcmp(type, "todo_list") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 180;
        limits.min_h = 160;
#else
        limits.min_w = 220;
        limits.min_h = 200;
#endif
        limits.max_w = 640;
        limits.max_h = 640;
    } else if (strcmp(type, "media_player") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 200;
        limits.min_h = 170;
#else
        limits.min_w = 260;
        limits.min_h = 220;
#endif
        limits.max_w = APP_CONTENT_BOX_WIDTH;
        limits.max_h = APP_CONTENT_BOX_HEIGHT;
    } else if (strcmp(type, "roborock_tile") == 0) {
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
        limits.min_w = 220;
        limits.min_h = 190;
#else
        limits.min_w = 240;
        limits.min_h = 220;
#endif
        limits.max_w = APP_CONTENT_BOX_WIDTH;
        limits.max_h = APP_CONTENT_BOX_HEIGHT;
    }

    if (limits.max_w > APP_CONTENT_BOX_WIDTH) {
        limits.max_w = APP_CONTENT_BOX_WIDTH;
    }
    if (limits.max_h > APP_CONTENT_BOX_HEIGHT) {
        limits.max_h = APP_CONTENT_BOX_HEIGHT;
    }
    return limits;
}

static void ui_runtime_clamp_widget_rect(ui_widget_def_t *def)
{
    if (def == NULL) {
        return;
    }

    ui_widget_size_limits_t limits = ui_runtime_widget_size_limits(def->type);

    if (def->w < limits.min_w) {
        def->w = limits.min_w;
    }
    if (def->h < limits.min_h) {
        def->h = limits.min_h;
    }
    if (def->w > limits.max_w) {
        def->w = limits.max_w;
    }
    if (def->h > limits.max_h) {
        def->h = limits.max_h;
    }

    if (def->x < 0) {
        def->x = 0;
    }
    if (def->y < 0) {
        def->y = 0;
    }

    if (def->x + def->w > APP_CONTENT_BOX_WIDTH) {
        def->x = APP_CONTENT_BOX_WIDTH - def->w;
    }
    if (def->y + def->h > APP_CONTENT_BOX_HEIGHT) {
        def->y = APP_CONTENT_BOX_HEIGHT - def->h;
    }

    if (def->x < 0) {
        def->x = 0;
    }
    if (def->y < 0) {
        def->y = 0;
    }
}

static void ui_runtime_refresh_topbar(void)
{
    time_t now = time(NULL);
    struct tm info = {0};
    localtime_r(&now, &info);

    bool wifi_connected = wifi_mgr_is_connected();
    bool wifi_setup_ap_active = wifi_mgr_is_setup_ap_active();
    bool ha_connected = ha_client_is_connected();
    bool ha_initial_sync_done = ha_client_is_initial_sync_done();

    bool datetime_changed = !s_topbar_cache.valid || info.tm_min != s_topbar_cache.minute ||
                            info.tm_hour != s_topbar_cache.hour || info.tm_mday != s_topbar_cache.day ||
                            info.tm_mon != s_topbar_cache.month || info.tm_year != s_topbar_cache.year;
    bool status_changed = !s_topbar_cache.valid || wifi_connected != s_topbar_cache.wifi_connected ||
                          wifi_setup_ap_active != s_topbar_cache.wifi_setup_ap_active ||
                          ha_connected != s_topbar_cache.ha_connected ||
                          ha_initial_sync_done != s_topbar_cache.ha_initial_sync_done;

    if (datetime_changed) {
        ui_pages_set_topbar_datetime(&info);
    }
    if (status_changed) {
        ui_pages_set_topbar_status(wifi_connected, wifi_setup_ap_active, ha_connected, ha_initial_sync_done);
    }

    s_topbar_cache.valid = true;
    s_topbar_cache.minute = info.tm_min;
    s_topbar_cache.hour = info.tm_hour;
    s_topbar_cache.day = info.tm_mday;
    s_topbar_cache.month = info.tm_mon;
    s_topbar_cache.year = info.tm_year;
    s_topbar_cache.wifi_connected = wifi_connected;
    s_topbar_cache.wifi_setup_ap_active = wifi_setup_ap_active;
    s_topbar_cache.ha_connected = ha_connected;
    s_topbar_cache.ha_initial_sync_done = ha_initial_sync_done;
}

static void ui_runtime_show_weather_icon_overlay(void)
{
#if APP_UI_TEST_WEATHER_ICON_OVERLAY
    if (s_weather_icon_overlay != NULL) {
        lv_obj_move_foreground(s_weather_icon_overlay);
        return;
    }

    const lv_font_t *font = mdi_font_weather();
    if (font == NULL) {
        font = mdi_font_large();
    }

    s_weather_icon_overlay = lv_label_create(lv_layer_top());
    lv_obj_add_flag(s_weather_icon_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_text_color(s_weather_icon_overlay, lv_color_hex(0x2FE3E3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_weather_icon_overlay, LV_OPA_TRANSP, LV_PART_MAIN);
    if (font != NULL) {
        lv_obj_set_style_text_font(s_weather_icon_overlay, font, LV_PART_MAIN);
    }

    /* Rainy icon U+F0597 rendered directly from weather font on top layer. */
    lv_label_set_text(s_weather_icon_overlay, "\xF3\xB0\x96\x97");
    lv_obj_align(s_weather_icon_overlay, LV_ALIGN_CENTER, 0, -20);
    lv_obj_move_foreground(s_weather_icon_overlay);

    ESP_LOGI(TAG_UI, "Weather icon overlay test enabled (font=%s)",
        (mdi_font_weather() != NULL) ? "72/56" : "none");
#endif
}

static void ui_runtime_apply_entity_state_ex(const char *entity_id, bool mark_unavailable_if_missing)
{
    if (entity_id == NULL || entity_id[0] == '\0') {
        return;
    }

    memset(&s_state_scratch, 0, sizeof(s_state_scratch));
    bool found = ha_model_get_state(entity_id, &s_state_scratch);
    for (size_t i = 0; i < s_widget_count; i++) {
        bool is_primary = (strncmp(entity_id, s_widgets[i].entity_id, APP_MAX_ENTITY_ID_LEN) == 0);
        bool is_secondary = (s_widgets[i].secondary_entity_id[0] != '\0') &&
                            (strncmp(entity_id, s_widgets[i].secondary_entity_id, APP_MAX_ENTITY_ID_LEN) == 0);
        if (!is_primary && !is_secondary) {
            continue;
        }
        if (found) {
            ui_widget_factory_apply_state(&s_widgets[i], &s_state_scratch);
        } else if (is_primary && mark_unavailable_if_missing) {
            ui_widget_factory_mark_unavailable(&s_widgets[i]);
        }
    }

    if (found) {
        for (size_t i = 0; i < s_energy_page_count; i++) {
            (void)ui_energy_page_apply_state(&s_energy_pages[i], &s_state_scratch);
        }
        for (size_t i = 0; i < s_music_page_count; i++) {
            if (ui_music_page_apply_state_detect_play_start(&s_music_pages[i], &s_state_scratch)) {
                /* Playback just started: wake the panel (dismiss the
                 * screensaver and restore full brightness) and jump straight
                 * to the music tab. */
                display_note_activity();
                (void)ui_pages_show(s_music_pages[i].page_id);
            }
        }
    }
}

static void ui_runtime_apply_entity_state(const char *entity_id)
{
    ui_runtime_apply_entity_state_ex(entity_id, true);
}

static void ui_runtime_apply_widget_current_state(ui_widget_instance_t *widget, bool mark_unavailable_if_missing)
{
    if (widget == NULL) {
        return;
    }

    if (widget->entity_id[0] != '\0') {
        memset(&s_state_scratch, 0, sizeof(s_state_scratch));
        bool found = ha_model_get_state(widget->entity_id, &s_state_scratch);
        if (found) {
            ui_widget_factory_apply_state(widget, &s_state_scratch);
        } else if (mark_unavailable_if_missing) {
            ui_widget_factory_mark_unavailable(widget);
        }
    }

    if (widget->secondary_entity_id[0] != '\0' &&
        strncmp(widget->secondary_entity_id, widget->entity_id, APP_MAX_ENTITY_ID_LEN) != 0) {
        memset(&s_state_scratch, 0, sizeof(s_state_scratch));
        if (ha_model_get_state(widget->secondary_entity_id, &s_state_scratch)) {
            ui_widget_factory_apply_state(widget, &s_state_scratch);
        }
    }
}

static void ui_runtime_update_widget_visibility(const char *page_id, bool refresh_visible_widgets)
{
    if (page_id == NULL || page_id[0] == '\0') {
        return;
    }

    for (size_t i = 0; i < s_widget_count; i++) {
        bool visible = (strncmp(s_widgets[i].page_id, page_id, APP_MAX_PAGE_ID_LEN) == 0);
        ui_widget_factory_set_visible(&s_widgets[i], visible);
        if (visible && refresh_visible_widgets) {
            ui_runtime_apply_widget_current_state(&s_widgets[i], false);
        }
    }
}

static void ui_runtime_apply_all_states(void)
{
    for (size_t i = 0; i < s_widget_count; i++) {
        ui_runtime_apply_entity_state(s_widgets[i].entity_id);
        if (s_widgets[i].secondary_entity_id[0] != '\0' &&
            strncmp(s_widgets[i].secondary_entity_id, s_widgets[i].entity_id, APP_MAX_ENTITY_ID_LEN) != 0) {
            ui_runtime_apply_entity_state(s_widgets[i].secondary_entity_id);
        }
    }
    for (size_t i = 0; i < s_energy_page_count; i++) {
        ui_energy_page_apply_all_states(&s_energy_pages[i]);
    }
    for (size_t i = 0; i < s_music_page_count; i++) {
        ui_music_page_apply_all_states(&s_music_pages[i]);
    }
}

static void ui_runtime_apply_all_states_preserve_missing(void)
{
    for (size_t i = 0; i < s_widget_count; i++) {
        ui_runtime_apply_entity_state_ex(s_widgets[i].entity_id, false);
        if (s_widgets[i].secondary_entity_id[0] != '\0' &&
            strncmp(s_widgets[i].secondary_entity_id, s_widgets[i].entity_id, APP_MAX_ENTITY_ID_LEN) != 0) {
            ui_runtime_apply_entity_state_ex(s_widgets[i].secondary_entity_id, false);
        }
    }
    for (size_t i = 0; i < s_energy_page_count; i++) {
        ui_energy_page_apply_all_states(&s_energy_pages[i]);
    }
    for (size_t i = 0; i < s_music_page_count; i++) {
        ui_music_page_apply_all_states(&s_music_pages[i]);
    }
}

static void ui_runtime_copy_json_string(cJSON *obj, const char *key, char *dst, size_t dst_size);

static bool ui_runtime_widget_from_json(cJSON *widget_json, ui_widget_def_t *out)
{
    cJSON *id = cJSON_GetObjectItemCaseSensitive(widget_json, "id");
    cJSON *type = cJSON_GetObjectItemCaseSensitive(widget_json, "type");
    cJSON *title = cJSON_GetObjectItemCaseSensitive(widget_json, "title");
    cJSON *entity_id = cJSON_GetObjectItemCaseSensitive(widget_json, "entity_id");
    cJSON *secondary_entity_id = cJSON_GetObjectItemCaseSensitive(widget_json, "secondary_entity_id");
    cJSON *slider_direction = cJSON_GetObjectItemCaseSensitive(widget_json, "slider_direction");
    cJSON *slider_accent_color = cJSON_GetObjectItemCaseSensitive(widget_json, "slider_accent_color");
    cJSON *button_accent_color = cJSON_GetObjectItemCaseSensitive(widget_json, "button_accent_color");
    cJSON *button_mode = cJSON_GetObjectItemCaseSensitive(widget_json, "button_mode");
    cJSON *graph_line_color = cJSON_GetObjectItemCaseSensitive(widget_json, "graph_line_color");
    cJSON *graph_point_count = cJSON_GetObjectItemCaseSensitive(widget_json, "graph_point_count");
    cJSON *graph_time_window_min = cJSON_GetObjectItemCaseSensitive(widget_json, "graph_time_window_min");
    cJSON *graph_display_mode = cJSON_GetObjectItemCaseSensitive(widget_json, "graph_display_mode");
    cJSON *graph_bar_bucket_min = cJSON_GetObjectItemCaseSensitive(widget_json, "graph_bar_bucket_min");
    cJSON *style_variant = cJSON_GetObjectItemCaseSensitive(widget_json, "style_variant");
    cJSON *arc_opening = cJSON_GetObjectItemCaseSensitive(widget_json, "arc_opening");
    cJSON *binary_color_on = cJSON_GetObjectItemCaseSensitive(widget_json, "binary_color_on");
    cJSON *binary_color_off = cJSON_GetObjectItemCaseSensitive(widget_json, "binary_color_off");
    cJSON *binary_text_on = cJSON_GetObjectItemCaseSensitive(widget_json, "binary_text_on");
    cJSON *binary_text_off = cJSON_GetObjectItemCaseSensitive(widget_json, "binary_text_off");
    cJSON *binary_show_title = cJSON_GetObjectItemCaseSensitive(widget_json, "binary_show_title");
    cJSON *alarm_code = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_code");
    cJSON *alarm_modes = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_modes");
    cJSON *alarm_ask_code = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_ask_code");
    cJSON *alarm_show_sensors = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_show_sensors");
    cJSON *alarm_show_bypassed = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_show_bypassed");
    cJSON *alarm_force_arm = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_force_arm");
    cJSON *alarm_skip_delay = cJSON_GetObjectItemCaseSensitive(widget_json, "alarm_skip_delay");
    cJSON *clock_show_seconds = cJSON_GetObjectItemCaseSensitive(widget_json, "clock_show_seconds");
    cJSON *clock_show_date = cJSON_GetObjectItemCaseSensitive(widget_json, "clock_show_date");
    cJSON *sensor_value_color = cJSON_GetObjectItemCaseSensitive(widget_json, "sensor_value_color");
    cJSON *tile_border_width = cJSON_GetObjectItemCaseSensitive(widget_json, "tile_border_width");
    cJSON *tile_radius = cJSON_GetObjectItemCaseSensitive(widget_json, "tile_radius");
    cJSON *tile_opacity = cJSON_GetObjectItemCaseSensitive(widget_json, "tile_opacity");
    cJSON *tile_shadow = cJSON_GetObjectItemCaseSensitive(widget_json, "tile_shadow");
    cJSON *rect = cJSON_GetObjectItemCaseSensitive(widget_json, "rect");
    if (!cJSON_IsString(id) || !cJSON_IsString(type) || !cJSON_IsObject(rect)) {
        return false;
    }

    const bool requires_entity = (strcmp(type->valuestring, "empty_tile") != 0) &&
                                 (strcmp(type->valuestring, "clock_alarm") != 0);
    if (requires_entity && !cJSON_IsString(entity_id)) {
        return false;
    }

    cJSON *x = cJSON_GetObjectItemCaseSensitive(rect, "x");
    cJSON *y = cJSON_GetObjectItemCaseSensitive(rect, "y");
    cJSON *w = cJSON_GetObjectItemCaseSensitive(rect, "w");
    cJSON *h = cJSON_GetObjectItemCaseSensitive(rect, "h");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(w) || !cJSON_IsNumber(h)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    snprintf(out->id, sizeof(out->id), "%s", id->valuestring);
    snprintf(out->type, sizeof(out->type), "%s", type->valuestring);
    snprintf(out->title, sizeof(out->title), "%s", cJSON_IsString(title) ? title->valuestring : id->valuestring);
    if (cJSON_IsString(entity_id) && entity_id->valuestring != NULL) {
        snprintf(out->entity_id, sizeof(out->entity_id), "%s", entity_id->valuestring);
    }
    if (cJSON_IsString(secondary_entity_id) && secondary_entity_id->valuestring != NULL) {
        snprintf(out->secondary_entity_id, sizeof(out->secondary_entity_id), "%s", secondary_entity_id->valuestring);
    }
    if (cJSON_IsString(slider_direction) && slider_direction->valuestring != NULL) {
        snprintf(out->slider_direction, sizeof(out->slider_direction), "%s", slider_direction->valuestring);
    }
    if (cJSON_IsString(slider_accent_color) && slider_accent_color->valuestring != NULL) {
        snprintf(out->slider_accent_color, sizeof(out->slider_accent_color), "%s", slider_accent_color->valuestring);
    }
    if (cJSON_IsString(button_accent_color) && button_accent_color->valuestring != NULL) {
        snprintf(out->button_accent_color, sizeof(out->button_accent_color), "%s", button_accent_color->valuestring);
    }
    if (cJSON_IsString(button_mode) && button_mode->valuestring != NULL) {
        snprintf(out->button_mode, sizeof(out->button_mode), "%s", button_mode->valuestring);
    }
    if (cJSON_IsString(graph_line_color) && graph_line_color->valuestring != NULL) {
        snprintf(out->graph_line_color, sizeof(out->graph_line_color), "%s", graph_line_color->valuestring);
    }
    if (cJSON_IsNumber(graph_point_count)) {
        out->graph_point_count = graph_point_count->valueint;
    }
    if (cJSON_IsNumber(graph_time_window_min)) {
        out->graph_time_window_min = graph_time_window_min->valueint;
    }
    if (cJSON_IsString(graph_display_mode) && graph_display_mode->valuestring != NULL) {
        snprintf(out->graph_display_mode, sizeof(out->graph_display_mode), "%s", graph_display_mode->valuestring);
    }
    if (cJSON_IsNumber(graph_bar_bucket_min)) {
        out->graph_bar_bucket_min = graph_bar_bucket_min->valueint;
    }
    if (cJSON_IsString(style_variant) && style_variant->valuestring != NULL) {
        snprintf(out->style_variant, sizeof(out->style_variant), "%s", style_variant->valuestring);
    }
    if (cJSON_IsString(arc_opening) && arc_opening->valuestring != NULL) {
        snprintf(out->arc_opening, sizeof(out->arc_opening), "%s", arc_opening->valuestring);
    }
    if (cJSON_IsString(binary_color_on) && binary_color_on->valuestring != NULL) {
        snprintf(out->binary_color_on, sizeof(out->binary_color_on), "%s", binary_color_on->valuestring);
    }
    if (cJSON_IsString(binary_color_off) && binary_color_off->valuestring != NULL) {
        snprintf(out->binary_color_off, sizeof(out->binary_color_off), "%s", binary_color_off->valuestring);
    }
    if (cJSON_IsString(binary_text_on) && binary_text_on->valuestring != NULL) {
        snprintf(out->binary_text_on, sizeof(out->binary_text_on), "%s", binary_text_on->valuestring);
    }
    if (cJSON_IsString(binary_text_off) && binary_text_off->valuestring != NULL) {
        snprintf(out->binary_text_off, sizeof(out->binary_text_off), "%s", binary_text_off->valuestring);
    }
    out->binary_show_title = true;
    if (cJSON_IsBool(binary_show_title)) {
        out->binary_show_title = cJSON_IsTrue(binary_show_title);
    }
    if (cJSON_IsString(alarm_code) && alarm_code->valuestring != NULL) {
        snprintf(out->alarm_code, sizeof(out->alarm_code), "%s", alarm_code->valuestring);
    }
    if (cJSON_IsString(alarm_modes) && alarm_modes->valuestring != NULL) {
        snprintf(out->alarm_modes, sizeof(out->alarm_modes), "%s", alarm_modes->valuestring);
    }
    if (cJSON_IsBool(alarm_ask_code)) {
        out->alarm_ask_code = cJSON_IsTrue(alarm_ask_code);
    }
    ui_runtime_copy_json_string(widget_json, "alarm_backend", out->alarm_backend, sizeof(out->alarm_backend));
    ui_runtime_copy_json_string(widget_json, "alarm_zone_label", out->alarm_zone_label,
        sizeof(out->alarm_zone_label));
    out->alarm_show_sensors = true;
    if (cJSON_IsBool(alarm_show_sensors)) {
        out->alarm_show_sensors = cJSON_IsTrue(alarm_show_sensors);
    }
    out->alarm_show_bypassed = true;
    if (cJSON_IsBool(alarm_show_bypassed)) {
        out->alarm_show_bypassed = cJSON_IsTrue(alarm_show_bypassed);
    }
    out->alarm_force_arm = true;
    if (cJSON_IsBool(alarm_force_arm)) {
        out->alarm_force_arm = cJSON_IsTrue(alarm_force_arm);
    }
    if (cJSON_IsBool(alarm_skip_delay)) {
        out->alarm_skip_delay = cJSON_IsTrue(alarm_skip_delay);
    }
    if (cJSON_IsBool(clock_show_seconds)) {
        out->clock_show_seconds = cJSON_IsTrue(clock_show_seconds);
    }
    if (cJSON_IsBool(clock_show_date)) {
        out->clock_show_date = cJSON_IsTrue(clock_show_date);
    }
    if (cJSON_IsString(sensor_value_color) && sensor_value_color->valuestring != NULL) {
        snprintf(out->sensor_value_color, sizeof(out->sensor_value_color), "%s", sensor_value_color->valuestring);
    }
    ui_runtime_copy_json_string(widget_json, "tile_bg_color", out->tile_bg_color, sizeof(out->tile_bg_color));
    ui_runtime_copy_json_string(widget_json, "tile_bg_grad_color", out->tile_bg_grad_color,
        sizeof(out->tile_bg_grad_color));
    ui_runtime_copy_json_string(widget_json, "tile_bg_grad_dir", out->tile_bg_grad_dir, sizeof(out->tile_bg_grad_dir));
    ui_runtime_copy_json_string(widget_json, "tile_border_color", out->tile_border_color,
        sizeof(out->tile_border_color));
    ui_runtime_copy_json_string(widget_json, "tile_text_color", out->tile_text_color, sizeof(out->tile_text_color));
    ui_runtime_copy_json_string(widget_json, "tile_title_color", out->tile_title_color,
        sizeof(out->tile_title_color));
    ui_runtime_copy_json_string(widget_json, "tile_label_color", out->tile_label_color,
        sizeof(out->tile_label_color));
    ui_runtime_copy_json_string(widget_json, "tile_value_color", out->tile_value_color,
        sizeof(out->tile_value_color));
    ui_runtime_copy_json_string(widget_json, "tile_icon_color", out->tile_icon_color, sizeof(out->tile_icon_color));
    ui_runtime_copy_json_string(widget_json, "tile_font_scale", out->tile_font_scale, sizeof(out->tile_font_scale));
    if (out->tile_bg_grad_dir[0] == '\0') {
        snprintf(out->tile_bg_grad_dir, sizeof(out->tile_bg_grad_dir), "%s", "none");
    }
    if (out->tile_font_scale[0] == '\0') {
        snprintf(out->tile_font_scale, sizeof(out->tile_font_scale), "%s", "auto");
    }
    if (out->alarm_backend[0] == '\0') {
        snprintf(out->alarm_backend, sizeof(out->alarm_backend), "%s", "auto");
    }
    out->tile_border_width = -1;
    if (cJSON_IsNumber(tile_border_width)) {
        out->tile_border_width = tile_border_width->valueint;
    }
    out->tile_radius = -1;
    if (cJSON_IsNumber(tile_radius)) {
        out->tile_radius = tile_radius->valueint;
    }
    out->tile_opacity = -1;
    if (cJSON_IsNumber(tile_opacity)) {
        out->tile_opacity = tile_opacity->valueint;
    }
    out->tile_shadow = false;
    if (cJSON_IsBool(tile_shadow)) {
        out->tile_shadow = cJSON_IsTrue(tile_shadow);
    }
    out->x = x->valueint;
    out->y = y->valueint;
    out->w = w->valueint;
    out->h = h->valueint;
    ui_runtime_clamp_widget_rect(out);
    return true;
}

static void ui_runtime_copy_json_string(cJSON *obj, const char *key, char *dst, size_t dst_size)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    cJSON *item = (obj != NULL) ? cJSON_GetObjectItemCaseSensitive(obj, key) : NULL;
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        snprintf(dst, dst_size, "%s", item->valuestring);
    }
}

static void ui_runtime_energy_config_from_json(cJSON *page_json, const char *page_id, const char *page_title,
    ui_energy_page_config_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    snprintf(out->page_id, sizeof(out->page_id), "%s", page_id != NULL ? page_id : "");
    snprintf(out->title, sizeof(out->title), "%s", (page_title != NULL && page_title[0] != '\0') ? page_title : "Energy");
    snprintf(out->source, sizeof(out->source), "%s", "ha_energy");

    cJSON *energy = (page_json != NULL) ? cJSON_GetObjectItemCaseSensitive(page_json, "energy") : NULL;
    if (!cJSON_IsObject(energy)) {
        return;
    }

    cJSON *source_item = cJSON_GetObjectItemCaseSensitive(energy, "source");
    bool source_was_configured =
        cJSON_IsString(source_item) && source_item->valuestring != NULL && source_item->valuestring[0] != '\0';
    ui_runtime_copy_json_string(energy, "source", out->source, sizeof(out->source));
    if (strcmp(out->source, "manual_live") != 0 && strcmp(out->source, "ha_energy") != 0) {
        snprintf(out->source, sizeof(out->source), "%s", "ha_energy");
    }

    ui_runtime_copy_json_string(
        energy, "home_power_entity_id", out->home_power_entity_id, sizeof(out->home_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "solar_power_entity_id", out->solar_power_entity_id, sizeof(out->solar_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "grid_power_entity_id", out->grid_power_entity_id, sizeof(out->grid_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "grid_import_power_entity_id", out->grid_import_power_entity_id, sizeof(out->grid_import_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "grid_export_power_entity_id", out->grid_export_power_entity_id, sizeof(out->grid_export_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "battery_power_entity_id", out->battery_power_entity_id, sizeof(out->battery_power_entity_id));
    ui_runtime_copy_json_string(energy,
        "battery_charge_power_entity_id",
        out->battery_charge_power_entity_id,
        sizeof(out->battery_charge_power_entity_id));
    ui_runtime_copy_json_string(energy,
        "battery_discharge_power_entity_id",
        out->battery_discharge_power_entity_id,
        sizeof(out->battery_discharge_power_entity_id));
    ui_runtime_copy_json_string(
        energy, "battery_soc_entity_id", out->battery_soc_entity_id, sizeof(out->battery_soc_entity_id));

    if (!source_was_configured &&
        (out->home_power_entity_id[0] != '\0' || out->solar_power_entity_id[0] != '\0' ||
            out->grid_power_entity_id[0] != '\0' || out->grid_import_power_entity_id[0] != '\0' ||
            out->grid_export_power_entity_id[0] != '\0' || out->battery_power_entity_id[0] != '\0' ||
            out->battery_charge_power_entity_id[0] != '\0' || out->battery_discharge_power_entity_id[0] != '\0' ||
            out->battery_soc_entity_id[0] != '\0')) {
        snprintf(out->source, sizeof(out->source), "%s", "manual_live");
    }
}

static void ui_runtime_music_config_from_json(cJSON *page_json, const char *page_id, const char *page_title,
    ui_music_page_config_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    snprintf(out->page_id, sizeof(out->page_id), "%s", page_id != NULL ? page_id : "");
    snprintf(out->title, sizeof(out->title), "%s",
        (page_title != NULL && page_title[0] != '\0') ? page_title : "Music");

    cJSON *music = (page_json != NULL) ? cJSON_GetObjectItemCaseSensitive(page_json, "music") : NULL;
    if (!cJSON_IsObject(music)) {
        return;
    }

    ui_runtime_copy_json_string(
        music, "player_entity_id", out->player_entity_id, sizeof(out->player_entity_id));

    cJSON *players = cJSON_GetObjectItemCaseSensitive(music, "players");
    if (cJSON_IsArray(players)) {
        int n = cJSON_GetArraySize(players);
        for (int i = 0; i < n && out->player_count < UI_MUSIC_MAX_PLAYERS; i++) {
            cJSON *item = cJSON_GetArrayItem(players, i);
            if (cJSON_IsString(item) && item->valuestring != NULL && item->valuestring[0] != '\0') {
                snprintf(out->players[out->player_count], APP_MAX_ENTITY_ID_LEN, "%s", item->valuestring);
                out->player_count++;
            }
        }
    }
}

static bool ui_runtime_is_background_widget_type(const char *type)
{
    return type != NULL && strcmp(type, "empty_tile") == 0;
}

/* Reads the optional per-page background ("page_*" keys). A page without any of
 * them gets an empty style, which leaves the page container untinted. */
static void ui_runtime_page_style_from_json(cJSON *page_json, ui_page_style_t *out)
{
    ui_page_style_init(out);
    if (out == NULL) {
        return;
    }
    ui_runtime_copy_json_string(page_json, "page_bg_color", out->bg_color, sizeof(out->bg_color));
    ui_runtime_copy_json_string(page_json, "page_bg_grad_color", out->bg_grad_color, sizeof(out->bg_grad_color));
    ui_runtime_copy_json_string(page_json, "page_bg_grad_dir", out->bg_grad_dir, sizeof(out->bg_grad_dir));
    if (out->bg_grad_dir[0] == '\0') {
        snprintf(out->bg_grad_dir, sizeof(out->bg_grad_dir), "%s", "none");
    }

    cJSON *wallpaper = cJSON_GetObjectItemCaseSensitive(page_json, "page_wallpaper");
    out->wallpaper = cJSON_IsBool(wallpaper) && cJSON_IsTrue(wallpaper);

    cJSON *dim = cJSON_GetObjectItemCaseSensitive(page_json, "page_dim");
    out->dim = cJSON_IsNumber(dim) ? dim->valueint : 0;
}

esp_err_t ui_runtime_load_layout(const char *layout_json)
{
    if (!display_lock(UI_LAYOUT_LOCK_TIMEOUT_MS)) {
        ESP_LOGW(TAG_UI, "Layout load: display lock busy");
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = ui_runtime_apply_layout_locked(layout_json);
    display_unlock();
    return err;
}

/* Builds the whole UI from layout_json. The display lock MUST already be held:
 * page/widget creation and deletion must not race with the LVGL render task. */
static esp_err_t ui_runtime_apply_layout_locked(const char *layout_json)
{
    if (!s_initialized || layout_json == NULL || s_widgets == NULL || s_energy_pages == NULL || s_music_pages == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *root = cJSON_Parse(layout_json);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *pages = cJSON_GetObjectItemCaseSensitive(root, "pages");
    if (!cJSON_IsArray(pages)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    s_topbar_cache.valid = false;
    ui_pages_reset();
    /* The page containers were just deleted: forget the wallpaper pages and the
     * per-page theme overrides, both are re-read below. */
    ui_page_style_reset();
    ui_theme_router_reset_pages();
    memset(s_widgets, 0, APP_MAX_WIDGETS_TOTAL * sizeof(*s_widgets));
    s_widget_count = 0;
    memset(s_energy_pages, 0, APP_MAX_PAGES * sizeof(*s_energy_pages));
    s_energy_page_count = 0;
    memset(s_music_pages, 0, APP_MAX_PAGES * sizeof(*s_music_pages));
    s_music_page_count = 0;

    int page_count = cJSON_GetArraySize(pages);
    for (int p = 0; p < page_count; p++) {
        cJSON *page = cJSON_GetArrayItem(pages, p);
        cJSON *page_id = cJSON_GetObjectItemCaseSensitive(page, "id");
        cJSON *page_title = cJSON_GetObjectItemCaseSensitive(page, "title");
        cJSON *page_type = cJSON_GetObjectItemCaseSensitive(page, "type");
        cJSON *widgets = cJSON_GetObjectItemCaseSensitive(page, "widgets");
        if (!cJSON_IsString(page_id)) {
            continue;
        }

        lv_obj_t *page_container = ui_pages_add(
            page_id->valuestring, cJSON_IsString(page_title) ? page_title->valuestring : page_id->valuestring);
        if (page_container == NULL) {
            continue;
        }

        /* Background colour / gradient / wallpaper for this page. */
        ui_page_style_t page_style;
        ui_runtime_page_style_from_json(page, &page_style);
        ui_page_style_apply(page_container, &page_style);

        /* Optional per-page theme override: the page is painted with it as soon
         * as it becomes visible (see ui_theme_router). */
        char page_theme[APP_MAX_THEME_ID_LEN];
        ui_runtime_copy_json_string(page, "page_theme", page_theme, sizeof(page_theme));
        ui_theme_router_set_page_theme(page_id->valuestring, page_theme);

        bool is_energy_page = cJSON_IsString(page_type) && page_type->valuestring != NULL &&
                              strcmp(page_type->valuestring, "energy_dashboard") == 0;
        if (is_energy_page) {
            if (s_energy_page_count >= APP_MAX_PAGES) {
                continue;
            }
            ui_energy_page_config_t energy_config = {0};
            ui_runtime_energy_config_from_json(page,
                page_id->valuestring,
                cJSON_IsString(page_title) ? page_title->valuestring : page_id->valuestring,
                &energy_config);
            esp_err_t err =
                ui_energy_page_create(&energy_config, page_container, &s_energy_pages[s_energy_page_count]);
            if (err == ESP_OK) {
                s_energy_page_count++;
            }
            continue;
        }

        bool is_music_page = cJSON_IsString(page_type) && page_type->valuestring != NULL &&
                             strcmp(page_type->valuestring, "music_assistant") == 0;
        if (is_music_page) {
            if (s_music_page_count >= APP_MAX_PAGES) {
                continue;
            }
            ui_music_page_config_t music_config = {0};
            ui_runtime_music_config_from_json(page,
                page_id->valuestring,
                cJSON_IsString(page_title) ? page_title->valuestring : page_id->valuestring,
                &music_config);
            esp_err_t err =
                ui_music_page_create(&music_config, page_container, &s_music_pages[s_music_page_count]);
            if (err == ESP_OK) {
                s_music_page_count++;
            }
            continue;
        }

        if (!cJSON_IsArray(widgets)) {
            continue;
        }

        int widget_count = cJSON_GetArraySize(widgets);
        for (int pass = 0; pass < 2; pass++) {
            const bool background_pass = (pass == 0);
            for (int w = 0; w < widget_count; w++) {
                if (s_widget_count >= APP_MAX_WIDGETS_TOTAL) {
                    break;
                }
                ui_widget_def_t def = {0};
                if (!ui_runtime_widget_from_json(cJSON_GetArrayItem(widgets, w), &def)) {
                    continue;
                }
                bool is_background = ui_runtime_is_background_widget_type(def.type);
                if (is_background != background_pass) {
                    continue;
                }
                esp_err_t err = ui_widget_factory_create(&def, page_container, &s_widgets[s_widget_count]);
                if (err == ESP_OK) {
                    snprintf(s_widgets[s_widget_count].page_id, sizeof(s_widgets[s_widget_count].page_id),
                             "%s", page_id->valuestring);
                    s_widget_count++;
                }
            }
        }
    }

    cJSON_Delete(root);

    if (ui_pages_count() > 0) {
        size_t index = 0;
        if (s_restore_page_id_valid) {
            s_restore_page_id_valid = false;
            uint16_t count = ui_pages_count();
            bool found = false;
            for (uint16_t i = 0; i < count; i++) {
                const char *id = ui_pages_id_at(i);
                if (id != NULL && strcmp(id, s_restore_page_id) == 0) {
                    index = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                ESP_LOGW(TAG_UI, "layout rebuild: page %s is gone, showing the first one", s_restore_page_id);
            }
            s_restore_page_id[0] = '\0';
        }
        ui_pages_show_index(index);
    }
    ui_runtime_apply_all_states();
    ui_runtime_refresh_topbar();
    ESP_LOGI(TAG_UI, "Layout loaded: %u widgets", (unsigned)s_widget_count);
    return ESP_OK;
}

esp_err_t ui_runtime_reload_layout(void)
{
    /* Reads NVS without the display lock: the file system/flash access can take
     * a while and holding the lock during it would freeze the render task. */
    char *json = NULL;
    esp_err_t err = layout_store_load(&json);
    if (err != ESP_OK || json == NULL) {
        json = strdup(layout_store_default_json());
        if (json == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    /* The whole rebuild (theme styles + page/widget recreation) has to run under
     * the display lock: theme_default_rebuild_styles() resets LVGL styles that
     * are attached to live objects (widgets, pages), and the page teardown
     * deletes objects the render task may be reading. */
    if (!display_lock(UI_LAYOUT_LOCK_TIMEOUT_MS)) {
        free(json);
        s_layout_reload_pending = true;
        ESP_LOGW(TAG_UI, "Layout reload: display lock busy, retrying");
        return ESP_ERR_TIMEOUT;
    }

    int64_t t0 = esp_timer_get_time() / 1000;
    theme_default_rebuild_styles();
    int64_t t1 = esp_timer_get_time() / 1000;
    ui_runtime_kick_heartbeat();
    err = ui_runtime_apply_layout_locked(json);
    int64_t t2 = esp_timer_get_time() / 1000;
    display_unlock();
    free(json);

    s_layout_reload_pending = false;
    system_log_write_info(TAG_UI, "layout reload ok=%d (theme %lld ms, build %lld ms)",
                          (int)(err == ESP_OK), (long long)(t1 - t0), (long long)(t2 - t1));
    return err;
}

esp_err_t ui_runtime_request_layout_reload(void)
{
    app_event_t event = {.type = EV_LAYOUT_UPDATED};
    if (app_events_publish(&event, pdMS_TO_TICKS(UI_LAYOUT_PUBLISH_TIMEOUT_MS))) {
        /* The UI task applies it asynchronously. */
        system_log_write_info(TAG_UI, "layout reload queued (async)");
        return ESP_OK;
    }

    /* The event queue is saturated (UI task busy or not draining). Applying the
     * layout here keeps the caller honest: it reports the real outcome instead
     * of silently leaving the old look on screen. */
    ESP_LOGW(TAG_UI, "Layout reload event queue busy, applying synchronously");
    esp_err_t err = ui_runtime_reload_layout();
    if (err == ESP_ERR_TIMEOUT) {
        /* Display lock contention: the UI task loop retries until it succeeds. */
        return ESP_OK;
    }
    system_log_write_info(TAG_UI, "layout reload applied synchronously result=%s", esp_err_to_name(err));
    return err;
}

esp_err_t ui_runtime_request_layout_reload_on_page(const char *page_id)
{
    if (page_id != NULL && page_id[0] != '\0') {
        snprintf(s_restore_page_id, sizeof(s_restore_page_id), "%s", page_id);
        s_restore_page_id_valid = true;
    }

    /* Always asynchronous: the caller may be a page show callback, and
     * rebuilding there would delete the pages under the caller's feet. */
    app_event_t event = {.type = EV_LAYOUT_UPDATED};
    if (app_events_publish(&event, pdMS_TO_TICKS(UI_LAYOUT_PUBLISH_TIMEOUT_MS))) {
        system_log_write_info(TAG_UI, "layout reload queued (page %s)",
                              s_restore_page_id_valid ? s_restore_page_id : "-");
        return ESP_OK;
    }
    /* The UI task retries pending reloads on its own schedule. */
    s_layout_reload_pending = true;
    system_log_write_info(TAG_UI, "layout reload deferred (event queue busy)");
    return ESP_OK;
}

static void ui_runtime_handle_event(const app_event_t *event)
{
    if (event == NULL) {
        return;
    }

    bool needs_lock = (event->type != EV_LAYOUT_UPDATED);
    if (needs_lock && !display_lock(0)) {
        if (event->type == EV_HA_STATE_CHANGED || event->type == EV_HA_CONNECTED ||
            event->type == EV_HA_ENERGY_CHANGED) {
            s_pending_state_reconcile = true;
            s_pending_topbar_refresh = true;
        } else if (event->type == EV_HA_DISCONNECTED) {
            s_pending_topbar_refresh = true;
        }

        s_deferred_event_count++;
        int64_t now_ms = esp_timer_get_time() / 1000;
        if ((now_ms - s_deferred_event_log_ms) >= 5000) {
            ESP_LOGW(TAG_UI, "Deferred UI event processing due to display lock contention (deferred=%u)",
                (unsigned)s_deferred_event_count);
            s_deferred_event_log_ms = now_ms;
            s_deferred_event_count = 0;
        }
        return;
    }

    switch (event->type) {
    case EV_HA_STATE_CHANGED:
        ui_runtime_apply_entity_state(event->data.ha_state_changed.entity_id);
#if APP_HA_ROUTE_TRACE_LOG
        ESP_LOGI(TAG_UI, "route panel->ui entity=%s", event->data.ha_state_changed.entity_id);
#endif
        break;
    case EV_HA_CONNECTED:
        ui_runtime_refresh_topbar();
        /* During initial/partial HA sync we may temporarily miss some entities.
         * Preserve currently rendered widgets instead of forcing unavailable. */
        ui_runtime_apply_all_states_preserve_missing();
        break;
    case EV_HA_ENERGY_CHANGED:
        for (size_t i = 0; i < s_energy_page_count; i++) {
            ui_energy_page_apply_all_states(&s_energy_pages[i]);
        }
        break;
    case EV_HA_DISCONNECTED:
        ui_runtime_refresh_topbar();
        break;
    case EV_LAYOUT_UPDATED:
        ui_runtime_reload_layout();
        break;
    case EV_UI_NAVIGATE:
        ui_pages_show(event->data.navigate.page_id);
        break;
    case EV_NONE:
    default:
        break;
    }

    if (needs_lock) {
        display_unlock();
    }
}

static void ui_runtime_task(void *arg)
{
    (void)arg;
    while (true) {
        s_heartbeat++;
        app_event_t event = {0};
        while (app_events_receive(&event, 0)) {
            ui_runtime_handle_event(&event);
        }

        int64_t now_ms = esp_timer_get_time() / 1000;

        /* Per-page theme overrides and the automatic day/night theme switch.
         * Runs on this task, so it may touch the palette and the layout. */
        ui_theme_router_tick();

        if (s_layout_reload_pending && (now_ms - s_layout_reload_retry_ms) >= UI_LAYOUT_RETRY_INTERVAL_MS) {
            s_layout_reload_retry_ms = now_ms;
            (void)ui_runtime_reload_layout();
        }

        uint32_t model_revision = ha_model_state_revision();
        if (model_revision != s_last_model_revision) {
            s_last_model_revision = model_revision;
            s_model_reconcile_pending = true;
        }

        if ((s_pending_state_reconcile || s_pending_topbar_refresh) && display_lock(20)) {
            if (s_pending_topbar_refresh) {
                ui_runtime_refresh_topbar();
                s_pending_topbar_refresh = false;
            }
            if (s_pending_state_reconcile) {
                /* Reconcile all states after lock contention so UI never stays stale. */
                ui_runtime_apply_all_states_preserve_missing();
                s_pending_state_reconcile = false;
            }
            display_unlock();
        }

        if (s_model_reconcile_pending && (now_ms - s_last_model_reconcile_ms) >= UI_MODEL_RECONCILE_INTERVAL_MS &&
            display_lock(20)) {
            /* Protect against missed per-entity events under burst: periodically reconcile from model snapshot. */
            ui_runtime_apply_all_states_preserve_missing();
            display_unlock();
            s_model_reconcile_pending = false;
            s_last_model_reconcile_ms = now_ms;
        }

        if ((now_ms - s_last_topbar_refresh_ms) >= 1000) {
            if (display_lock(20)) {
                ui_runtime_refresh_topbar();
                display_unlock();
                s_last_topbar_refresh_ms = now_ms;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t ui_runtime_init(void)
{
    esp_err_t err = ui_runtime_alloc_buffers();
    if (err != ESP_OK) {
        return err;
    }

    if (!display_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    s_topbar_cache.valid = false;
    theme_default_init();
    ui_pages_init();
    ui_screen_saver_init();
    ui_page_transition_init();
    ui_press_feedback_init();
    ui_value_anim_init();
    ui_pages_set_show_callback(ui_runtime_on_page_shown);
    ui_runtime_show_weather_icon_overlay();
    ui_runtime_refresh_topbar();
    display_unlock();
    s_last_topbar_refresh_ms = esp_timer_get_time() / 1000;
    s_last_model_reconcile_ms = s_last_topbar_refresh_ms;
    s_last_model_revision = ha_model_state_revision();
    s_model_reconcile_pending = false;
    s_pending_state_reconcile = false;
    s_pending_topbar_refresh = false;
    s_deferred_event_count = 0;
    s_deferred_event_log_ms = 0;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t ui_runtime_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ui_task != NULL) {
        return ESP_OK;
    }

    BaseType_t created =
        app_task_create(ui_runtime_task, "ui_runtime", APP_UI_TASK_STACK, NULL, APP_UI_TASK_PRIO, &s_ui_task);
    return (created == pdPASS) ? ESP_OK : ESP_FAIL;
}

bool ui_runtime_is_running(void)
{
    return s_ui_task != NULL;
}

uint32_t ui_runtime_get_heartbeat(void)
{
    return s_heartbeat;
}

void ui_runtime_kick_heartbeat(void)
{
    /* Long but healthy rebuilds (large layouts, slow PSRAM) must not trip the
     * UI watchdog: report progress from phase boundaries instead. */
    s_heartbeat++;
}
