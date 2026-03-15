/**
 * @file flash_storage.h
 * @brief Generic batched binary log driver for embedded Linux (Luckfox Pico)
 *
 * Writes fixed-size, opaque binary records to the writable flash filesystem.
 * Records are batched in RAM to minimise flash wear; a new binary file is
 * created every time the current one reaches max_file_size, and the oldest
 * file is deleted once max_files is exceeded.
 *
 * The driver is record-type agnostic: it operates on blobs of record_size
 * bytes.  Domain-specific structs and filtering logic belong in the layer
 * above (e.g. uav_detection_log).
 *
 * Binary file layout
 * ------------------
 *   [flash_file_header_t]
 *   [record_size bytes] * N  (appended sequentially)
 *
 * Usage
 * -----
 *   flash_storage_config_t cfg = {
 *       .dir          = "/userdata/my_logs",
 *       .file_prefix  = "data",
 *       .record_size  = sizeof(my_record_t),   // REQUIRED
 *       .batch_size   = 32,
 *       .max_file_size = 1 * 1024 * 1024,
 *       .max_files    = 8,
 *   };
 *   flash_storage_init(&cfg);
 *
 *   // inside event loop:
 *   flash_storage_write(&my_record);
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

/** Flush the RAM batch buffer to disk after this many records. */
#define FLASH_STORAGE_DEFAULT_BATCH           32u

/** Start a new log file once the current one reaches this size (bytes). */
#define FLASH_STORAGE_DEFAULT_MAX_FILE_SIZE   (1u * 1024u * 1024u)  /* 1 MiB */

/** Delete the oldest file once this many log files exist. */
#define FLASH_STORAGE_DEFAULT_MAX_FILES       8u

/** Magic number written at the start of every log file ("UAVD" — kept for format compatibility). */
#define FLASH_STORAGE_MAGIC    0x55415644u

/** Binary format version. */
#define FLASH_STORAGE_VERSION  1u

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

#pragma pack(pop)

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *dir;           /**< Directory to store log files                    */
    const char *file_prefix;   /**< Filename prefix (prefix_YYYYMMDD_…bin)          */
    uint32_t    record_size;   /**< Size of one record in bytes — MUST be non-zero  */
    uint32_t    batch_size;    /**< Records to buffer in RAM before flush            */
    uint32_t    max_file_size; /**< Bytes per file before rotation                  */
    uint32_t    max_files;     /**< Maximum number of log files to keep             */
} flash_storage_config_t;

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
 * most recent log file, and allocates the write batch buffer.
 * cfg->record_size must be > 0.
 *
 * @param cfg  Pointer to configuration.  Must not be NULL.
 * @return  0 on success, -1 on error.
 */
int flash_storage_init(const flash_storage_config_t *cfg);

/**
 * @brief Append one record to the in-RAM batch buffer.
 *
 * Automatically flushes the buffer to disk when it reaches batch_size,
 * and rotates the file when it would exceed max_file_size.
 *
 * @param record  Pointer to a record of exactly record_size bytes.
 * @return 0 on success, -1 on error.
 */
int flash_storage_write(const void *record);

/**
 * @brief Flush the in-RAM batch buffer to disk immediately.
 * @return 0 on success, -1 on error.
 */
int flash_storage_flush(void);

/**
 * @brief Read records from a specific log file into a caller-allocated buffer.
 *
 * @param file_path  Absolute path to a .bin log file.
 * @param buf        Buffer to fill (must be at least rec_size * buf_count bytes).
 * @param rec_size   Size of one record in bytes.
 * @param buf_count  Maximum number of records to read.
 * @return Number of records read, or -1 on error.
 */
int flash_storage_read_file(const char *file_path,
                            void *buf, size_t rec_size, size_t buf_count);

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
