/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "boot_guard.h"

#include <stdio.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "diag/system_log.h"
#include "net/wifi_mgr.h"
#include "util/log_tags.h"

/* Boot counter shared through RTC memory: it survives software, panic and
 * watchdog resets (so a reboot loop is visible in the log) and is zeroed by a
 * real power-on reset. */
#define BOOT_GUARD_MAGIC 0x42475431UL /* "BGT1" */
RTC_NOINIT_ATTR static uint32_t s_rtc_magic;
RTC_NOINIT_ATTR static uint32_t s_rtc_boot_count;

#define BOOT_GUARD_POLL_MS 5000

static bool s_initialized;
static boot_guard_info_t s_info;
static bool s_confirmed;

static esp_err_t boot_guard_read_ota_state(uint32_t *out_state)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err != ESP_OK) {
        return err;
    }

    *out_state = (uint32_t)state;
    return ESP_OK;
}

void boot_guard_init(void)
{
    const esp_reset_reason_t reason = esp_reset_reason();

    if (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT || s_rtc_magic != BOOT_GUARD_MAGIC) {
        s_rtc_boot_count = 1;
        s_rtc_magic = BOOT_GUARD_MAGIC;
    } else {
        s_rtc_boot_count++;
    }

    s_info.reset_reason = reason;
    s_info.boot_count = s_rtc_boot_count;
    s_info.ota_state_err = boot_guard_read_ota_state(&s_info.ota_state);
    s_info.rollback_pending = (s_info.ota_state_err == ESP_OK && s_info.ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    s_info.confirmed = false;
    s_info.confirm_failed = false;
    s_initialized = true;
}

void boot_guard_report_boot(void)
{
    if (!s_initialized) {
        boot_guard_init();
    }

    char line[160];
    snprintf(line,
             sizeof(line),
             "boot #%lu, reset reason: %s, OTA state: %s",
             (unsigned long)s_info.boot_count,
             boot_guard_reset_reason_str(s_info.reset_reason),
             s_info.ota_state_err == ESP_OK ? boot_guard_ota_state_str(s_info.ota_state)
                                            : esp_err_to_name(s_info.ota_state_err));
    ESP_LOGI(TAG_APP, "%s", line);
    /* The capture hook only forwards WARN/ERROR, so write the boot line into
     * the persistent log explicitly: it is the first thing to look at when the
     * panel misbehaves after an update. */
    system_log_write_info("app", "%s", line);

    if (s_info.boot_count >= 10) {
        ESP_LOGW(TAG_APP,
                 "%lu boots since power-on: repeated restarts, check /api/logs and /api/diagnostics",
                 (unsigned long)s_info.boot_count);
    }
    if (s_info.rollback_pending) {
        ESP_LOGW(TAG_APP, "running image is PENDING_VERIFY; it must confirm itself or the panel rolls back on reboot");
    }
}

static void boot_guard_confirm_task(void *arg)
{
    (void)arg;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BOOT_GUARD_POLL_MS));

        uint32_t state = ESP_OTA_IMG_UNDEFINED;
        esp_err_t err = boot_guard_read_ota_state(&state);
        if (err == ESP_OK && state != ESP_OTA_IMG_PENDING_VERIFY) {
            /* Nothing to confirm (image already valid, or no rollback armed). */
            s_info.rollback_pending = false;
            break;
        }

        const int64_t uptime_ms = esp_timer_get_time() / 1000;
        const bool wifi_ok = wifi_mgr_is_connected();
        const bool uptime_ok = uptime_ms >= APP_BOOT_CONFIRM_TIMEOUT_MS;
        if (!wifi_ok && !uptime_ok) {
            if (uptime_ms >= APP_BOOT_CONFIRM_TIMEOUT_MS / 2) {
                ESP_LOGW(TAG_APP,
                         "rollback confirmation still waiting (wifi=%d, uptime=%llds)",
                         (int)wifi_ok,
                         (long long)(uptime_ms / 1000));
            }
            continue;
        }

        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            s_confirmed = true;
            s_info.confirmed = true;
            s_info.rollback_pending = false;
            ESP_LOGI(TAG_APP,
                     "firmware confirmed (wifi=%d, uptime=%llds): OTA rollback cancelled",
                     (int)wifi_ok,
                     (long long)(uptime_ms / 1000));
            break;
        }

        s_info.confirm_failed = true;
        ESP_LOGE(TAG_APP, "failed to confirm firmware: %s (wifi=%d)", esp_err_to_name(err), (int)wifi_ok);
        break;
    }

    vTaskDelete(NULL);
}

void boot_guard_start_confirm_task(void)
{
    if (!s_initialized) {
        boot_guard_init();
    }
    if (s_confirmed || !s_info.rollback_pending) {
        return;
    }
    if (xTaskCreate(boot_guard_confirm_task, "boot_guard", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG_APP, "Failed to create boot-guard task");
    }
}

void boot_guard_get_info(boot_guard_info_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_info;
    out->confirmed = s_confirmed;
    if (s_info.ota_state_err != ESP_OK) {
        out->ota_state_err = boot_guard_read_ota_state(&out->ota_state);
    }
}

const char *boot_guard_reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_UNKNOWN:
        return "unknown";
    case ESP_RST_POWERON:
        return "power_on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt_wdt";
    case ESP_RST_TASK_WDT:
        return "task_wdt";
    case ESP_RST_WDT:
        return "wdt";
    case ESP_RST_DEEPSLEEP:
        return "deep_sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    case ESP_RST_USB:
        return "usb";
    case ESP_RST_JTAG:
        return "jtag";
    case ESP_RST_EFUSE:
        return "efuse";
    case ESP_RST_PWR_GLITCH:
        return "power_glitch";
    case ESP_RST_CPU_LOCKUP:
        return "cpu_lockup";
    default:
        break;
    }
    return "other";
}

const char *boot_guard_ota_state_str(uint32_t state)
{
    switch ((esp_ota_img_states_t)state) {
    case ESP_OTA_IMG_NEW:
        return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
        return "pending_verify";
    case ESP_OTA_IMG_VALID:
        return "valid";
    case ESP_OTA_IMG_INVALID:
        return "invalid";
    case ESP_OTA_IMG_ABORTED:
        return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
        break;
    default:
        break;
    }
    return "undefined";
}
