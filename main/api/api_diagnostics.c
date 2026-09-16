/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "api/api_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "cJSON.h"
#include "diag/boot_guard.h"
#include "driver/temperature_sensor.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ha/ha_client.h"
#include "mqtt/panel_mqtt.h"
#include "net/wifi_mgr.h"
#include "ui/ui_lvgl_mem.h"

/* The temperature sensor is installed and enabled lazily on the first request
 * and stays enabled afterwards: install/enable cycle costs several register
 * syncs and the wall panel has no power budget concern. */
static temperature_sensor_handle_t s_temp_sensor = NULL;
static SemaphoreHandle_t s_temp_sensor_mutex = NULL;
static bool s_temp_sensor_failed = false;

static const char *chip_model_str(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32: return "ESP32";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
    case CHIP_ESP32C2: return "ESP32-C2";
    case CHIP_ESP32C6: return "ESP32-C6";
    case CHIP_ESP32H2: return "ESP32-H2";
    case CHIP_ESP32P4: return "ESP32-P4";
    default: return "unknown";
    }
}

/* Internal DRAM is split into pools with different capability masks (the plain
 * 8-bit pool, the 32-bit-capable pool and the DMA-capable reserve taken by
 * SPIRAM_MALLOC_RESERVE_INTERNAL). A single MALLOC_CAP_INTERNAL figure hides
 * which pool is actually tight, so the panel reports each of them. */
static void diagnostics_add_heap_region(cJSON *parent, const char *name, uint32_t caps)
{
    multi_heap_info_t info;
    memset(&info, 0, sizeof(info));
    heap_caps_get_info(&info, caps);

    size_t total = info.total_free_bytes + info.total_allocated_bytes;
    cJSON *obj = cJSON_AddObjectToObject(parent, name);
    if (obj == NULL) {
        return;
    }
    cJSON_AddNumberToObject(obj, "total", (double)total);
    cJSON_AddNumberToObject(obj, "free", (double)info.total_free_bytes);
    cJSON_AddNumberToObject(obj, "largest_block", (double)info.largest_free_block);
    cJSON_AddNumberToObject(obj, "free_min", (double)info.minimum_free_bytes);
    cJSON_AddNumberToObject(obj, "allocated", (double)info.total_allocated_bytes);
    cJSON_AddNumberToObject(obj, "alloc_blocks", (double)info.allocated_blocks);
    cJSON_AddNumberToObject(obj, "free_blocks", (double)info.free_blocks);
    cJSON_AddNumberToObject(
        obj, "fragmentation_pct",
        (info.total_free_bytes > 0)
            ? (double)(100 - (int)((info.largest_free_block * 100) / info.total_free_bytes))
            : 0.0);
}

static bool diagnostics_read_cpu_temp(float *out_celsius)
{
    if (out_celsius == NULL || s_temp_sensor_failed) {
        return false;
    }
    if (s_temp_sensor_mutex == NULL) {
        s_temp_sensor_mutex = xSemaphoreCreateMutex();
        if (s_temp_sensor_mutex == NULL) {
            s_temp_sensor_failed = true;
            return false;
        }
    }
    if (xSemaphoreTake(s_temp_sensor_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }

    bool ok = false;
    if (s_temp_sensor == NULL) {
        const temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
        if (temperature_sensor_install(&cfg, &s_temp_sensor) != ESP_OK) {
            s_temp_sensor = NULL;
            s_temp_sensor_failed = true;
        } else if (temperature_sensor_enable(s_temp_sensor) != ESP_OK) {
            (void)temperature_sensor_uninstall(s_temp_sensor);
            s_temp_sensor = NULL;
            s_temp_sensor_failed = true;
        }
    }

    float celsius = 0.0f;
    if (s_temp_sensor != NULL && temperature_sensor_get_celsius(s_temp_sensor, &celsius) == ESP_OK) {
        *out_celsius = celsius;
        ok = true;
    }

    xSemaphoreGive(s_temp_sensor_mutex);
    return ok;
}

static void add_wifi_section(cJSON *root)
{
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    if (wifi == NULL) {
        return;
    }

    bool connected = wifi_mgr_is_connected();
    cJSON_AddBoolToObject(wifi, "connected", connected);
    cJSON_AddBoolToObject(wifi, "setup_ap_active", wifi_mgr_is_setup_ap_active());

    char ip[48] = {0};
    if (wifi_mgr_get_sta_ip(ip, sizeof(ip)) == ESP_OK && ip[0] != '\0') {
        cJSON_AddStringToObject(wifi, "ip", ip);
    }

    wifi_mgr_sta_ap_info_t ap_info = {0};
    if (wifi_mgr_get_sta_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddStringToObject(wifi, "ssid", ap_info.ssid);
        cJSON_AddNumberToObject(wifi, "rssi", ap_info.rssi);
        cJSON_AddNumberToObject(wifi, "channel", ap_info.channel);
        cJSON_AddNumberToObject(wifi, "authmode", ap_info.authmode);
    }

    const char *setup_ssid = wifi_mgr_get_setup_ap_ssid();
    if (setup_ssid != NULL && setup_ssid[0] != '\0') {
        cJSON_AddStringToObject(wifi, "setup_ap_ssid", setup_ssid);
    }

    wifi_mgr_link_stats_t stats;
    if (wifi_mgr_get_link_stats(&stats) == ESP_OK) {
        cJSON_AddNumberToObject(wifi, "connect_count", (double)stats.connect_count);
        cJSON_AddNumberToObject(wifi, "disconnect_count", (double)stats.disconnect_count);
        cJSON_AddNumberToObject(wifi, "reconnect_count", (double)stats.reconnect_count);
        cJSON_AddNumberToObject(wifi, "hard_recover_count", (double)stats.hard_recover_count);
        cJSON_AddNumberToObject(wifi, "last_disconnect_reason", (double)stats.last_disconnect_reason);
        cJSON_AddNumberToObject(wifi, "last_connect_uptime_ms", (double)stats.last_connect_uptime_ms);
        cJSON_AddNumberToObject(wifi, "last_session_ms", (double)stats.last_session_ms);
    }
}

static void add_ha_section(cJSON *root)
{
    cJSON *ha = cJSON_AddObjectToObject(root, "ha");
    if (ha == NULL) {
        return;
    }

    cJSON_AddBoolToObject(ha, "connected", ha_client_is_connected());
    cJSON_AddBoolToObject(ha, "initial_sync_done", ha_client_is_initial_sync_done());
    cJSON_AddBoolToObject(ha, "heavy_gate_busy", ha_client_heavy_gate_is_busy());

    ha_client_http_ctx_t ctx;
    if (ha_client_get_http_context(&ctx)) {
        /* Intentionally omits bearer_token: diagnostics must never leak it. */
        cJSON_AddStringToObject(ha, "base_url", ctx.base_url);
        cJSON_AddStringToObject(ha, "cert_common_name", ctx.cert_common_name);
    }

    ha_client_diagnostics_t diag;
    ha_client_get_diagnostics(&diag);
    cJSON_AddNumberToObject(ha, "missing_entities", (double)diag.total);

    ha_client_link_stats_t stats;
    ha_client_get_link_stats(&stats);

    cJSON *link = cJSON_AddObjectToObject(ha, "link");
    if (link != NULL) {
        cJSON_AddNumberToObject(link, "connect_count", (double)stats.connect_count);
        cJSON_AddNumberToObject(link, "disconnect_count", (double)stats.disconnect_count);
        cJSON_AddNumberToObject(link, "recover_count", (double)stats.recover_count);
        cJSON_AddNumberToObject(link, "error_streak", (double)stats.error_streak);
        cJSON_AddNumberToObject(link, "short_session_strikes", (double)stats.short_session_strikes);
        cJSON_AddNumberToObject(link, "last_connected_uptime_ms", (double)stats.last_connected_unix_ms);
        cJSON_AddNumberToObject(link, "last_session_ms", (double)stats.last_session_ms);
        cJSON_AddBoolToObject(link, "session_healthy",
            stats.connect_count > 0 && stats.error_streak == 0);
    }
}

static void add_mqtt_section(cJSON *root)
{
    cJSON *mqtt = cJSON_AddObjectToObject(root, "mqtt");
    if (mqtt == NULL) {
        return;
    }

    const char *uri = panel_mqtt_broker_uri();
    bool has_uri = (uri != NULL && uri[0] != '\0');
    cJSON_AddBoolToObject(mqtt, "connected", panel_mqtt_is_connected());
    cJSON_AddBoolToObject(mqtt, "enabled", has_uri);
    if (has_uri) {
        cJSON_AddStringToObject(mqtt, "broker_uri", uri);
        cJSON_AddBoolToObject(mqtt, "tls", (strncmp(uri, "mqtts://", 8) == 0));
    }
}

static void add_ota_section(cJSON *root)
{
    cJSON *ota = cJSON_AddObjectToObject(root, "ota");
    if (ota == NULL) {
        return;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    cJSON_AddStringToObject(ota, "running_partition", (running != NULL) ? running->label : "");
    cJSON_AddStringToObject(ota, "next_update_partition", (next != NULL) ? next->label : "");
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    cJSON_AddBoolToObject(ota, "rollback_enabled", true);
#else
    cJSON_AddBoolToObject(ota, "rollback_enabled", false);
#endif

    boot_guard_info_t boot;
    boot_guard_get_info(&boot);
    cJSON_AddNumberToObject(ota, "boot_count", (double)boot.boot_count);
    cJSON_AddStringToObject(ota, "reset_reason", boot_guard_reset_reason_str(boot.reset_reason));
    cJSON_AddNumberToObject(ota, "reset_reason_code", (double)boot.reset_reason);
    cJSON_AddBoolToObject(ota, "rollback_pending", boot.rollback_pending);
    cJSON_AddBoolToObject(ota, "boot_confirmed", boot.confirmed);
    cJSON_AddStringToObject(ota, "image_state", boot_guard_ota_state_str(boot.ota_state));
}

esp_err_t api_diagnostics_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_500(req);
    }

    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddNumberToObject(root, "uptime_ms", (double)(esp_timer_get_time() / 1000));

    const esp_app_desc_t *desc = esp_app_get_description();
    cJSON *app = cJSON_AddObjectToObject(root, "app");
    if (app != NULL) {
        cJSON_AddStringToObject(app, "name", APP_NAME);
        cJSON_AddStringToObject(app, "version",
            (desc != NULL && desc->version[0] != '\0') ? desc->version : "unknown");
        cJSON_AddStringToObject(app, "project",
            (desc != NULL && desc->project_name[0] != '\0') ? desc->project_name : APP_NAME);
        cJSON_AddStringToObject(app, "idf_version", esp_get_idf_version());
        cJSON_AddStringToObject(app, "build_date",
            (desc != NULL) ? desc->date : "");
        cJSON_AddStringToObject(app, "build_time",
            (desc != NULL) ? desc->time : "");
    }

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    cJSON *hw = cJSON_AddObjectToObject(root, "chip");
    if (hw != NULL) {
        cJSON_AddStringToObject(hw, "model", chip_model_str(chip.model));
        cJSON_AddNumberToObject(hw, "cores", chip.cores);
        cJSON_AddNumberToObject(hw, "revision", chip.revision);
        cJSON_AddNumberToObject(hw, "features", (double)chip.features);
        cJSON_AddNumberToObject(hw, "screen_w", APP_SCREEN_WIDTH);
        cJSON_AddNumberToObject(hw, "screen_h", APP_SCREEN_HEIGHT);
    }

    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t heap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    cJSON *mem = cJSON_AddObjectToObject(root, "memory");
    if (mem != NULL) {
        cJSON_AddNumberToObject(mem, "heap_free", (double)heap_free);
        cJSON_AddNumberToObject(mem, "heap_free_min", (double)heap_min);
        cJSON_AddNumberToObject(mem, "heap_largest_block", (double)heap_largest);
        cJSON_AddNumberToObject(mem, "heap_fragmentation_pct",
            (heap_free > 0) ? (double)(100 - (int)((heap_largest * 100) / heap_free)) : 0.0);
        cJSON_AddNumberToObject(mem, "psram_free", (double)psram_free);
        cJSON_AddNumberToObject(mem, "psram_largest_block", (double)psram_largest);
        cJSON_AddNumberToObject(mem, "psram_fragmentation_pct",
            (psram_free > 0) ? (double)(100 - (int)((psram_largest * 100) / psram_free)) : 0.0);

        cJSON_AddNumberToObject(mem, "iram_free", (double)heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));
        cJSON_AddNumberToObject(mem, "iram_largest_block", (double)heap_caps_get_largest_free_block(MALLOC_CAP_IRAM_8BIT));

        cJSON *regions = cJSON_AddObjectToObject(mem, "regions");
        if (regions != NULL) {
            diagnostics_add_heap_region(regions, "internal", MALLOC_CAP_INTERNAL);
            diagnostics_add_heap_region(regions, "internal_dma", MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            diagnostics_add_heap_region(regions, "internal_32bit", MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
            diagnostics_add_heap_region(regions, "psram", MALLOC_CAP_SPIRAM);
            diagnostics_add_heap_region(regions, "default", MALLOC_CAP_DEFAULT);
        }
    }

    ui_lvgl_mem_stats_t lvgl_mem;
    ui_lvgl_mem_get_stats(&lvgl_mem);
    cJSON *lvgl = cJSON_AddObjectToObject(root, "lvgl");
    if (lvgl != NULL) {
        cJSON_AddNumberToObject(lvgl, "used_bytes", (double)lvgl_mem.used_bytes);
        cJSON_AddNumberToObject(lvgl, "peak_bytes", (double)lvgl_mem.peak_bytes);
        cJSON_AddNumberToObject(lvgl, "live_blocks", (double)lvgl_mem.block_count);
        cJSON_AddNumberToObject(lvgl, "alloc_count", (double)lvgl_mem.alloc_count);
        cJSON_AddNumberToObject(lvgl, "free_count", (double)lvgl_mem.free_count);
        cJSON_AddNumberToObject(lvgl, "fail_count", (double)lvgl_mem.fail_count);
    }

    float cpu_temp = 0.0f;
    if (diagnostics_read_cpu_temp(&cpu_temp)) {
        cJSON_AddNumberToObject(root, "cpu_temp_c", (double)cpu_temp);
    }

    add_wifi_section(root);
    add_ha_section(root);
    add_mqtt_section(root);
    add_ota_section(root);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t err = httpd_resp_sendstr(req, payload);
    cJSON_free(payload);
    return err;
}
