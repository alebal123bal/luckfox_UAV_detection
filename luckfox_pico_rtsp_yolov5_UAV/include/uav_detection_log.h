/**
 * @file uav_detection_log.h
 * @brief UAV detection logging — application layer on top of flash_storage.
 *
 * Owns the on-disk detection record format and all UAV-specific filtering
 * logic (confidence threshold, frame decimation).  Delegates all file I/O
 * to the generic flash_storage driver.
 *
 * Usage
 * -----
 *   uav_detection_log_config_t cfg = UAV_DETECTION_LOG_DEFAULT_CONFIG;
 *   uav_detection_log_init(&cfg);
 *
 *   // inside detection loop:
 *   uav_detection_log_record(sX, sY, w, h, det->prop, det->cls_id, i, W, H);
 *
 *   // on shutdown:
 *   uav_detection_log_deinit();
 */

#ifndef UAV_DETECTION_LOG_H
#define UAV_DETECTION_LOG_H

#include "flash_storage.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Defaults                                                            */
/* ------------------------------------------------------------------ */

#define UAV_DETECTION_LOG_DEFAULT_DIR           "/userdata/uav_detections"
#define UAV_DETECTION_LOG_DEFAULT_FILE_PREFIX   "detections"
#define UAV_DETECTION_LOG_DEFAULT_BATCH         32u
#define UAV_DETECTION_LOG_DEFAULT_MAX_FILE_SIZE (1u * 1024u * 1024u)
#define UAV_DETECTION_LOG_DEFAULT_MAX_FILES     8u
#define UAV_DETECTION_LOG_DEFAULT_MIN_CONFIDENCE 0.5f
#define UAV_DETECTION_LOG_DEFAULT_DECIMATE_N    5u

/* ------------------------------------------------------------------ */
/*  On-disk record                                                      */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)

/**
 * Single detection record stored on disk.
 * Size: 32 bytes (naturally aligned, no hidden compiler padding with pack(1)).
 */
typedef struct {
    uint64_t timestamp_us;  /**< µs since Unix epoch at detection time */
    int32_t  x;             /**< Bounding box left edge (pixels)       */
    int32_t  y;             /**< Bounding box top edge (pixels)        */
    int32_t  w;             /**< Bounding box width (pixels)           */
    int32_t  h;             /**< Bounding box height (pixels)          */
    float    confidence;    /**< Detection confidence [0.0 – 1.0]      */
    uint16_t frame_width;   /**< Source frame width (pixels)           */
    uint16_t frame_height;  /**< Source frame height (pixels)          */
    uint8_t  class_id;      /**< YOLOv5 class index                    */
    uint8_t  target_num;    /**< Index within current frame            */
    uint8_t  _pad[2];
} uav_detection_record_t;

#pragma pack(pop)

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *dir;           /**< Directory to store log files                    */
    const char *file_prefix;   /**< Filename prefix (prefix_YYYYMMDD_…bin)          */
    uint32_t    batch_size;    /**< Records to buffer in RAM before flush            */
    uint32_t    max_file_size; /**< Bytes per file before rotation                  */
    uint32_t    max_files;     /**< Maximum number of log files to keep             */
    float       min_confidence;/**< Drop detections below this score [0–1]          */
    uint32_t    decimate_n;    /**< Save 1 out of every N detections (1 = off)      */
} uav_detection_log_config_t;

#define UAV_DETECTION_LOG_DEFAULT_CONFIG {          \
    UAV_DETECTION_LOG_DEFAULT_DIR,                  \
    UAV_DETECTION_LOG_DEFAULT_FILE_PREFIX,          \
    UAV_DETECTION_LOG_DEFAULT_BATCH,                \
    UAV_DETECTION_LOG_DEFAULT_MAX_FILE_SIZE,        \
    UAV_DETECTION_LOG_DEFAULT_MAX_FILES,            \
    UAV_DETECTION_LOG_DEFAULT_MIN_CONFIDENCE,       \
    UAV_DETECTION_LOG_DEFAULT_DECIMATE_N            \
}

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the UAV detection log.
 *
 * Configures and starts the underlying flash_storage driver.
 *
 * @param cfg  Pointer to configuration, or NULL to use all defaults.
 * @return 0 on success, -1 on error.
 */
int uav_detection_log_init(const uav_detection_log_config_t *cfg);

/**
 * @brief Record one detection, applying confidence filtering and decimation.
 *
 * Detections below min_confidence are silently dropped.
 * Only 1 out of every decimate_n qualifying detections is actually saved.
 *
 * @param x, y         Top-left corner of bounding box in pixels.
 * @param w, h         Width and height of bounding box in pixels.
 * @param confidence   Detection confidence [0.0 – 1.0].
 * @param class_id     YOLOv5 class index.
 * @param target_num   Index of this detection within the current frame.
 * @param frame_width  Width of the source frame in pixels.
 * @param frame_height Height of the source frame in pixels.
 * @return 0 on success, -1 on error.
 */
int uav_detection_log_record(int x, int y, int w, int h,
                             float confidence,
                             uint8_t class_id, uint8_t target_num,
                             int frame_width, int frame_height);

/**
 * @brief Flush the in-RAM batch buffer to disk immediately.
 * @return 0 on success, -1 on error.
 */
int uav_detection_log_flush(void);

/**
 * @brief Populate a stats struct with current runtime counters.
 * @param stats  Output statistics (must not be NULL).
 */
void uav_detection_log_get_stats(flash_storage_stats_t *stats);

/**
 * @brief Delete all log files in the storage directory.
 * @return 0 on success, -1 on error.
 */
int uav_detection_log_clear(void);

/**
 * @brief Flush pending writes, close the log, and free all resources.
 */
void uav_detection_log_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* UAV_DETECTION_LOG_H */
