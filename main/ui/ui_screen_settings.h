/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

void ui_screen_settings_open(void);

/* Must be called by ui_pages_init() before it cleans the active screen: the
 * settings page is a child of that screen and is deleted by the clean-up, so
 * the cached handles have to be dropped to avoid deleting freed objects later. */
void ui_screen_settings_handle_screen_clean(void);
