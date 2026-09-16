/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 *
 * Copies the persistent LittleFS system log to the microSD card so long-term
 * history survives a reflash or a factory reset of the internal filesystem.
 *
 * `esp_littlefs` has no readdir(), so the exports are built from the known
 * rotation naming scheme (/littlefs/logs/system.log plus `.1` … `.N`) instead
 * of walking the directory.  Data is streamed through a small fixed buffer, so
 * memory use does not depend on the log size.
 */
#include "sd/sd_logs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "app_task.h"
#include "diag/system_log.h"
#include "sd/sd_card.h"
#include "util/log_tags.h"

#if APP_SD_SUPPORTED

static const char *TAG = TAG_SD;

#define SD_EXPORT_CHUNK_BYTES 2048
#define SD_EXPORT_TASK_STACK 4096
#define SD_EXPORT_TASK_PRIO 1

static TaskHandle_t s_export_task;

static void log_source_path(int generation, char *out, size_t out_len)
{
    if (generation <= 0) {
        snprintf(out, out_len, "%s", APP_LOG_FILE);
    } else {
        snprintf(out, out_len, "%s.%d", APP_LOG_FILE, generation);
    }
}

/* Appends one LittleFS log file to the open export file.
 * Returns the number of bytes appended (0 when the generation is missing). */
static size_t append_generation(FILE *dest, int generation, char *buf, size_t buf_len)
{
    char source[sizeof(APP_LOG_FILE) + 8];
    log_source_path(generation, source, sizeof(source));

    FILE *input = fopen(source, "rb");
    if (input == NULL) {
        return 0;
    }

    size_t total = 0;
    size_t got = 0;
    while ((got = fread(buf, 1, buf_len, input)) > 0) {
        if (fwrite(buf, 1, got, dest) != got) {
            ESP_LOGW(TAG, "Write failed while exporting logs");
            break;
        }
        total += got;
    }
    fclose(input);
    return total;
}

esp_err_t sd_logs_prune(void)
{
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* One slot above the limit: a listing that stops exactly at the limit cannot
     * tell a full folder from an overflowing one. */
    enum { LOG_SCAN_SLOTS = APP_SD_LOG_MAX_FILES + 1 };
    for (int pass = 0; pass < LOG_SCAN_SLOTS * 2; pass++) {
        sd_dir_entry_t entries[LOG_SCAN_SLOTS];
        size_t count = 0;
        if (sd_card_list(APP_SD_LOG_DIR, entries, LOG_SCAN_SLOTS, &count) != ESP_OK) {
            /* The folder may not exist yet. */
            return ESP_OK;
        }

        /* Oldest first: the exporter uses a sortable timestamp as file name. */
        size_t oldest = 0;
        uint64_t total_bytes = 0;
        for (size_t i = 0; i < count; i++) {
            total_bytes += entries[i].size;
            if (strcmp(entries[i].name, entries[oldest].name) < 0) {
                oldest = i;
            }
        }

        const bool too_many = count > APP_SD_LOG_MAX_FILES;
        const bool too_big = total_bytes > APP_SD_LOG_MAX_BYTES;
        if (!too_many && !too_big) {
            return ESP_OK;
        }

        char rel[APP_SD_MAX_PATH_LEN];
        snprintf(rel, sizeof(rel), "%s/%s", APP_SD_LOG_DIR, entries[oldest].name);
        if (sd_card_remove(rel) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t sd_logs_export(char *out_name, size_t out_name_len)
{
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    time_t now = 0;
    time(&now);
    struct tm local = {0};
    localtime_r(&now, &local);

    char name[APP_SD_MAX_NAME_LEN];
    strftime(name, sizeof(name), "panel-%Y%m%d-%H%M%S.log", &local);

    char rel[APP_SD_MAX_PATH_LEN];
    snprintf(rel, sizeof(rel), "%s/%s", APP_SD_LOG_DIR, name);

    char path[APP_SD_MAX_PATH_LEN];
    if (!sd_card_build_path(rel, path, sizeof(path))) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *dest = fopen(path, "wb");
    if (dest == NULL) {
        ESP_LOGW(TAG, "Cannot open %s for writing", path);
        return ESP_FAIL;
    }

    char *buf = malloc(SD_EXPORT_CHUNK_BYTES);
    if (buf == NULL) {
        fclose(dest);
        remove(path);
        return ESP_ERR_NO_MEM;
    }

    size_t written = 0;
    /* Rotated generations are older, so they are copied first. */
    for (int generation = APP_LOG_MAX_ROTATED; generation >= 1; generation--) {
        written += append_generation(dest, generation, buf, SD_EXPORT_CHUNK_BYTES);
    }
    written += append_generation(dest, 0, buf, SD_EXPORT_CHUNK_BYTES);

    free(buf);
    fclose(dest);

    if (written == 0) {
        remove(path);
        return ESP_ERR_NOT_FOUND;
    }

    if (out_name != NULL && out_name_len > 0) {
        snprintf(out_name, out_name_len, "%s", name);
    }
    system_log_write_info(TAG_SD, "Log exported to %s (%u bytes)", name, (unsigned)written);
    sd_logs_prune();
    return ESP_OK;
}

static void sd_export_task(void *arg)
{
    (void)arg;

    /* First export happens on the next boot tick so the card has settled. */
    vTaskDelay(pdMS_TO_TICKS(30000));

    for (;;) {
        if (sd_card_is_mounted()) {
            (void)sd_logs_export(NULL, 0);
        }
#if APP_SD_LOG_EXPORT_PERIOD_SEC > 0
        vTaskDelay(pdMS_TO_TICKS(APP_SD_LOG_EXPORT_PERIOD_SEC * 1000));
#else
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000));
#endif
    }
}

void sd_logs_start(void)
{
    if (s_export_task != NULL) {
        return;
    }
#if APP_SD_LOG_EXPORT_PERIOD_SEC > 0
    if (app_task_create(sd_export_task, "sd_logs", SD_EXPORT_TASK_STACK, NULL, SD_EXPORT_TASK_PRIO,
            &s_export_task) != pdPASS) {
        ESP_LOGW(TAG, "Failed to create log export task");
        s_export_task = NULL;
    }
#endif
}

#else /* !APP_SD_SUPPORTED */

esp_err_t sd_logs_export(char *out_name, size_t out_name_len)
{
    (void)out_name;
    (void)out_name_len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_logs_prune(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

void sd_logs_start(void)
{
}

#endif /* APP_SD_SUPPORTED */
