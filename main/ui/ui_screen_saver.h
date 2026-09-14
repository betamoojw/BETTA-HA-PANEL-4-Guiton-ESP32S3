/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

#include "settings/runtime_settings.h"

void ui_screen_saver_init(void);
void ui_screen_saver_apply_settings(const runtime_settings_t *settings);

/* Wake the panel to the screensaver clock (used by MQTT/HA "wake" command and
 * motion sensors). A real touch dismisses the clock and opens the full UI. */
void ui_screen_saver_wake(void);

/* Reload the screensaver wallpaper from littlefs (called after upload/delete). */
void ui_screen_saver_reload_wallpaper(void);
