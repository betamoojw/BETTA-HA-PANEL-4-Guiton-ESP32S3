/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "api/api_routes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "app_config.h"
#include "bsp/display.h"
#include "drivers/display_init.h"
#include "ha/ha_client.h"
#include "mqtt/panel_mqtt.h"
#include "net/wifi_mgr.h"
#include "settings/i18n_store.h"
#include "settings/runtime_settings.h"
#include "ui/ui_screen_saver.h"

static esp_timer_handle_t s_restart_timer = NULL;

static void set_json_headers(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static esp_err_t send_json_error(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "error", (message != NULL) ? message : "Invalid request");
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    if (status != NULL) {
        httpd_resp_set_status(req, status);
    }
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}

static bool has_ws_scheme(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return true;
    }
    return strncmp(url, "ws://", 5) == 0 || strncmp(url, "wss://", 6) == 0;
}

static bool validate_ipv4(const char *ip)
{
    if (ip == NULL || ip[0] == '\0') {
        return false;
    }
    struct in_addr addr4;
    return inet_pton(AF_INET, ip, &addr4) == 1;
}

static bool normalize_country_code(char *country_code, size_t country_code_len)
{
    if (country_code == NULL || country_code_len < APP_WIFI_COUNTRY_CODE_MAX_LEN) {
        return false;
    }
    if (country_code[0] == '\0') {
        strlcpy(country_code, APP_WIFI_COUNTRY_CODE, country_code_len);
        return true;
    }
    if (strlen(country_code) != 2) {
        return false;
    }
    if (!isalpha((unsigned char)country_code[0]) || !isalpha((unsigned char)country_code[1])) {
        return false;
    }

    country_code[0] = (char)toupper((unsigned char)country_code[0]);
    country_code[1] = (char)toupper((unsigned char)country_code[1]);
    country_code[2] = '\0';
    return true;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool normalize_bssid(char *bssid, size_t bssid_len)
{
    if (bssid == NULL || bssid_len < APP_WIFI_BSSID_MAX_LEN) {
        return false;
    }
    if (bssid[0] == '\0') {
        return true;
    }
    if (strlen(bssid) != 17U) {
        return false;
    }

    char sep = bssid[2];
    if (sep != ':' && sep != '-') {
        return false;
    }

    for (size_t i = 0; i < 6; i++) {
        size_t idx = i * 3;
        int hi = hex_nibble(bssid[idx]);
        int lo = hex_nibble(bssid[idx + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        bssid[idx] = (char)toupper((unsigned char)bssid[idx]);
        bssid[idx + 1] = (char)toupper((unsigned char)bssid[idx + 1]);
        if (i < 5) {
            if (bssid[idx + 2] != sep) {
                return false;
            }
            bssid[idx + 2] = ':';
        }
    }
    bssid[17] = '\0';
    return true;
}

static bool normalize_ui_language(char *language, size_t language_len)
{
    if (language == NULL || language_len == 0) {
        return false;
    }
    if (language[0] == '\0') {
        strlcpy(language, APP_UI_DEFAULT_LANGUAGE, language_len);
        return true;
    }

    char normalized[APP_UI_LANGUAGE_MAX_LEN] = {0};
    if (!i18n_store_normalize_language_code(language, normalized, sizeof(normalized))) {
        return false;
    }
    strlcpy(language, normalized, language_len);
    return true;
}

static void restart_timer_cb(void *arg)
{
    (void)arg;
    /* Avoid random panel colors during software reset. */
    (void)bsp_display_backlight_off();
    esp_restart();
}

static void schedule_restart(void)
{
    if (s_restart_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &restart_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "settings_restart",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&timer_args, &s_restart_timer) != ESP_OK) {
            esp_restart();
            return;
        }
    }

    if (esp_timer_is_active(s_restart_timer)) {
        (void)esp_timer_stop(s_restart_timer);
    }
    if (esp_timer_start_once(s_restart_timer, 1500ULL * 1000ULL) != ESP_OK) {
        esp_restart();
    }
}

esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    runtime_settings_t *settings = calloc(1, sizeof(runtime_settings_t));
    if (settings == NULL) {
        return httpd_resp_send_500(req);
    }

    esp_err_t settings_err = runtime_settings_load(settings);
    if (settings_err != ESP_OK) {
        runtime_settings_set_defaults(settings);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *ha = cJSON_CreateObject();
    cJSON *time_cfg = cJSON_CreateObject();
    cJSON *ui = cJSON_CreateObject();
    cJSON *display = cJSON_CreateObject();
    cJSON *mqtt = cJSON_CreateObject();
    cJSON *system = cJSON_CreateObject();
    if (root == NULL || wifi == NULL || ha == NULL || time_cfg == NULL || ui == NULL || display == NULL || mqtt == NULL ||
        system == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(wifi);
        cJSON_Delete(ha);
        cJSON_Delete(time_cfg);
        cJSON_Delete(ui);
        cJSON_Delete(display);
        cJSON_Delete(mqtt);
        cJSON_Delete(system);
        free(settings);
        return httpd_resp_send_500(req);
    }

    cJSON_AddStringToObject(wifi, "ssid", settings->wifi_ssid);
    cJSON_AddStringToObject(wifi, "country_code", settings->wifi_country_code);
    cJSON_AddStringToObject(wifi, "bssid", settings->wifi_bssid);
    cJSON_AddBoolToObject(wifi, "static_enabled", settings->wifi_static_enabled);
    cJSON_AddStringToObject(wifi, "static_ip", settings->wifi_static_ip);
    cJSON_AddStringToObject(wifi, "static_netmask", settings->wifi_static_netmask);
    cJSON_AddStringToObject(wifi, "static_gateway", settings->wifi_static_gateway);
    cJSON_AddStringToObject(wifi, "static_dns", settings->wifi_static_dns);
    cJSON_AddBoolToObject(wifi, "password_set", settings->wifi_password[0] != '\0');
    cJSON_AddBoolToObject(wifi, "configured", runtime_settings_has_wifi(settings));
    cJSON_AddBoolToObject(wifi, "connected", wifi_mgr_is_connected());
    cJSON_AddBoolToObject(wifi, "setup_ap_active", wifi_mgr_is_setup_ap_active());
    cJSON_AddStringToObject(wifi, "setup_ap_ssid", wifi_mgr_get_setup_ap_ssid());
    wifi_mgr_sta_ap_info_t sta_ap = {0};
    if (wifi_mgr_get_sta_ap_info(&sta_ap) == ESP_OK) {
        char connected_bssid[APP_WIFI_BSSID_MAX_LEN] = {0};
        snprintf(
            connected_bssid,
            sizeof(connected_bssid),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            sta_ap.bssid[0],
            sta_ap.bssid[1],
            sta_ap.bssid[2],
            sta_ap.bssid[3],
            sta_ap.bssid[4],
            sta_ap.bssid[5]);
        cJSON_AddNumberToObject(wifi, "rssi_dbm", (double)sta_ap.rssi);
        cJSON_AddStringToObject(wifi, "connected_bssid", connected_bssid);
        cJSON_AddNumberToObject(wifi, "connected_channel", (double)sta_ap.channel);
    } else {
        cJSON_AddNullToObject(wifi, "rssi_dbm");
        cJSON_AddNullToObject(wifi, "connected_bssid");
        cJSON_AddNullToObject(wifi, "connected_channel");
    }
    cJSON_AddBoolToObject(wifi, "scan_supported", true);
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON_AddStringToObject(ha, "ws_url", settings->ha_ws_url);
    cJSON_AddBoolToObject(ha, "access_token_set", settings->ha_access_token[0] != '\0');
    cJSON_AddBoolToObject(ha, "rest_enabled", settings->ha_rest_enabled);
    cJSON_AddBoolToObject(ha, "configured", runtime_settings_has_ha(settings));
    cJSON_AddBoolToObject(ha, "connected", ha_client_is_connected());
    cJSON_AddItemToObject(root, "ha", ha);

    cJSON_AddStringToObject(time_cfg, "ntp_server", settings->ntp_server);
    cJSON_AddStringToObject(time_cfg, "timezone", settings->time_tz);
    cJSON_AddItemToObject(root, "time", time_cfg);

    cJSON_AddStringToObject(ui, "language", settings->ui_language);
    cJSON_AddItemToObject(root, "ui", ui);

    cJSON_AddNumberToObject(display, "brightness", settings->display_brightness);
    cJSON_AddBoolToObject(display, "screensaver_enabled", settings->display_screensaver_enabled);
    cJSON_AddNumberToObject(display, "screensaver_timeout_sec", (double)settings->display_screensaver_timeout_sec);
    cJSON_AddBoolToObject(display, "screen_off_enabled", settings->display_screen_off_enabled);
    cJSON_AddNumberToObject(display, "screen_off_timeout_sec", (double)settings->display_screen_off_timeout_sec);
    cJSON_AddBoolToObject(display, "clock_24h", settings->display_clock_24h);
    cJSON_AddBoolToObject(display, "saver_show_seconds", settings->display_saver_show_seconds);
    cJSON_AddBoolToObject(display, "saver_show_date", settings->display_saver_show_date);
    cJSON_AddNumberToObject(display, "saver_clock_color", (double)(settings->display_saver_clock_color & 0xFFFFFF));
    cJSON_AddNumberToObject(display, "saver_date_color", (double)(settings->display_saver_date_color & 0xFFFFFF));
    cJSON_AddItemToObject(root, "display", display);

    cJSON_AddBoolToObject(mqtt, "enabled", settings->mqtt_enabled);
    cJSON_AddStringToObject(mqtt, "host", settings->mqtt_host);
    cJSON_AddNumberToObject(mqtt, "port", (double)settings->mqtt_port);
    cJSON_AddStringToObject(mqtt, "username", settings->mqtt_username);
    cJSON_AddBoolToObject(mqtt, "password_set", settings->mqtt_password[0] != '\0');
    cJSON_AddStringToObject(mqtt, "discovery_prefix", settings->mqtt_discovery_prefix);
    cJSON_AddBoolToObject(mqtt, "connected", panel_mqtt_is_connected());
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON_AddBoolToObject(system, "auto_restart_enabled", settings->system_auto_restart_enabled);
    cJSON_AddNumberToObject(system, "auto_restart_hours", (double)settings->system_auto_restart_hours);
    cJSON_AddItemToObject(root, "system", system);

    cJSON_AddBoolToObject(root, "ok", true);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(settings);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}

static bool update_string_setting(
    cJSON *obj,
    const char *key,
    char *dst,
    size_t dst_len,
    bool *out_invalid_type,
    bool *out_too_long)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        return false;
    }

    if (cJSON_IsString(item) && item->valuestring != NULL) {
        if (strlen(item->valuestring) >= dst_len) {
            if (out_too_long != NULL) {
                *out_too_long = true;
            }
            return false;
        }
        strlcpy(dst, item->valuestring, dst_len);
        return true;
    }
    if (cJSON_IsNull(item)) {
        dst[0] = '\0';
        return true;
    }

    if (out_invalid_type != NULL) {
        *out_invalid_type = true;
    }
    return false;
}

static bool update_bool_setting(cJSON *obj, const char *key, bool *dst, bool *out_invalid_type)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        return false;
    }

    if (cJSON_IsBool(item)) {
        *dst = cJSON_IsTrue(item);
        return true;
    }

    if (out_invalid_type != NULL) {
        *out_invalid_type = true;
    }
    return false;
}

static bool update_color_setting(cJSON *obj, const char *key, uint32_t *dst, bool *out_invalid_type)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        return false;
    }

    if (cJSON_IsNumber(item)) {
        uint32_t value = (uint32_t)item->valuedouble;
        if (value > 0xFFFFFF) {
            value = 0xFFFFFF;
        }
        *dst = value;
        return true;
    }

    if (out_invalid_type != NULL) {
        *out_invalid_type = true;
    }
    return false;
}

esp_err_t api_settings_put_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > APP_SETTINGS_MAX_JSON_LEN) {
        return send_json_error(req, "400 Bad Request", "Invalid payload size");
    }

    char *buf = calloc((size_t)req->content_len + 1U, sizeof(char));
    if (buf == NULL) {
        return httpd_resp_send_500(req);
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, buf + received, req->content_len - received);
        if (r <= 0) {
            free(buf);
            return send_json_error(req, "400 Bad Request", "Failed to read request body");
        }
        received += r;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "Invalid JSON");
    }

    runtime_settings_t *settings = calloc(1, sizeof(runtime_settings_t));
    if (settings == NULL) {
        cJSON_Delete(root);
        return httpd_resp_send_500(req);
    }

    esp_err_t load_err = runtime_settings_load(settings);
    if (load_err != ESP_OK) {
        runtime_settings_set_defaults(settings);
    }

    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    cJSON *ha = cJSON_GetObjectItemCaseSensitive(root, "ha");
    cJSON *time_cfg = cJSON_GetObjectItemCaseSensitive(root, "time");
    cJSON *ui = cJSON_GetObjectItemCaseSensitive(root, "ui");
    cJSON *display = cJSON_GetObjectItemCaseSensitive(root, "display");
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
    cJSON *system = cJSON_GetObjectItemCaseSensitive(root, "system");
    if (wifi != NULL && !cJSON_IsObject(wifi)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "wifi must be an object");
    }
    if (ha != NULL && !cJSON_IsObject(ha)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "ha must be an object");
    }
    if (time_cfg != NULL && !cJSON_IsObject(time_cfg)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "time must be an object");
    }
    if (ui != NULL && !cJSON_IsObject(ui)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "ui must be an object");
    }
    if (display != NULL && !cJSON_IsObject(display)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "display must be an object");
    }
    if (mqtt != NULL && !cJSON_IsObject(mqtt)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "mqtt must be an object");
    }
    if (system != NULL && !cJSON_IsObject(system)) {
        cJSON_Delete(root);
        free(settings);
        return send_json_error(req, "400 Bad Request", "system must be an object");
    }

    bool invalid_type = false;
    bool too_long = false;
    if (cJSON_IsObject(wifi)) {
        (void)update_string_setting(
            wifi, "ssid", settings->wifi_ssid, sizeof(settings->wifi_ssid), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "password", settings->wifi_password, sizeof(settings->wifi_password), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "country_code", settings->wifi_country_code, sizeof(settings->wifi_country_code), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "bssid", settings->wifi_bssid, sizeof(settings->wifi_bssid), &invalid_type, &too_long);
        (void)update_bool_setting(wifi, "static_enabled", &settings->wifi_static_enabled, &invalid_type);
        (void)update_string_setting(
            wifi, "static_ip", settings->wifi_static_ip, sizeof(settings->wifi_static_ip), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "static_netmask", settings->wifi_static_netmask, sizeof(settings->wifi_static_netmask), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "static_gateway", settings->wifi_static_gateway, sizeof(settings->wifi_static_gateway), &invalid_type, &too_long);
        (void)update_string_setting(
            wifi, "static_dns", settings->wifi_static_dns, sizeof(settings->wifi_static_dns), &invalid_type, &too_long);
    }
    if (cJSON_IsObject(ha)) {
        (void)update_string_setting(
            ha, "ws_url", settings->ha_ws_url, sizeof(settings->ha_ws_url), &invalid_type, &too_long);
        (void)update_string_setting(
            ha, "access_token", settings->ha_access_token, sizeof(settings->ha_access_token), &invalid_type, &too_long);
        (void)update_bool_setting(ha, "rest_enabled", &settings->ha_rest_enabled, &invalid_type);
    }
    if (cJSON_IsObject(time_cfg)) {
        (void)update_string_setting(
            time_cfg, "ntp_server", settings->ntp_server, sizeof(settings->ntp_server), &invalid_type, &too_long);
        (void)update_string_setting(
            time_cfg, "timezone", settings->time_tz, sizeof(settings->time_tz), &invalid_type, &too_long);
    }
    if (cJSON_IsObject(ui)) {
        (void)update_string_setting(
            ui, "language", settings->ui_language, sizeof(settings->ui_language), &invalid_type, &too_long);
    }
    if (cJSON_IsObject(display)) {
        cJSON *brightness = cJSON_GetObjectItemCaseSensitive(display, "brightness");
        if (brightness != NULL) {
            if (cJSON_IsNumber(brightness)) {
                int value = (int)brightness->valuedouble;
                if (value < 1) {
                    value = 1;
                } else if (value > 100) {
                    value = 100;
                }
                settings->display_brightness = (uint8_t)value;
            } else {
                invalid_type = true;
            }
        }

        cJSON *saver_timeout = cJSON_GetObjectItemCaseSensitive(display, "screensaver_timeout_sec");
        if (saver_timeout != NULL) {
            if (cJSON_IsNumber(saver_timeout)) {
                uint32_t value = (uint32_t)saver_timeout->valuedouble;
                if (value < 5) {
                    value = 5;
                } else if (value > 3600) {
                    value = 3600;
                }
                settings->display_screensaver_timeout_sec = value;
            } else {
                invalid_type = true;
            }
        }

        cJSON *off_timeout = cJSON_GetObjectItemCaseSensitive(display, "screen_off_timeout_sec");
        if (off_timeout != NULL) {
            if (cJSON_IsNumber(off_timeout)) {
                uint32_t value = (uint32_t)off_timeout->valuedouble;
                if (value < 5) {
                    value = 5;
                } else if (value > 7200) {
                    value = 7200;
                }
                settings->display_screen_off_timeout_sec = value;
            } else {
                invalid_type = true;
            }
        }

        (void)update_bool_setting(display, "screensaver_enabled", &settings->display_screensaver_enabled, &invalid_type);
        (void)update_bool_setting(display, "screen_off_enabled", &settings->display_screen_off_enabled, &invalid_type);
        (void)update_bool_setting(display, "clock_24h", &settings->display_clock_24h, &invalid_type);
        (void)update_bool_setting(display, "saver_show_seconds", &settings->display_saver_show_seconds, &invalid_type);
        (void)update_bool_setting(display, "saver_show_date", &settings->display_saver_show_date, &invalid_type);
        (void)update_color_setting(display, "saver_clock_color", &settings->display_saver_clock_color, &invalid_type);
        (void)update_color_setting(display, "saver_date_color", &settings->display_saver_date_color, &invalid_type);
    }
    if (cJSON_IsObject(mqtt)) {
        (void)update_bool_setting(mqtt, "enabled", &settings->mqtt_enabled, &invalid_type);
        (void)update_string_setting(
            mqtt, "host", settings->mqtt_host, sizeof(settings->mqtt_host), &invalid_type, &too_long);
        (void)update_string_setting(
            mqtt, "username", settings->mqtt_username, sizeof(settings->mqtt_username), &invalid_type, &too_long);
        (void)update_string_setting(
            mqtt,
            "discovery_prefix",
            settings->mqtt_discovery_prefix,
            sizeof(settings->mqtt_discovery_prefix),
            &invalid_type,
            &too_long);

        cJSON *port = cJSON_GetObjectItemCaseSensitive(mqtt, "port");
        if (port != NULL) {
            if (cJSON_IsNumber(port)) {
                int value = (int)port->valuedouble;
                if (value < 1) {
                    value = 1;
                } else if (value > 65535) {
                    value = 65535;
                }
                settings->mqtt_port = (uint16_t)value;
            } else {
                invalid_type = true;
            }
        }

        /* Empty/omitted password keeps the previously stored value. */
        cJSON *password = cJSON_GetObjectItemCaseSensitive(mqtt, "password");
        if (password != NULL) {
            if (cJSON_IsString(password) && password->valuestring != NULL) {
                if (password->valuestring[0] != '\0') {
                    size_t len = strlen(password->valuestring);
                    if (len >= sizeof(settings->mqtt_password)) {
                        too_long = true;
                    } else {
                        strlcpy(settings->mqtt_password, password->valuestring, sizeof(settings->mqtt_password));
                    }
                }
            } else {
                invalid_type = true;
            }
        }
    }

    if (cJSON_IsObject(system)) {
        (void)update_bool_setting(system, "auto_restart_enabled", &settings->system_auto_restart_enabled, &invalid_type);

        cJSON *auto_restart_hours = cJSON_GetObjectItemCaseSensitive(system, "auto_restart_hours");
        if (auto_restart_hours != NULL) {
            if (cJSON_IsNumber(auto_restart_hours)) {
                int value = (int)auto_restart_hours->valuedouble;
                if (value < 1) {
                    value = 1;
                } else if (value > 168) {
                    value = 168;
                }
                settings->system_auto_restart_hours = (uint32_t)value;
            } else {
                invalid_type = true;
            }
        }
    }

    (void)update_string_setting(
        root, "wifi_ssid", settings->wifi_ssid, sizeof(settings->wifi_ssid), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "wifi_password", settings->wifi_password, sizeof(settings->wifi_password), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "wifi_country_code", settings->wifi_country_code, sizeof(settings->wifi_country_code), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "wifi_bssid", settings->wifi_bssid, sizeof(settings->wifi_bssid), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "ha_ws_url", settings->ha_ws_url, sizeof(settings->ha_ws_url), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "ha_access_token", settings->ha_access_token, sizeof(settings->ha_access_token), &invalid_type, &too_long);
    (void)update_bool_setting(root, "ha_rest_enabled", &settings->ha_rest_enabled, &invalid_type);
    (void)update_string_setting(
        root, "ntp_server", settings->ntp_server, sizeof(settings->ntp_server), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "time_tz", settings->time_tz, sizeof(settings->time_tz), &invalid_type, &too_long);
    (void)update_string_setting(
        root, "language", settings->ui_language, sizeof(settings->ui_language), &invalid_type, &too_long);

    bool reboot = true;
    cJSON *reboot_item = cJSON_GetObjectItemCaseSensitive(root, "reboot");
    if (reboot_item != NULL) {
        if (cJSON_IsBool(reboot_item)) {
            reboot = cJSON_IsTrue(reboot_item);
        } else {
            cJSON_Delete(root);
            free(settings);
            return send_json_error(req, "400 Bad Request", "reboot must be boolean");
        }
    }

    cJSON_Delete(root);

    if (invalid_type) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "One or more settings fields have invalid type");
    }
    if (too_long) {
        free(settings);
        return send_json_error(
            req,
            "400 Bad Request",
            "One or more settings values are too long (ssid<=32, wifi_password<=64, country_code<=2, bssid<=17, static_ip/netmask/gateway/dns<=15, ws_url<=255, token<=511, ntp<=127, timezone<=127, language<=15, mqtt_host<=128, mqtt_username<=64, mqtt_password<=64, mqtt_discovery_prefix<=64)");
    }
    if (!has_ws_scheme(settings->ha_ws_url)) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "ha.ws_url must start with ws:// or wss://");
    }
    if (!normalize_country_code(settings->wifi_country_code, sizeof(settings->wifi_country_code))) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "wifi.country_code must be a 2-letter ISO code (e.g. US, DE)");
    }
    if (!normalize_bssid(settings->wifi_bssid, sizeof(settings->wifi_bssid))) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "wifi.bssid must be empty or MAC format AA:BB:CC:DD:EE:FF");
    }
    if (settings->wifi_static_enabled) {
        if (!validate_ipv4(settings->wifi_static_ip) ||
            !validate_ipv4(settings->wifi_static_netmask) ||
            !validate_ipv4(settings->wifi_static_gateway)) {
            free(settings);
            return send_json_error(
                req,
                "400 Bad Request",
                "wifi.static_ip, wifi.static_netmask and wifi.static_gateway must be valid IPv4 addresses when static IP is enabled");
        }
    } else {
        settings->wifi_static_ip[0] = '\0';
        settings->wifi_static_netmask[0] = '\0';
        settings->wifi_static_gateway[0] = '\0';
        settings->wifi_static_dns[0] = '\0';
    }
    if (settings->wifi_static_dns[0] != '\0' && !validate_ipv4(settings->wifi_static_dns)) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "wifi.static_dns must be a valid IPv4 address or empty");
    }
    if (settings->wifi_ssid[0] == '\0') {
        settings->wifi_password[0] = '\0';
        settings->wifi_bssid[0] = '\0';
    }
    if (settings->ntp_server[0] == '\0') {
        strlcpy(settings->ntp_server, APP_NTP_SERVER, sizeof(settings->ntp_server));
    }
    if (settings->time_tz[0] == '\0') {
        strlcpy(settings->time_tz, APP_TIME_TZ, sizeof(settings->time_tz));
    }
    if (!normalize_ui_language(settings->ui_language, sizeof(settings->ui_language))) {
        free(settings);
        return send_json_error(req, "400 Bad Request", "ui.language must use [a-z0-9_-] and be 2-15 chars");
    }

    esp_err_t save_err = runtime_settings_save(settings);
    if (save_err == ESP_OK && !reboot) {
        /* Apply display settings immediately without a reboot. */
        ui_screen_saver_apply_settings(settings);
        /* (Re)configure native MQTT + HA discovery without a reboot. */
        panel_mqtt_notify_settings_changed();
    }
    free(settings);
    if (save_err != ESP_OK) {
        return httpd_resp_send_500(req);
    }

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddBoolToObject(resp, "rebooting", reboot);
    char *payload = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    esp_err_t send_err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);

    if (send_err == ESP_OK && reboot) {
        schedule_restart();
    }
    return send_err;
}

esp_err_t api_display_activity_post_handler(httpd_req_t *req)
{
    /* Wake the display: HA automations (e.g. a PIR motion sensor) POST here. */
    display_note_activity();

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(resp, "ok", true);
    char *payload = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}

#define WALLPAPER_UPLOAD_CHUNK 4096

esp_err_t api_display_wallpaper_post_handler(httpd_req_t *req)
{
    if (req->content_len != (int)APP_WALLPAPER_BYTES) {
        char msg[96] = {0};
        snprintf(msg, sizeof(msg), "Expected exactly %u bytes of RGB565 data", (unsigned)APP_WALLPAPER_BYTES);
        return send_json_error(req, "400 Bad Request", msg);
    }

    FILE *f = fopen(APP_WALLPAPER_TMP_PATH, "wb");
    if (f == NULL) {
        return send_json_error(req, "500 Internal Server Error", "Failed to open wallpaper file");
    }

    uint8_t *buf = (uint8_t *)heap_caps_malloc(WALLPAPER_UPLOAD_CHUNK, MALLOC_CAP_8BIT);
    if (buf == NULL) {
        fclose(f);
        remove(APP_WALLPAPER_TMP_PATH);
        return httpd_resp_send_500(req);
    }

    int received = 0;
    bool ok = true;
    while (received < req->content_len) {
        int remaining = req->content_len - received;
        int to_read = remaining > WALLPAPER_UPLOAD_CHUNK ? WALLPAPER_UPLOAD_CHUNK : remaining;
        int r = httpd_req_recv(req, (char *)buf, to_read);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            ok = false;
            break;
        }
        if (fwrite(buf, 1, (size_t)r, f) != (size_t)r) {
            ok = false;
            break;
        }
        received += r;
    }
    heap_caps_free(buf);
    fclose(f);

    if (!ok || received != req->content_len) {
        remove(APP_WALLPAPER_TMP_PATH);
        return send_json_error(req, "400 Bad Request", "Wallpaper upload aborted");
    }

    if (rename(APP_WALLPAPER_TMP_PATH, APP_WALLPAPER_PATH) != 0) {
        remove(APP_WALLPAPER_TMP_PATH);
        return send_json_error(req, "500 Internal Server Error", "Failed to store wallpaper");
    }

    ui_screen_saver_reload_wallpaper();
    display_note_activity();

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(resp, "ok", true);
    char *payload = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}

esp_err_t api_display_wallpaper_delete_handler(httpd_req_t *req)
{
    remove(APP_WALLPAPER_PATH);
    ui_screen_saver_reload_wallpaper();

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(resp, "ok", true);
    char *payload = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    set_json_headers(req);
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}
