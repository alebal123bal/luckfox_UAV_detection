/**
 * @file flash_storage.h
 * @brief Persistent detection log storage for Luckfox Pico (Buildroot Linux)
 *
 * Stores YOLOv5 detection records to the writable flash filesystem
 * (typically /userdata on Luckfox Pico).  Writes are batched in RAM to
 * minimise flash wear; a new binary file is created every time the current
 * one reaches FLASH_STORAGE_MAX_FILE_SIZE, and the oldest file is deleted
 * once FLASH_STORAGE_MAX_FILES is exceeded.
 *
 * Binary file layout
 * ------------------
 *   [flash_file_header_t]
 *   [flash_detection_record_t] * N  (appended sequentially)
 *
 * Usage
 * -----
 *   flash_storage_config_t cfg = FLASH_STORAGE_DEFAULT_CONFIG;
 *   flash_storage_init(&cfg);
 *
 *   // inside detection loop:
 *   flash_storage_record(sX, sY, eX-sX, eY-sY,
 *                        det->prop, det->cls_id, i, width, height);
 *
 *   // on shutdown:
 *   flash_storage_deinit();
 */

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */

/** Default directory on the writable /userdata partition (Luckfox Pico). */
#define FLASH_STORAGE_DEFAULT_DIR     "/userdata/uav_detections"

/** Default prefix for log filenames (prefix_YYYYMMDD_HHMMSS_<us>.bin). */
#define FLASH_STORAGE_DEFAULT_FILE_PREFIX  "detections"

/** Flush the RAM batch buffer to disk after this many records. */
#define FLASH_STORAGE_DEFAULT_BATCH   32

/** Start a new log file once the current one reaches this size (bytes). */
#define FLASH_STORAGE_DEFAULT_MAX_FILE_SIZE   (1 * 1024 * 1024)  /* 1 MiB */

/** Delete the oldest file once this many log files exist. */
#define FLASH_STORAGE_DEFAULT_MAX_FILES  8

/** Detections with confidence strictly below this value are silently dropped. */
#define FLASH_STORAGE_DEFAULT_MIN_CONFIDENCE  0.6f

/**
 * Save only 1 out of every N detections that pass the confidence filter.
 * Set to 1 to disable decimation (save every detection).
 */
#define FLASH_STORAGE_DEFAULT_DECIMATE_N  5u

/** Magic number written at the start of every log file ("UAVD"). */
#define FLASH_STORAGE_MAGIC   0x55415644u

/** Binary format version. */
#define FLASH_STORAGE_VERSION 1u

/* ------------------------------------------------------------------ */
/*  On-disk structures                                                  */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)

/** Written once at the beginning of each binary log file. */
typedef struct {
    uint32_t magic;          /**< FLASH_STORAGE_MAGIC                  */
    uint8_t  version;        /**< FLASH_STORAGE_VERSION                 */
    uint8_t  _pad[3];
    uint64_t created_us;     /**< File creation time (µs since epoch)  */
} flash_file_header_t;

/**
 * Single detection record stored on disk.
 * Size: 32 bytes (naturally aligned, no hidden compiler padding with pack(1)).
 */
typedef struct {
    uint64_t timestamp_us;   /**< µs since Unix epoch at detection time */
    int32_t  x;              /**< Bounding box left edge (pixels)       */
    int32_t  y;              /**< Bounding box top edge (pixels)        */
    int32_t  w;              /**< Bounding box width (pixels)           */
    int32_t  h;              /**< Bounding box height (pixels)          */
    float    confidence;     /**< Detection confidence [0.0 – 1.0]      */
    uint16_t frame_width;    /**< Source frame width (pixels)           */
    uint16_t frame_height;   /**< Source frame height (pixels)          */
    uint8_t  class_id;       /**< YOLOv5 class index                    */
    uint8_t  target_num;     /**< Index within current frame            */
    uint8_t  _pad[2];
} flash_detection_record_t;

#pragma pack(pop)

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *dir;          /**< Directory to store log files          */
    const char *file_prefix;  /**< Filename prefix (prefix_YYYYMMDD_…bin) */
    uint32_t    batch_size;   /**< Records to buffer in RAM before flush */
    uint32_t    max_file_size;/**< Bytes per file before rotation        */
    uint32_t    max_files;    /**< Maximum number of log files to keep   */
    float       min_confidence;   /**< Drop detections below this score [0–1]    */
    uint32_t    decimate_n;       /**< Save 1 out of every N detections (1=off)  */
} flash_storage_config_t;

/** Initialise a config struct with sensible defaults. */
#define FLASH_STORAGE_DEFAULT_CONFIG { \
    FLASH_STORAGE_DEFAULT_DIR,           \
    FLASH_STORAGE_DEFAULT_FILE_PREFIX,   \
    FLASH_STORAGE_DEFAULT_BATCH,         \
    FLASH_STORAGE_DEFAULT_MAX_FILE_SIZE, \
    FLASH_STORAGE_DEFAULT_MAX_FILES,     \
    FLASH_STORAGE_DEFAULT_MIN_CONFIDENCE,\
    FLASH_STORAGE_DEFAULT_DECIMATE_N     \
}

/* ------------------------------------------------------------------ */
/*  Runtime statistics                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t total_records_written; /**< Lifetime records flushed to disk */
    uint32_t total_flushes;         /**< Number of flush operations        */
    uint32_t current_file_records;  /**< Records in the current open file  */
    uint32_t num_files;             /**< Log files currently on disk       */
    uint32_t records_in_buffer;     /**< Records pending in RAM buffer     */
} flash_storage_stats_t;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the storage subsystem.
 *
 * Creates the directory if it does not exist, opens (or creates) the
 * most recent log file, and loads the pending-write batch buffer.
 *
 * @param cfg  Pointer to configuration, or NULL to use defaults.
 * @return  0 on success, -1 on error (errno is set).
 */
int flash_storage_init(const flash_storage_config_t *cfg);

/**
 * @brief Append one detection to the in-RAM batch buffer.
 *
 * Automatically flushes the buffer to disk when it reaches
 * cfg.batch_size, and rotates the file when it would exceed
 * cfg.max_file_size.
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
int flash_storage_record(int x, int y, int w, int h,
                         float confidence,
                         uint8_t class_id, uint8_t target_num,
                         int frame_width, int frame_height);

/**
 * @brief Flush the in-RAM batch buffer to disk immediately.
 * @return 0 on success, -1 on error.
 */
int flash_storage_flush(void);

/**
 * @brief Read a specific log file into a caller-allocated array.
 *
 * @param file_path  Absolute path to a .bin log file.
 * @param buf        Array to fill with records.
 * @param buf_len    Maximum number of records that fit in buf.
 * @return Number of records read, or -1 on error.
 */
int flash_storage_read_file(const char *file_path,
                            flash_detection_record_t *buf, size_t buf_len);

/**
 * @brief Populate a stats struct with current runtime counters.
 * @param stats  Output statistics (must not be NULL).
 */
void flash_storage_get_stats(flash_storage_stats_t *stats);

/**
 * @brief Delete all log files in the storage directory.
 * @return 0 on success, -1 on error.
 */
int flash_storage_clear(void);

/**
 * @brief Flush pending writes, close the current log file, and free
 *        all resources.  Safe to call even if init was never called.
 */
void flash_storage_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_H */
