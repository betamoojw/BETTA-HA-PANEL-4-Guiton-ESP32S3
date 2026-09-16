/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "ui/ui_lvgl_mem.h"

#include <string.h>

#include "esp_heap_caps.h"

/* LVGL allocation backend for the Guiton 4 panels.
 *
 * LVGL creates hundreds of small objects per screen (18 tiles, styles, labels,
 * animations). With the stock CLIB backend those went through heap_caps_malloc()
 * on the default heap, and because CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024 every
 * block below 1 KB landed in internal DRAM. That exhausted the ~160 KB of free
 * internal RAM and fragmented it badly, which is what made long uptimes unstable.
 *
 * This backend keeps the same malloc/free semantics but asks for PSRAM first and
 * only falls back to the default heap when PSRAM is really out of space. The RGB
 * panel draws through PSRAM framebuffers, so no LVGL buffer needs internal RAM. */

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

#define LVGL_MEM_PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LVGL_MEM_FALLBACK_CAPS (MALLOC_CAP_8BIT)

static ui_lvgl_mem_stats_t s_stats;

static void lvgl_mem_account_alloc(size_t real_size)
{
    __atomic_add_fetch(&s_stats.used_bytes, real_size, __ATOMIC_RELAXED);
    __atomic_add_fetch(&s_stats.alloc_count, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&s_stats.block_count, 1, __ATOMIC_RELAXED);

    size_t peak = __atomic_load_n(&s_stats.peak_bytes, __ATOMIC_RELAXED);
    size_t used = __atomic_load_n(&s_stats.used_bytes, __ATOMIC_RELAXED);
    while (used > peak && !__atomic_compare_exchange_n(&s_stats.peak_bytes, &peak, used, false, __ATOMIC_RELAXED,
        __ATOMIC_RELAXED)) {
    }
}

static void lvgl_mem_account_free(size_t real_size)
{
    size_t used = __atomic_load_n(&s_stats.used_bytes, __ATOMIC_RELAXED);
    if (real_size <= used) {
        __atomic_sub_fetch(&s_stats.used_bytes, real_size, __ATOMIC_RELAXED);
    } else {
        __atomic_store_n(&s_stats.used_bytes, (size_t)0, __ATOMIC_RELAXED);
    }
    __atomic_sub_fetch(&s_stats.block_count, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&s_stats.free_count, 1, __ATOMIC_RELAXED);
}

void ui_lvgl_mem_get_stats(ui_lvgl_mem_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    out->used_bytes = __atomic_load_n(&s_stats.used_bytes, __ATOMIC_RELAXED);
    out->peak_bytes = __atomic_load_n(&s_stats.peak_bytes, __ATOMIC_RELAXED);
    out->block_count = __atomic_load_n(&s_stats.block_count, __ATOMIC_RELAXED);
    out->alloc_count = __atomic_load_n(&s_stats.alloc_count, __ATOMIC_RELAXED);
    out->free_count = __atomic_load_n(&s_stats.free_count, __ATOMIC_RELAXED);
    out->fail_count = __atomic_load_n(&s_stats.fail_count, __ATOMIC_RELAXED);
}

void ui_lvgl_mem_reset_peak(void)
{
    __atomic_store_n(&s_stats.peak_bytes, __atomic_load_n(&s_stats.used_bytes, __ATOMIC_RELAXED), __ATOMIC_RELAXED);
}

/* ── LVGL backend contract ──────────────────────────────────────────────── */

void lv_mem_init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

void lv_mem_deinit(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    /* Pool allocator is not used here; the panel heap does the work. */
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void *lv_malloc_core(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = heap_caps_malloc_prefer(size, 2, LVGL_MEM_PSRAM_CAPS, LVGL_MEM_FALLBACK_CAPS);
    if (ptr == NULL) {
        __atomic_add_fetch(&s_stats.fail_count, 1, __ATOMIC_RELAXED);
        return NULL;
    }

    lvgl_mem_account_alloc(heap_caps_get_allocated_size(ptr));
    return ptr;
}

void lv_free_core(void *p)
{
    if (p == NULL) {
        return;
    }
    lvgl_mem_account_free(heap_caps_get_allocated_size(p));
    heap_caps_free(p);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    if (p == NULL) {
        return lv_malloc_core(new_size);
    }
    if (new_size == 0) {
        lv_free_core(p);
        return NULL;
    }

    /* Copy-realloc keeps the PSRAM preference whatever heap the old block came
     * from, and avoids asking the heap layer to move a block between regions. */
    void *moved = lv_malloc_core(new_size);
    if (moved == NULL) {
        return NULL;
    }

    size_t old_size = heap_caps_get_allocated_size(p);
    memcpy(moved, p, (old_size < new_size) ? old_size : new_size);
    lv_free_core(p);
    return moved;
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    if (mon_p == NULL) {
        return;
    }

    /* LVGL shares the PSRAM heap with the framebuffers and the JSON parser, so
     * the free/biggest figures describe PSRAM as a whole while the used/peak
     * figures come from the LVGL tracker above. */
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);

    size_t total = info.total_free_bytes + info.total_allocated_bytes;
    mon_p->total_size = total;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->free_cnt = info.free_blocks;
    mon_p->used_cnt = __atomic_load_n(&s_stats.block_count, __ATOMIC_RELAXED);
    mon_p->max_used = __atomic_load_n(&s_stats.peak_bytes, __ATOMIC_RELAXED);
    mon_p->used_pct = (total > 0) ? (uint8_t)((info.total_allocated_bytes * 100) / total) : 0;
    mon_p->frag_pct = (info.total_free_bytes > 0)
        ? (uint8_t)(100 - ((info.largest_free_block * 100) / info.total_free_bytes))
        : 0;
}

lv_result_t lv_mem_test_core(void)
{
    void *probe = lv_malloc_core(64);
    if (probe == NULL) {
        return LV_RESULT_INVALID;
    }

    memset(probe, 0xA5, 64);
    bool intact = true;
    const uint8_t *bytes = (const uint8_t *)probe;
    for (size_t i = 0; i < 64; i++) {
        if (bytes[i] != 0xA5) {
            intact = false;
            break;
        }
    }

    lv_free_core(probe);
    return intact ? LV_RESULT_OK : LV_RESULT_INVALID;
}

#else /* !LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM */

void ui_lvgl_mem_get_stats(ui_lvgl_mem_stats_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
}

void ui_lvgl_mem_reset_peak(void)
{
}

#endif
