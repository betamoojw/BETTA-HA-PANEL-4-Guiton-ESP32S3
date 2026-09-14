/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "app_config.h"

typedef struct {
    char wifi_ssid[APP_WIFI_SSID_MAX_LEN];
    char wifi_password[APP_WIFI_PASSWORD_MAX_LEN];
    char wifi_country_code[APP_WIFI_COUNTRY_CODE_MAX_LEN];
    char wifi_bssid[APP_WIFI_BSSID_MAX_LEN];
    bool wifi_static_enabled;
    char wifi_static_ip[APP_WIFI_IPV4_MAX_LEN];
    char wifi_static_netmask[APP_WIFI_IPV4_MAX_LEN];
    char wifi_static_gateway[APP_WIFI_IPV4_MAX_LEN];
    char wifi_static_dns[APP_WIFI_IPV4_MAX_LEN];
    char ha_ws_url[APP_HA_WS_URL_MAX_LEN];
    char ha_access_token[APP_HA_ACCESS_TOKEN_MAX_LEN];
    bool ha_rest_enabled;
    char ntp_server[APP_NTP_SERVER_MAX_LEN];
    char time_tz[APP_TIME_TZ_MAX_LEN];
    char ui_language[APP_UI_LANGUAGE_MAX_LEN];
    uint8_t display_brightness;
    bool display_screensaver_enabled;
    uint32_t display_screensaver_timeout_sec;
    bool display_screen_off_enabled;
    uint32_t display_screen_off_timeout_sec;
    bool display_clock_24h;
    bool display_saver_show_seconds;
    bool display_saver_show_date;
    uint32_t display_saver_clock_color;
    uint32_t display_saver_date_color;
    bool mqtt_enabled;
    char mqtt_host[APP_MQTT_HOST_MAX_LEN];
    uint16_t mqtt_port;
    char mqtt_username[APP_MQTT_USERNAME_MAX_LEN];
    char mqtt_password[APP_MQTT_PASSWORD_MAX_LEN];
    char mqtt_discovery_prefix[APP_MQTT_DISCOVERY_PREFIX_MAX_LEN];
    bool system_auto_restart_enabled;
    uint32_t system_auto_restart_hours;
} runtime_settings_t;

void runtime_settings_set_defaults(runtime_settings_t *out);
esp_err_t runtime_settings_init(void);
esp_err_t runtime_settings_load(runtime_settings_t *out);
esp_err_t runtime_settings_save(const runtime_settings_t *settings);
bool runtime_settings_has_wifi(const runtime_settings_t *settings);
bool runtime_settings_has_ha(const runtime_settings_t *settings);
