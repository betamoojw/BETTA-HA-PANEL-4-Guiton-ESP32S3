/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "settings/runtime_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"

#include "settings/i18n_store.h"
#include "util/log_tags.h"

#define SETTINGS_NVS_NAMESPACE "runtime_sec"
#define SETTINGS_NVS_KEY_WIFI_PASSWORD "wifi_pwd"
#define SETTINGS_NVS_KEY_HA_ACCESS_TOKEN "ha_token"
#define SETTINGS_NVS_KEY_MQTT_PASSWORD "mqtt_pwd"

static bool is_placeholder(const char *text)
{
    return text == NULL || text[0] == '\0' || strstr(text, "YOUR_") != NULL;
}

static void normalize_ui_language(char *language, size_t language_len)
{
    if (language == NULL || language_len == 0) {
        return;
    }
    if (language[0] == '\0') {
        strlcpy(language, APP_UI_DEFAULT_LANGUAGE, language_len);
        return;
    }

    char normalized[APP_UI_LANGUAGE_MAX_LEN] = {0};
    if (!i18n_store_normalize_language_code(language, normalized, sizeof(normalized))) {
        strlcpy(language, APP_UI_DEFAULT_LANGUAGE, language_len);
        return;
    }
    strlcpy(language, normalized, language_len);
}

static void json_copy_string(cJSON *obj, const char *key, char *dst, size_t dst_len)
{
    if (obj == NULL || key == NULL || dst == NULL || dst_len == 0) {
        return;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        strlcpy(dst, item->valuestring, dst_len);
    }
}

static esp_err_t load_file_text(const char *path, size_t max_len, char **out_text)
{
    if (path == NULL || out_text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_text = NULL;

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_FAIL;
    }
    long size = ftell(f);
    if (size <= 0 || (size_t)size > max_len) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    rewind(f);

    char *buf = calloc((size_t)size + 1U, sizeof(char));
    if (buf == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read = fread(buf, 1U, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        return ESP_FAIL;
    }

    *out_text = buf;
    return ESP_OK;
}

static esp_err_t write_public_settings_file(const runtime_settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *ha = cJSON_CreateObject();
    cJSON *time_cfg = cJSON_CreateObject();
    cJSON *ui = cJSON_CreateObject();
    cJSON *display = cJSON_CreateObject();
    cJSON *mqtt = cJSON_CreateObject();
    cJSON *system = cJSON_CreateObject();
    if (root == NULL || wifi == NULL || ha == NULL || time_cfg == NULL || ui == NULL || display == NULL ||
        mqtt == NULL || system == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(wifi);
        cJSON_Delete(ha);
        cJSON_Delete(time_cfg);
        cJSON_Delete(ui);
        cJSON_Delete(display);
        cJSON_Delete(mqtt);
        cJSON_Delete(system);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "version", 1);

    cJSON_AddStringToObject(wifi, "ssid", settings->wifi_ssid);
    cJSON_AddStringToObject(wifi, "country_code", settings->wifi_country_code);
    cJSON_AddStringToObject(wifi, "bssid", settings->wifi_bssid);
    cJSON_AddBoolToObject(wifi, "static_enabled", settings->wifi_static_enabled);
    cJSON_AddStringToObject(wifi, "static_ip", settings->wifi_static_ip);
    cJSON_AddStringToObject(wifi, "static_netmask", settings->wifi_static_netmask);
    cJSON_AddStringToObject(wifi, "static_gateway", settings->wifi_static_gateway);
    cJSON_AddStringToObject(wifi, "static_dns", settings->wifi_static_dns);
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON_AddStringToObject(ha, "ws_url", settings->ha_ws_url);
    cJSON_AddBoolToObject(ha, "rest_enabled", settings->ha_rest_enabled);
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
    cJSON_AddStringToObject(mqtt, "discovery_prefix", settings->mqtt_discovery_prefix);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON_AddBoolToObject(system, "auto_restart_enabled", settings->system_auto_restart_enabled);
    cJSON_AddNumberToObject(system, "auto_restart_hours", (double)settings->system_auto_restart_hours);
    cJSON_AddItemToObject(root, "system", system);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    FILE *f = fopen(APP_SETTINGS_PATH, "wb");
    if (f == NULL) {
        cJSON_free(payload);
        return ESP_FAIL;
    }

    size_t len = strlen(payload);
    size_t written = fwrite(payload, 1U, len, f);
    fclose(f);
    cJSON_free(payload);

    if (written != len) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t parse_settings_json(
    const char *json,
    runtime_settings_t *out,
    bool *out_legacy_wifi_password,
    bool *out_legacy_ha_access_token,
    bool *out_legacy_mqtt_password)
{
    if (json == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_legacy_wifi_password != NULL) {
        *out_legacy_wifi_password = false;
    }
    if (out_legacy_ha_access_token != NULL) {
        *out_legacy_ha_access_token = false;
    }
    if (out_legacy_mqtt_password != NULL) {
        *out_legacy_mqtt_password = false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    cJSON *ha = cJSON_GetObjectItemCaseSensitive(root, "ha");
    cJSON *time_cfg = cJSON_GetObjectItemCaseSensitive(root, "time");
    cJSON *ui = cJSON_GetObjectItemCaseSensitive(root, "ui");
    cJSON *display = cJSON_GetObjectItemCaseSensitive(root, "display");
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
    cJSON *system = cJSON_GetObjectItemCaseSensitive(root, "system");

    if (cJSON_IsObject(wifi)) {
        json_copy_string(wifi, "ssid", out->wifi_ssid, sizeof(out->wifi_ssid));
        json_copy_string(wifi, "country_code", out->wifi_country_code, sizeof(out->wifi_country_code));
        json_copy_string(wifi, "bssid", out->wifi_bssid, sizeof(out->wifi_bssid));
        cJSON *static_enabled = cJSON_GetObjectItemCaseSensitive(wifi, "static_enabled");
        if (cJSON_IsBool(static_enabled)) {
            out->wifi_static_enabled = cJSON_IsTrue(static_enabled);
        }
        json_copy_string(wifi, "static_ip", out->wifi_static_ip, sizeof(out->wifi_static_ip));
        json_copy_string(wifi, "static_netmask", out->wifi_static_netmask, sizeof(out->wifi_static_netmask));
        json_copy_string(wifi, "static_gateway", out->wifi_static_gateway, sizeof(out->wifi_static_gateway));
        json_copy_string(wifi, "static_dns", out->wifi_static_dns, sizeof(out->wifi_static_dns));
        cJSON *pwd = cJSON_GetObjectItemCaseSensitive(wifi, "password");
        if (pwd != NULL) {
            if (out_legacy_wifi_password != NULL) {
                *out_legacy_wifi_password = true;
            }
            if (cJSON_IsString(pwd) && pwd->valuestring != NULL) {
                strlcpy(out->wifi_password, pwd->valuestring, sizeof(out->wifi_password));
            }
        }
    } else {
        json_copy_string(root, "wifi_ssid", out->wifi_ssid, sizeof(out->wifi_ssid));
        json_copy_string(root, "wifi_country_code", out->wifi_country_code, sizeof(out->wifi_country_code));
        json_copy_string(root, "wifi_bssid", out->wifi_bssid, sizeof(out->wifi_bssid));
        cJSON *static_enabled = cJSON_GetObjectItemCaseSensitive(root, "wifi_static_enabled");
        if (cJSON_IsBool(static_enabled)) {
            out->wifi_static_enabled = cJSON_IsTrue(static_enabled);
        }
        json_copy_string(root, "wifi_static_ip", out->wifi_static_ip, sizeof(out->wifi_static_ip));
        json_copy_string(root, "wifi_static_netmask", out->wifi_static_netmask, sizeof(out->wifi_static_netmask));
        json_copy_string(root, "wifi_static_gateway", out->wifi_static_gateway, sizeof(out->wifi_static_gateway));
        json_copy_string(root, "wifi_static_dns", out->wifi_static_dns, sizeof(out->wifi_static_dns));
        cJSON *pwd = cJSON_GetObjectItemCaseSensitive(root, "wifi_password");
        if (pwd != NULL) {
            if (out_legacy_wifi_password != NULL) {
                *out_legacy_wifi_password = true;
            }
            if (cJSON_IsString(pwd) && pwd->valuestring != NULL) {
                strlcpy(out->wifi_password, pwd->valuestring, sizeof(out->wifi_password));
            }
        }
    }

    if (cJSON_IsObject(ha)) {
        json_copy_string(ha, "ws_url", out->ha_ws_url, sizeof(out->ha_ws_url));
        cJSON *rest_enabled = cJSON_GetObjectItemCaseSensitive(ha, "rest_enabled");
        if (cJSON_IsBool(rest_enabled)) {
            out->ha_rest_enabled = cJSON_IsTrue(rest_enabled);
        }
        cJSON *token = cJSON_GetObjectItemCaseSensitive(ha, "access_token");
        if (token != NULL) {
            if (out_legacy_ha_access_token != NULL) {
                *out_legacy_ha_access_token = true;
            }
            if (cJSON_IsString(token) && token->valuestring != NULL) {
                strlcpy(out->ha_access_token, token->valuestring, sizeof(out->ha_access_token));
            }
        }
    } else {
        json_copy_string(root, "ha_ws_url", out->ha_ws_url, sizeof(out->ha_ws_url));
        cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "ha_access_token");
        if (token != NULL) {
            if (out_legacy_ha_access_token != NULL) {
                *out_legacy_ha_access_token = true;
            }
            if (cJSON_IsString(token) && token->valuestring != NULL) {
                strlcpy(out->ha_access_token, token->valuestring, sizeof(out->ha_access_token));
            }
        }
    }
    cJSON *rest_enabled = cJSON_GetObjectItemCaseSensitive(root, "ha_rest_enabled");
    if (cJSON_IsBool(rest_enabled)) {
        out->ha_rest_enabled = cJSON_IsTrue(rest_enabled);
    }

    if (cJSON_IsObject(time_cfg)) {
        json_copy_string(time_cfg, "ntp_server", out->ntp_server, sizeof(out->ntp_server));
        json_copy_string(time_cfg, "timezone", out->time_tz, sizeof(out->time_tz));
    } else {
        json_copy_string(root, "ntp_server", out->ntp_server, sizeof(out->ntp_server));
        json_copy_string(root, "time_tz", out->time_tz, sizeof(out->time_tz));
    }

    if (cJSON_IsObject(ui)) {
        json_copy_string(ui, "language", out->ui_language, sizeof(out->ui_language));
    } else {
        json_copy_string(root, "language", out->ui_language, sizeof(out->ui_language));
    }

    if (cJSON_IsObject(display)) {
        cJSON *brightness = cJSON_GetObjectItemCaseSensitive(display, "brightness");
        if (cJSON_IsNumber(brightness)) {
            int value = (int)brightness->valuedouble;
            if (value < 0) {
                value = 0;
            }
            if (value > 100) {
                value = 100;
            }
            out->display_brightness = (uint8_t)value;
        }
        cJSON *screensaver_enabled = cJSON_GetObjectItemCaseSensitive(display, "screensaver_enabled");
        if (cJSON_IsBool(screensaver_enabled)) {
            out->display_screensaver_enabled = cJSON_IsTrue(screensaver_enabled);
        }
        cJSON *screensaver_timeout = cJSON_GetObjectItemCaseSensitive(display, "screensaver_timeout_sec");
        if (cJSON_IsNumber(screensaver_timeout)) {
            double value = screensaver_timeout->valuedouble;
            if (value < 0) {
                value = 0;
            }
            out->display_screensaver_timeout_sec = (uint32_t)value;
        }
        cJSON *screen_off_enabled = cJSON_GetObjectItemCaseSensitive(display, "screen_off_enabled");
        if (cJSON_IsBool(screen_off_enabled)) {
            out->display_screen_off_enabled = cJSON_IsTrue(screen_off_enabled);
        }
        cJSON *screen_off_timeout = cJSON_GetObjectItemCaseSensitive(display, "screen_off_timeout_sec");
        if (cJSON_IsNumber(screen_off_timeout)) {
            double value = screen_off_timeout->valuedouble;
            if (value < 0) {
                value = 0;
            }
            out->display_screen_off_timeout_sec = (uint32_t)value;
        }
        cJSON *clock_24h = cJSON_GetObjectItemCaseSensitive(display, "clock_24h");
        if (cJSON_IsBool(clock_24h)) {
            out->display_clock_24h = cJSON_IsTrue(clock_24h);
        }
        cJSON *saver_show_seconds = cJSON_GetObjectItemCaseSensitive(display, "saver_show_seconds");
        if (cJSON_IsBool(saver_show_seconds)) {
            out->display_saver_show_seconds = cJSON_IsTrue(saver_show_seconds);
        }
        cJSON *saver_show_date = cJSON_GetObjectItemCaseSensitive(display, "saver_show_date");
        if (cJSON_IsBool(saver_show_date)) {
            out->display_saver_show_date = cJSON_IsTrue(saver_show_date);
        }
        cJSON *saver_clock_color = cJSON_GetObjectItemCaseSensitive(display, "saver_clock_color");
        if (cJSON_IsNumber(saver_clock_color)) {
            uint32_t value = (uint32_t)saver_clock_color->valuedouble;
            if (value > 0xFFFFFF) {
                value = 0xFFFFFF;
            }
            out->display_saver_clock_color = value;
        }
        cJSON *saver_date_color = cJSON_GetObjectItemCaseSensitive(display, "saver_date_color");
        if (cJSON_IsNumber(saver_date_color)) {
            uint32_t value = (uint32_t)saver_date_color->valuedouble;
            if (value > 0xFFFFFF) {
                value = 0xFFFFFF;
            }
            out->display_saver_date_color = value;
        }
    }

    if (cJSON_IsObject(mqtt)) {
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(mqtt, "enabled");
        if (cJSON_IsBool(enabled)) {
            out->mqtt_enabled = cJSON_IsTrue(enabled);
        }
        json_copy_string(mqtt, "host", out->mqtt_host, sizeof(out->mqtt_host));
        cJSON *port = cJSON_GetObjectItemCaseSensitive(mqtt, "port");
        if (cJSON_IsNumber(port)) {
            int value = (int)port->valuedouble;
            if (value < 1) {
                value = 1;
            }
            if (value > 65535) {
                value = 65535;
            }
            out->mqtt_port = (uint16_t)value;
        }
        json_copy_string(mqtt, "username", out->mqtt_username, sizeof(out->mqtt_username));
        json_copy_string(mqtt, "discovery_prefix", out->mqtt_discovery_prefix, sizeof(out->mqtt_discovery_prefix));
        cJSON *password = cJSON_GetObjectItemCaseSensitive(mqtt, "password");
        if (password != NULL) {
            if (out_legacy_mqtt_password != NULL) {
                *out_legacy_mqtt_password = true;
            }
            if (cJSON_IsString(password) && password->valuestring != NULL) {
                strlcpy(out->mqtt_password, password->valuestring, sizeof(out->mqtt_password));
            }
        }
    }

    if (cJSON_IsObject(system)) {
        cJSON *auto_restart_enabled = cJSON_GetObjectItemCaseSensitive(system, "auto_restart_enabled");
        if (cJSON_IsBool(auto_restart_enabled)) {
            out->system_auto_restart_enabled = cJSON_IsTrue(auto_restart_enabled);
        }
        cJSON *auto_restart_hours = cJSON_GetObjectItemCaseSensitive(system, "auto_restart_hours");
        if (cJSON_IsNumber(auto_restart_hours)) {
            int value = (int)auto_restart_hours->valuedouble;
            if (value < 1) {
                value = 1;
            }
            if (value > 168) {
                value = 168;
            }
            out->system_auto_restart_hours = (uint32_t)value;
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t nvs_load_secret(const char *key, char *out, size_t out_len, bool *out_found)
{
    if (key == NULL || out == NULL || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';
    if (out_found != NULL) {
        *out_found = false;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t required = 0;
    err = nvs_get_str(handle, key, NULL, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    if (required == 0) {
        if (out_found != NULL) {
            *out_found = true;
        }
        nvs_close(handle);
        return ESP_OK;
    }
    if (required > out_len) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    err = nvs_get_str(handle, key, out, &required);
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }

    if (out_found != NULL) {
        *out_found = true;
    }
    return ESP_OK;
}

static esp_err_t nvs_set_or_erase_secret(nvs_handle_t handle, const char *key, const char *value)
{
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (value[0] == '\0') {
        esp_err_t err = nvs_erase_key(handle, key);
        if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_OK;
        }
        return err;
    }

    return nvs_set_str(handle, key, value);
}

static esp_err_t nvs_save_secrets(const runtime_settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_or_erase_secret(handle, SETTINGS_NVS_KEY_WIFI_PASSWORD, settings->wifi_password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_set_or_erase_secret(handle, SETTINGS_NVS_KEY_HA_ACCESS_TOKEN, settings->ha_access_token);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_set_or_erase_secret(handle, SETTINGS_NVS_KEY_MQTT_PASSWORD, settings->mqtt_password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static void runtime_settings_merge_secrets_from_nvs(runtime_settings_t *settings)
{
    if (settings == NULL) {
        return;
    }

    char wifi_password[APP_WIFI_PASSWORD_MAX_LEN] = {0};
    bool has_wifi_password = false;
    esp_err_t wifi_err =
        nvs_load_secret(SETTINGS_NVS_KEY_WIFI_PASSWORD, wifi_password, sizeof(wifi_password), &has_wifi_password);
    if (wifi_err == ESP_OK && has_wifi_password) {
        strlcpy(settings->wifi_password, wifi_password, sizeof(settings->wifi_password));
    }

    char ha_access_token[APP_HA_ACCESS_TOKEN_MAX_LEN] = {0};
    bool has_ha_access_token = false;
    esp_err_t token_err = nvs_load_secret(
        SETTINGS_NVS_KEY_HA_ACCESS_TOKEN, ha_access_token, sizeof(ha_access_token), &has_ha_access_token);
    if (token_err == ESP_OK && has_ha_access_token) {
        strlcpy(settings->ha_access_token, ha_access_token, sizeof(settings->ha_access_token));
    }

    char mqtt_password[APP_MQTT_PASSWORD_MAX_LEN] = {0};
    bool has_mqtt_password = false;
    esp_err_t mqtt_err = nvs_load_secret(
        SETTINGS_NVS_KEY_MQTT_PASSWORD, mqtt_password, sizeof(mqtt_password), &has_mqtt_password);
    if (mqtt_err == ESP_OK && has_mqtt_password) {
        strlcpy(settings->mqtt_password, mqtt_password, sizeof(settings->mqtt_password));
    }
}

void runtime_settings_set_defaults(runtime_settings_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));

    if (!is_placeholder(APP_WIFI_SSID)) {
        strlcpy(out->wifi_ssid, APP_WIFI_SSID, sizeof(out->wifi_ssid));
    }
    if (!is_placeholder(APP_WIFI_PASSWORD)) {
        strlcpy(out->wifi_password, APP_WIFI_PASSWORD, sizeof(out->wifi_password));
    }
    strlcpy(out->wifi_country_code, APP_WIFI_COUNTRY_CODE, sizeof(out->wifi_country_code));
    if (!is_placeholder(APP_HA_WS_URL)) {
        strlcpy(out->ha_ws_url, APP_HA_WS_URL, sizeof(out->ha_ws_url));
    }
    if (!is_placeholder(APP_HA_ACCESS_TOKEN)) {
        strlcpy(out->ha_access_token, APP_HA_ACCESS_TOKEN, sizeof(out->ha_access_token));
    }
    out->ha_rest_enabled = false;
    strlcpy(out->ntp_server, APP_NTP_SERVER, sizeof(out->ntp_server));
    strlcpy(out->time_tz, APP_TIME_TZ, sizeof(out->time_tz));
    strlcpy(out->ui_language, APP_UI_DEFAULT_LANGUAGE, sizeof(out->ui_language));
    out->display_brightness = APP_DISPLAY_ACTIVE_BRIGHTNESS_PERCENT;
    out->display_screensaver_enabled = true;
    out->display_screensaver_timeout_sec = APP_DISPLAY_SCREENSAVER_TIMEOUT_SEC;
    out->display_screen_off_enabled = true;
    out->display_screen_off_timeout_sec = APP_DISPLAY_SCREEN_OFF_TIMEOUT_SEC;
    out->display_clock_24h = APP_DISPLAY_CLOCK_24H;
    out->display_saver_show_seconds = APP_DISPLAY_SAVER_SHOW_SECONDS;
    out->display_saver_show_date = APP_DISPLAY_SAVER_SHOW_DATE;
    out->display_saver_clock_color = APP_DISPLAY_SAVER_CLOCK_COLOR;
    out->display_saver_date_color = APP_DISPLAY_SAVER_DATE_COLOR;
    out->mqtt_enabled = APP_MQTT_ENABLED_DEFAULT;
    out->mqtt_host[0] = '\0';
    out->mqtt_port = APP_MQTT_PORT_DEFAULT;
    out->mqtt_username[0] = '\0';
    out->mqtt_password[0] = '\0';
    strlcpy(out->mqtt_discovery_prefix, APP_MQTT_DISCOVERY_PREFIX_DEFAULT, sizeof(out->mqtt_discovery_prefix));
    out->system_auto_restart_enabled = false;
    out->system_auto_restart_hours = 24;
}

esp_err_t runtime_settings_load(runtime_settings_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    runtime_settings_set_defaults(out);

    char *json = NULL;
    esp_err_t file_err = load_file_text(APP_SETTINGS_PATH, APP_SETTINGS_MAX_JSON_LEN, &json);
    if (file_err != ESP_OK) {
        return file_err;
    }

    bool legacy_wifi_password = false;
    bool legacy_ha_access_token = false;
    bool legacy_mqtt_password = false;
    esp_err_t parse_err =
        parse_settings_json(json, out, &legacy_wifi_password, &legacy_ha_access_token, &legacy_mqtt_password);
    free(json);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    if (out->wifi_country_code[0] == '\0') {
        strlcpy(out->wifi_country_code, APP_WIFI_COUNTRY_CODE, sizeof(out->wifi_country_code));
    }
    normalize_ui_language(out->ui_language, sizeof(out->ui_language));

    char nvs_wifi_password[APP_WIFI_PASSWORD_MAX_LEN] = {0};
    bool has_nvs_wifi_password = false;
    esp_err_t nvs_err =
        nvs_load_secret(SETTINGS_NVS_KEY_WIFI_PASSWORD, nvs_wifi_password, sizeof(nvs_wifi_password), &has_nvs_wifi_password);
    if (nvs_err != ESP_OK) {
        return nvs_err;
    }
    if (has_nvs_wifi_password) {
        strlcpy(out->wifi_password, nvs_wifi_password, sizeof(out->wifi_password));
    }

    char nvs_ha_access_token[APP_HA_ACCESS_TOKEN_MAX_LEN] = {0};
    bool has_nvs_ha_access_token = false;
    nvs_err = nvs_load_secret(
        SETTINGS_NVS_KEY_HA_ACCESS_TOKEN, nvs_ha_access_token, sizeof(nvs_ha_access_token), &has_nvs_ha_access_token);
    if (nvs_err != ESP_OK) {
        return nvs_err;
    }
    if (has_nvs_ha_access_token) {
        strlcpy(out->ha_access_token, nvs_ha_access_token, sizeof(out->ha_access_token));
    }

    char nvs_mqtt_password[APP_MQTT_PASSWORD_MAX_LEN] = {0};
    bool has_nvs_mqtt_password = false;
    nvs_err = nvs_load_secret(
        SETTINGS_NVS_KEY_MQTT_PASSWORD, nvs_mqtt_password, sizeof(nvs_mqtt_password), &has_nvs_mqtt_password);
    if (nvs_err != ESP_OK) {
        return nvs_err;
    }
    if (has_nvs_mqtt_password) {
        strlcpy(out->mqtt_password, nvs_mqtt_password, sizeof(out->mqtt_password));
    }

    bool migrate_to_nvs =
        (legacy_wifi_password && !has_nvs_wifi_password && out->wifi_password[0] != '\0') ||
        (legacy_ha_access_token && !has_nvs_ha_access_token && out->ha_access_token[0] != '\0') ||
        (legacy_mqtt_password && !has_nvs_mqtt_password && out->mqtt_password[0] != '\0');

    if (migrate_to_nvs) {
        nvs_err = nvs_save_secrets(out);
        if (nvs_err != ESP_OK) {
            return nvs_err;
        }
    }

    if (legacy_wifi_password || legacy_ha_access_token || legacy_mqtt_password) {
        esp_err_t scrub_err = write_public_settings_file(out);
        if (scrub_err != ESP_OK) {
            ESP_LOGW(TAG_APP, "Failed to scrub legacy secrets from LittleFS settings: %s", esp_err_to_name(scrub_err));
        }
    }

    return ESP_OK;
}

esp_err_t runtime_settings_save(const runtime_settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t file_err = write_public_settings_file(settings);
    if (file_err != ESP_OK) {
        return file_err;
    }

    esp_err_t nvs_err = nvs_save_secrets(settings);
    if (nvs_err != ESP_OK) {
        return nvs_err;
    }

    ESP_LOGI(TAG_APP, "Saved runtime settings");
    return ESP_OK;
}

esp_err_t runtime_settings_init(void)
{
    runtime_settings_t *settings = calloc(1, sizeof(runtime_settings_t));
    if (settings == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = runtime_settings_load(settings);
    if (err == ESP_OK) {
        free(settings);
        return ESP_OK;
    }

    ESP_LOGW(TAG_APP, "Settings missing/invalid (%s), writing defaults", esp_err_to_name(err));
    runtime_settings_set_defaults(settings);
    runtime_settings_merge_secrets_from_nvs(settings);
    esp_err_t save_err = runtime_settings_save(settings);
    free(settings);
    return save_err;
}

bool runtime_settings_has_wifi(const runtime_settings_t *settings)
{
    return settings != NULL && settings->wifi_ssid[0] != '\0';
}

bool runtime_settings_has_ha(const runtime_settings_t *settings)
{
    return settings != NULL && settings->ha_ws_url[0] != '\0' && settings->ha_access_token[0] != '\0';
}
