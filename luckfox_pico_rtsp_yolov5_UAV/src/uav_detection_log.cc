/**
 * @file uav_detection_log.cc
 * @brief UAV detection logging — application layer on top of flash_storage.
 *
 * Handles confidence filtering and count-based decimation before delegating
 * the actual write to the generic flash_storage driver.
 */

#include "uav_detection_log.h"
#include "flash_storage.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Internal state                                                      */
/* ------------------------------------------------------------------ */

static float    s_min_confidence   = UAV_DETECTION_LOG_DEFAULT_MIN_CONFIDENCE;
static uint32_t s_decimate_n       = UAV_DETECTION_LOG_DEFAULT_DECIMATE_N;
static uint32_t s_decimate_counter = 0;
static int      s_initialised      = 0;

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint64_t now_us_epoch(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

int uav_detection_log_init(const uav_detection_log_config_t *cfg)
{
    flash_storage_config_t fs_cfg;
    fs_cfg.dir          = (cfg && cfg->dir)         ? cfg->dir         : UAV_DETECTION_LOG_DEFAULT_DIR;
    fs_cfg.file_prefix  = (cfg && cfg->file_prefix) ? cfg->file_prefix : UAV_DETECTION_LOG_DEFAULT_FILE_PREFIX;
    fs_cfg.record_size  = sizeof(uav_detection_record_t);
    fs_cfg.batch_size   = (cfg && cfg->batch_size)      ? cfg->batch_size      : UAV_DETECTION_LOG_DEFAULT_BATCH;
    fs_cfg.max_file_size = (cfg && cfg->max_file_size)  ? cfg->max_file_size   : UAV_DETECTION_LOG_DEFAULT_MAX_FILE_SIZE;
    fs_cfg.max_files    = (cfg && cfg->max_files)       ? cfg->max_files       : UAV_DETECTION_LOG_DEFAULT_MAX_FILES;

    s_min_confidence   = cfg ? cfg->min_confidence : UAV_DETECTION_LOG_DEFAULT_MIN_CONFIDENCE;
    s_decimate_n       = (cfg && cfg->decimate_n > 0) ? cfg->decimate_n : UAV_DETECTION_LOG_DEFAULT_DECIMATE_N;
    s_decimate_counter = 0;

    int ret = flash_storage_init(&fs_cfg);
    if (ret == 0)
        s_initialised = 1;
    return ret;
}

/* ------------------------------------------------------------------ */

int uav_detection_log_record(int x, int y, int w, int h,
                             float confidence,
                             uint8_t class_id, uint8_t target_num,
                             int frame_width, int frame_height)
{
    if (!s_initialised) return -1;
    if (confidence < s_min_confidence) return 0;  /* below threshold — silently drop */

    /* Count-based decimation: save only every Nth qualifying detection. */
    s_decimate_counter++;
    if (s_decimate_counter < s_decimate_n)
        return 0;
    s_decimate_counter = 0;

    uav_detection_record_t r;
    memset(&r, 0, sizeof(r));
    r.timestamp_us = now_us_epoch();
    r.x            = x;
    r.y            = y;
    r.w            = w;
    r.h            = h;
    r.confidence   = confidence;
    r.class_id     = class_id;
    r.target_num   = target_num;
    r.frame_width  = (uint16_t)frame_width;
    r.frame_height = (uint16_t)frame_height;

    return flash_storage_write(&r);
}

/* ------------------------------------------------------------------ */

int uav_detection_log_flush(void)
{
    return flash_storage_flush();
}

/* ------------------------------------------------------------------ */

void uav_detection_log_get_stats(flash_storage_stats_t *stats)
{
    flash_storage_get_stats(stats);
}

/* ------------------------------------------------------------------ */

int uav_detection_log_clear(void)
{
    return flash_storage_clear();
}

/* ------------------------------------------------------------------ */

void uav_detection_log_deinit(void)
{
    flash_storage_deinit();
    s_initialised      = 0;
    s_decimate_counter = 0;
}
