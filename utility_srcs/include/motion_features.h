/**
 * motion_features.h
 *
 * Shared on-disk record types and motion-feature computation API.
 *
 * The struct layout (flash_file_header_t, flash_detection_record_t) must stay
 * in sync with flash_storage.h / uav_detection_log.h in the main project.
 *
 * compute_motion_features() accepts a flat, chronologically ordered array of
 * raw detection records, groups them by target_num, computes per-sample
 * centroid and inter-frame velocity, applies a sliding-window average, and
 * returns one motion_feature_t per unique target.
 */

#ifndef MOTION_FEATURES_H
#define MOTION_FEATURES_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */

#define FLASH_STORAGE_MAGIC   0x55415644u
#define FLASH_STORAGE_VERSION 1u

/** Number of most-recent samples used for the smoothing window. */
#define MOTION_WINDOW         20

/* ------------------------------------------------------------------ */
/*  On-disk structures (must match flash_storage.h exactly)            */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  _pad[3];
    uint64_t created_us;
} flash_file_header_t;

typedef struct {
    uint64_t timestamp_us;
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
    float    confidence;
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t  class_id;
    uint8_t  target_num;
    uint8_t  _pad[2];
} flash_detection_record_t;

#pragma pack(pop)

/* ------------------------------------------------------------------ */
/*  Motion feature output                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t timestamp_us;    /**< Timestamp of the most recent sample in the window    */
    double  nx, ny;           /**< Normalized centroid [0..1] (latest sample in window) */
    double  avg_vx, avg_vy;   /**< Smoothed velocity  (px/s)                            */
    double  avg_speed;        /**< Smoothed speed     (px/s)                            */
    double  avg_heading_rad;  /**< atan2(avg_vy, avg_vx)                                */
    double  avg_confidence;   /**< Mean confidence over the window                      */
    long    samples;          /**< Number of samples in the window actually used        */
    uint8_t target_id;        /**< target_num from the raw records                      */
} motion_feature_t;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Compute a decimated motion-feature time series for each unique target_num.
 *
 * Steps through each target's track in non-overlapping windows of MOTION_WINDOW
 * samples.  One motion_feature_t is emitted per window, so the output count is
 * ceil(N / MOTION_WINDOW) per target (e.g. 235 records → 47 entries at window=5).
 * The timestamp of each entry is that of the last sample in the window.
 *
 * @param records  Flat array of raw detection records.
 * @param count    Number of records in the array.
 * @param n_out    Output: number of entries in the returned array.
 * @return  Heap-allocated array of motion_feature_t (caller must free),
 *          or NULL if count == 0 or on allocation failure.
 */
motion_feature_t *compute_motion_features(
    const flash_detection_record_t *records,
    long count,
    int *n_out);

#endif /* MOTION_FEATURES_H */
