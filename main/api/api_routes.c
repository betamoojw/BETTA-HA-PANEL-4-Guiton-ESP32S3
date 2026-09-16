/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "api/api_routes.h"
#include "api/http_guard.h"

#include "esp_check.h"

static esp_err_t guarded_api_layout_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_layout_get_handler);
}

static esp_err_t guarded_api_layout_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_layout_put_handler);
}

static esp_err_t guarded_api_entities_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_entities_get_handler);
}

static esp_err_t guarded_api_light_entities_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_light_entities_get_handler);
}

static esp_err_t guarded_api_light_entities_delete(httpd_req_t *req)
{
    return http_guard_handle(req, api_light_entities_delete_handler);
}

static esp_err_t guarded_api_ha_energy_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_ha_energy_get_handler);
}

static esp_err_t guarded_api_state_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_state_get_handler);
}

static esp_err_t guarded_api_settings_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_settings_get_handler);
}

static esp_err_t guarded_api_settings_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_settings_put_handler);
}

static esp_err_t guarded_api_sd_status_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_status_get_handler);
}

static esp_err_t guarded_api_sd_status_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_status_put_handler);
}

static esp_err_t guarded_api_sd_format_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_format_post_handler);
}

static esp_err_t guarded_api_sd_files_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_files_get_handler);
}

static esp_err_t guarded_api_sd_file_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_file_get_handler);
}

static esp_err_t guarded_api_sd_file_delete(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_file_delete_handler);
}

static esp_err_t guarded_api_sd_logs_export_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_sd_logs_export_post_handler);
}

static esp_err_t guarded_api_display_activity_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_display_activity_post_handler);
}

static esp_err_t guarded_api_display_wallpaper_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_display_wallpaper_post_handler);
}

static esp_err_t guarded_api_display_wallpaper_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_display_wallpaper_get_handler);
}

static esp_err_t guarded_api_display_wallpaper_delete(httpd_req_t *req)
{
    return http_guard_handle(req, api_display_wallpaper_delete_handler);
}

static esp_err_t guarded_api_i18n_languages_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_i18n_languages_get_handler);
}

static esp_err_t guarded_api_i18n_effective_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_i18n_effective_get_handler);
}

static esp_err_t guarded_api_i18n_custom_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_i18n_custom_put_handler);
}

static esp_err_t guarded_api_wifi_scan_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_wifi_scan_get_handler);
}

static esp_err_t guarded_api_version_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_version_get_handler);
}

static esp_err_t guarded_api_screenshot_bmp_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_screenshot_bmp_get_handler);
}

static esp_err_t guarded_api_ota_status_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_ota_status_get_handler);
}

static esp_err_t guarded_api_ota_url_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_ota_url_post_handler);
}

static esp_err_t guarded_api_ota_upload_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_ota_upload_post_handler);
}

static esp_err_t guarded_api_ha_diagnostics_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_ha_diagnostics_get_handler);
}

static esp_err_t guarded_api_diagnostics_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_diagnostics_get_handler);
}

static esp_err_t guarded_api_logs_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_logs_get_handler);
}

static esp_err_t guarded_api_logs_delete(httpd_req_t *req)
{
    return http_guard_handle(req, api_logs_delete_handler);
}

static esp_err_t guarded_api_backup_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_backup_get_handler);
}

static esp_err_t guarded_api_backup_restore_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_backup_restore_post_handler);
}

static esp_err_t guarded_api_themes_list_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_list_get_handler);
}
static esp_err_t guarded_api_themes_active_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_active_get_handler);
}
static esp_err_t guarded_api_themes_active_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_active_put_handler);
}
static esp_err_t guarded_api_themes_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_get_handler);
}
static esp_err_t guarded_api_themes_custom_put(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_custom_put_handler);
}
static esp_err_t guarded_api_themes_custom_delete(httpd_req_t *req)
{
    return http_guard_handle(req, api_themes_custom_delete_handler);
}
static esp_err_t guarded_api_pages_get(httpd_req_t *req)
{
    return http_guard_handle(req, api_pages_get_handler);
}
static esp_err_t guarded_api_pages_activate_post(httpd_req_t *req)
{
    return http_guard_handle(req, api_pages_activate_post_handler);
}

esp_err_t api_routes_register(httpd_handle_t server)
{
    if (server == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t get_layout = {
        .uri = "/api/layout",
        .method = HTTP_GET,
        .handler = guarded_api_layout_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_layout = {
        .uri = "/api/layout",
        .method = HTTP_PUT,
        .handler = guarded_api_layout_put,
        .user_ctx = NULL,
    };
    httpd_uri_t get_entities = {
        .uri = "/api/entities",
        .method = HTTP_GET,
        .handler = guarded_api_entities_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_light_entities = {
        .uri = "/api/ha/light_entities",
        .method = HTTP_GET,
        .handler = guarded_api_light_entities_get,
        .user_ctx = NULL,
    };
    httpd_uri_t delete_light_entities = {
        .uri = "/api/ha/light_entities",
        .method = HTTP_DELETE,
        .handler = guarded_api_light_entities_delete,
        .user_ctx = NULL,
    };
    httpd_uri_t get_ha_energy = {
        .uri = "/api/ha/energy",
        .method = HTTP_GET,
        .handler = guarded_api_ha_energy_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_state = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = guarded_api_state_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_settings = {
        .uri = "/api/settings",
        .method = HTTP_GET,
        .handler = guarded_api_settings_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_settings = {
        .uri = "/api/settings",
        .method = HTTP_PUT,
        .handler = guarded_api_settings_put,
        .user_ctx = NULL,
    };
    httpd_uri_t get_sd_status = {
        .uri = "/api/sd",
        .method = HTTP_GET,
        .handler = guarded_api_sd_status_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_sd_status = {
        .uri = "/api/sd",
        .method = HTTP_PUT,
        .handler = guarded_api_sd_status_put,
        .user_ctx = NULL,
    };
    httpd_uri_t post_sd_format = {
        .uri = "/api/sd/format",
        .method = HTTP_POST,
        .handler = guarded_api_sd_format_post,
        .user_ctx = NULL,
    };
    httpd_uri_t get_sd_files = {
        .uri = "/api/sd/files",
        .method = HTTP_GET,
        .handler = guarded_api_sd_files_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_sd_file = {
        .uri = "/api/sd/file",
        .method = HTTP_GET,
        .handler = guarded_api_sd_file_get,
        .user_ctx = NULL,
    };
    httpd_uri_t delete_sd_file = {
        .uri = "/api/sd/file",
        .method = HTTP_DELETE,
        .handler = guarded_api_sd_file_delete,
        .user_ctx = NULL,
    };
    httpd_uri_t post_sd_logs_export = {
        .uri = "/api/sd/logs/export",
        .method = HTTP_POST,
        .handler = guarded_api_sd_logs_export_post,
        .user_ctx = NULL,
    };
    httpd_uri_t post_display_activity = {
        .uri = "/api/display/activity",
        .method = HTTP_POST,
        .handler = guarded_api_display_activity_post,
        .user_ctx = NULL,
    };
    httpd_uri_t post_display_wallpaper = {
        .uri = "/api/display/wallpaper",
        .method = HTTP_POST,
        .handler = guarded_api_display_wallpaper_post,
        .user_ctx = NULL,
    };
    httpd_uri_t get_display_wallpaper = {
        .uri = "/api/display/wallpaper",
        .method = HTTP_GET,
        .handler = guarded_api_display_wallpaper_get,
        .user_ctx = NULL,
    };
    httpd_uri_t delete_display_wallpaper = {
        .uri = "/api/display/wallpaper",
        .method = HTTP_DELETE,
        .handler = guarded_api_display_wallpaper_delete,
        .user_ctx = NULL,
    };
    httpd_uri_t get_i18n_languages = {
        .uri = "/api/i18n/languages",
        .method = HTTP_GET,
        .handler = guarded_api_i18n_languages_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_i18n_effective = {
        .uri = "/api/i18n/effective",
        .method = HTTP_GET,
        .handler = guarded_api_i18n_effective_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_i18n_custom = {
        .uri = "/api/i18n/custom",
        .method = HTTP_PUT,
        .handler = guarded_api_i18n_custom_put,
        .user_ctx = NULL,
    };
    httpd_uri_t get_wifi_scan = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = guarded_api_wifi_scan_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_version = {
        .uri = "/api/version",
        .method = HTTP_GET,
        .handler = guarded_api_version_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_screenshot_bmp = {
        .uri = "/api/screenshot.bmp",
        .method = HTTP_GET,
        .handler = guarded_api_screenshot_bmp_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_ota_status = {
        .uri = "/api/ota/status",
        .method = HTTP_GET,
        .handler = guarded_api_ota_status_get,
        .user_ctx = NULL,
    };
    httpd_uri_t post_ota_url = {
        .uri = "/api/ota/url",
        .method = HTTP_POST,
        .handler = guarded_api_ota_url_post,
        .user_ctx = NULL,
    };
    httpd_uri_t post_ota_upload = {
        .uri = "/api/ota/upload",
        .method = HTTP_POST,
        .handler = guarded_api_ota_upload_post,
        .user_ctx = NULL,
    };

    httpd_uri_t get_themes_list = {
        .uri = "/api/themes",
        .method = HTTP_GET,
        .handler = guarded_api_themes_list_get,
        .user_ctx = NULL,
    };
    httpd_uri_t get_themes_active = {
        .uri = "/api/themes/active",
        .method = HTTP_GET,
        .handler = guarded_api_themes_active_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_themes_active = {
        .uri = "/api/themes/active",
        .method = HTTP_PUT,
        .handler = guarded_api_themes_active_put,
        .user_ctx = NULL,
    };
    httpd_uri_t get_themes_export = {
        .uri = "/api/themes/get",
        .method = HTTP_GET,
        .handler = guarded_api_themes_get,
        .user_ctx = NULL,
    };
    httpd_uri_t put_themes_custom = {
        .uri = "/api/themes/custom",
        .method = HTTP_PUT,
        .handler = guarded_api_themes_custom_put,
        .user_ctx = NULL,
    };
    httpd_uri_t delete_themes_custom = {
        .uri = "/api/themes/custom",
        .method = HTTP_DELETE,
        .handler = guarded_api_themes_custom_delete,
        .user_ctx = NULL,
    };

    httpd_uri_t get_ha_diagnostics = {
        .uri = "/api/ha/diagnostics",
        .method = HTTP_GET,
        .handler = guarded_api_ha_diagnostics_get,
        .user_ctx = NULL,
    };

    httpd_uri_t get_diagnostics = {
        .uri = "/api/diagnostics",
        .method = HTTP_GET,
        .handler = guarded_api_diagnostics_get,
        .user_ctx = NULL,
    };

    httpd_uri_t get_logs = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = guarded_api_logs_get,
        .user_ctx = NULL,
    };

    httpd_uri_t delete_logs = {
        .uri = "/api/logs",
        .method = HTTP_DELETE,
        .handler = guarded_api_logs_delete,
        .user_ctx = NULL,
    };

    httpd_uri_t get_backup = {
        .uri = "/api/backup",
        .method = HTTP_GET,
        .handler = guarded_api_backup_get,
        .user_ctx = NULL,
    };

    httpd_uri_t post_backup_restore = {
        .uri = "/api/backup/restore",
        .method = HTTP_POST,
        .handler = guarded_api_backup_restore_post,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_layout), "api_routes", "GET /api/layout");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &put_layout), "api_routes", "PUT /api/layout");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_entities), "api_routes", "GET /api/entities");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_light_entities), "api_routes", "GET /api/ha/light_entities");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &delete_light_entities), "api_routes", "DELETE /api/ha/light_entities");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_ha_energy), "api_routes", "GET /api/ha/energy");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_state), "api_routes", "GET /api/state");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_settings), "api_routes", "GET /api/settings");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &put_settings), "api_routes", "PUT /api/settings");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_sd_status), "api_routes", "GET /api/sd");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &put_sd_status), "api_routes", "PUT /api/sd");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &post_sd_format), "api_routes", "POST /api/sd/format");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_sd_files), "api_routes", "GET /api/sd/files");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_sd_file), "api_routes", "GET /api/sd/file");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &delete_sd_file), "api_routes", "DELETE /api/sd/file");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &post_sd_logs_export), "api_routes", "POST /api/sd/logs/export");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &post_display_activity), "api_routes", "POST /api/display/activity");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &post_display_wallpaper), "api_routes", "POST /api/display/wallpaper");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_display_wallpaper), "api_routes", "GET /api/display/wallpaper");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &delete_display_wallpaper), "api_routes", "DELETE /api/display/wallpaper");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_i18n_languages), "api_routes", "GET /api/i18n/languages");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_i18n_effective), "api_routes", "GET /api/i18n/effective");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &put_i18n_custom), "api_routes", "PUT /api/i18n/custom");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_wifi_scan), "api_routes", "GET /api/wifi/scan");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_version), "api_routes", "GET /api/version");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_screenshot_bmp), "api_routes", "GET /api/screenshot.bmp");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_ota_status), "api_routes", "GET /api/ota/status");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &post_ota_url), "api_routes", "POST /api/ota/url");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &post_ota_upload), "api_routes", "POST /api/ota/upload");

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_themes_list), "api_routes", "GET /api/themes");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_themes_active), "api_routes", "GET /api/themes/active");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &put_themes_active), "api_routes", "PUT /api/themes/active");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_themes_export), "api_routes", "GET /api/themes/get");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &put_themes_custom), "api_routes", "PUT /api/themes/custom");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &delete_themes_custom), "api_routes", "DELETE /api/themes/custom");

    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_ha_diagnostics), "api_routes", "GET /api/ha/diagnostics");

    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_diagnostics), "api_routes", "GET /api/diagnostics");

    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &get_logs), "api_routes", "GET /api/logs");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &delete_logs), "api_routes", "DELETE /api/logs");

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_backup), "api_routes", "GET /api/backup");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &post_backup_restore), "api_routes", "POST /api/backup/restore");

    httpd_uri_t get_pages = {
        .uri = "/api/pages",
        .method = HTTP_GET,
        .handler = guarded_api_pages_get,
        .user_ctx = NULL,
    };
    httpd_uri_t post_pages_activate = {
        .uri = "/api/pages/activate",
        .method = HTTP_POST,
        .handler = guarded_api_pages_activate_post,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_pages), "api_routes", "GET /api/pages");
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &post_pages_activate), "api_routes", "POST /api/pages/activate");

    return ESP_OK;
}
