/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t display_init(void);
bool display_is_ready(void);
bool display_lock(uint32_t timeout_ms);
void display_unlock(void);
esp_err_t display_set_brightness_percent(int percent);
/* Set the brightness used when activity wakes the panel (e.g. on touch or an
 * MQTT wake/brightness command). Defaults to APP_DISPLAY_ACTIVE_BRIGHTNESS_PERCENT
 * and should be kept in sync with the user's configured brightness. */
void display_set_active_brightness_percent(int percent);
void display_note_activity(void);

typedef void (*display_activity_cb_t)(void);
void display_set_activity_callback(display_activity_cb_t cb);
int64_t display_ms_since_activity(void);
