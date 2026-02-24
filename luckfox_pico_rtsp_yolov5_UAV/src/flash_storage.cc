/**
 * @file flash_storage.cc
 * @brief Persistent detection log storage for Luckfox Pico (Buildroot Linux)
 *
 * See flash_storage.h for the public API and design notes.
 */

#include "flash_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/*  Internal state                                                      */
/* ------------------------------------------------------------------ */

/** Maximum path length used internally. */
#define FS_PATH_MAX 256

/** Extension used for log files. */
#define FS_FILE_EXT ".bin"

/** Separator between directory path and filename in logs. */
#define FS_SEP "/"

/* Internal config copy (set by flash_storage_init). */
static char           s_dir[FS_PATH_MAX]  = {0};
static uint32_t       s_batch_size        = FLASH_STORAGE_DEFAULT_BATCH;
static uint32_t       s_max_file_size     = FLASH_STORAGE_DEFAULT_MAX_FILE_SIZE;
static uint32_t       s_max_files         = FLASH_STORAGE_DEFAULT_MAX_FILES;

/* Current open log file. */
static int            s_fd                = -1;
static char           s_current_path[FS_PATH_MAX] = {0};
static uint32_t       s_current_file_records = 0;

/* In-RAM write batch. */
static flash_detection_record_t *s_batch  = NULL;
static uint32_t       s_batch_count       = 0;

/* Runtime statistics. */
static uint64_t       s_total_records     = 0;
static uint32_t       s_total_flushes     = 0;
static uint32_t       s_num_files         = 0;

/* Thread safety. */
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Guard against double-init. */
static int            s_initialised       = 0;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/** Return current time in microseconds since Unix epoch. */
static uint64_t now_us_epoch(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

/** Recursively create directories (equivalent to mkdir -p). */
static int mkdir_p(const char *path)
{
    char tmp[FS_PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/** Build a filename: <dir>/detections_YYYYMMDD_HHMMSS_<us>.bin */
static void make_filename(char *out, size_t out_sz, uint64_t ts_us)
{
    time_t secs = (time_t)(ts_us / 1000000ULL);
    struct tm t;
    gmtime_r(&secs, &t);
    snprintf(out, out_sz,
             "%s" FS_SEP "detections_%04d%02d%02d_%02d%02d%02d_%06llu" FS_FILE_EXT,
             s_dir,
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec,
             (unsigned long long)(ts_us % 1000000ULL));
}

/** Compare function for qsort on (char *) filenames. */
static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * Count and optionally collect .bin filenames in s_dir.
 * Caller must free each string in names[] and names itself.
 * Returns the number of .bin files found, or -1 on error.
 */
static int list_bin_files(char ***names_out)
{
    DIR *dp = opendir(s_dir);
    if (!dp) return -1;

    /* First pass: count. */
    int count = 0;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        size_t nl = strlen(de->d_name);
        size_t el = strlen(FS_FILE_EXT);
        if (nl > el &&
            strcmp(de->d_name + nl - el, FS_FILE_EXT) == 0)
            count++;
    }

    if (names_out == NULL || count == 0) {
        closedir(dp);
        return count;
    }

    char **names = (char **)malloc((size_t)count * sizeof(char *));
    if (!names) { closedir(dp); return -1; }

    rewinddir(dp);
    int idx = 0;
    while ((de = readdir(dp)) != NULL && idx < count) {
        size_t nl = strlen(de->d_name);
        size_t el = strlen(FS_FILE_EXT);
        if (nl > el &&
            strcmp(de->d_name + nl - el, FS_FILE_EXT) == 0) {
            char full[FS_PATH_MAX];
            snprintf(full, sizeof(full), "%s" FS_SEP "%s", s_dir, de->d_name);
            names[idx++] = strdup(full);
        }
    }
    closedir(dp);

    /* Sort lexicographically (filenames embed timestamp, so this = chrono). */
    qsort(names, (size_t)idx, sizeof(char *), cmp_str);

    *names_out = names;
    return idx;
}

/** Delete the oldest log files until only max_files remain. */
static void enforce_max_files(void)
{
    char **names = NULL;
    int n = list_bin_files(&names);
    if (n <= 0) return;

    int to_delete = n - (int)s_max_files;
    for (int i = 0; i < to_delete && i < n; i++) {
        unlink(names[i]);
        s_num_files = (s_num_files > 0) ? s_num_files - 1 : 0;
    }

    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
}

/**
 * Open a brand-new log file and write its header.
 * Closes the previously open file (if any) first.
 */
static int open_new_file(void)
{
    if (s_fd >= 0) {
        fsync(s_fd);
        close(s_fd);
        s_fd = -1;
    }

    uint64_t ts = now_us_epoch();
    make_filename(s_current_path, sizeof(s_current_path), ts);

    s_fd = open(s_current_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (s_fd < 0) {
        perror("[flash_storage] open_new_file: open");
        return -1;
    }

    flash_file_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic      = FLASH_STORAGE_MAGIC;
    hdr.version    = FLASH_STORAGE_VERSION;
    hdr.created_us = ts;

    if (write(s_fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
        perror("[flash_storage] open_new_file: write header");
        close(s_fd);
        s_fd = -1;
        return -1;
    }

    s_current_file_records = 0;
    s_num_files++;
    enforce_max_files();
    return 0;
}

/**
 * Re-open the newest existing log file for appending, or create one.
 * Called once during init.
 */
static int open_or_create_file(void)
{
    char **names  = NULL;
    int    n      = list_bin_files(&names);
    int    result = 0;

    if (n > 0) {
        /* Try to reuse the most recent file. */
        const char *newest = names[n - 1];

        /* Check it still has room. */
        struct stat st;
        if (stat(newest, &st) == 0 &&
            (uint32_t)st.st_size < s_max_file_size) {

            s_fd = open(newest, O_WRONLY | O_APPEND, 0644);
            if (s_fd >= 0) {
                strncpy(s_current_path, newest, FS_PATH_MAX - 1);
                /* Infer record count from file size. */
                off_t payload = st.st_size - (off_t)sizeof(flash_file_header_t);
                s_current_file_records =
                    (payload > 0)
                    ? (uint32_t)(payload / sizeof(flash_detection_record_t))
                    : 0;
                s_num_files = (uint32_t)n;
                goto out;
            }
        }
        s_num_files = (uint32_t)n;
    }

    /* Create a fresh file. */
    result = open_new_file();

out:
    if (names) {
        for (int i = 0; i < n; i++) free(names[i]);
        free(names);
    }
    return result;
}

/** Write s_batch[0..s_batch_count-1] to disk. NOT locked (caller holds lock). */
static int flush_locked(void)
{
    if (s_batch_count == 0)
        return 0;

    if (s_fd < 0) {
        fprintf(stderr, "[flash_storage] flush_locked: no open file\n");
        return -1;
    }

    /* Rotate if the write would overflow the current file. */
    size_t write_sz = s_batch_count * sizeof(flash_detection_record_t);
    struct stat st;
    if (fstat(s_fd, &st) == 0 &&
        (uint32_t)(st.st_size + (off_t)write_sz) >= s_max_file_size) {
        if (open_new_file() != 0)
            return -1;
    }

    ssize_t written = write(s_fd, s_batch, write_sz);
    if (written != (ssize_t)write_sz) {
        perror("[flash_storage] flush_locked: write");
        return -1;
    }

    /* fdatasync to push dirty pages to the flash device. */
    fdatasync(s_fd);

    s_total_records         += s_batch_count;
    s_current_file_records  += s_batch_count;
    s_total_flushes++;
    s_batch_count = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

int flash_storage_init(const flash_storage_config_t *cfg)
{
    pthread_mutex_lock(&s_mutex);

    if (s_initialised) {
        pthread_mutex_unlock(&s_mutex);
        return 0; /* already up */
    }

    /* Copy configuration. */
    const char *dir        = cfg ? cfg->dir           : FLASH_STORAGE_DEFAULT_DIR;
    s_batch_size           = cfg ? cfg->batch_size    : FLASH_STORAGE_DEFAULT_BATCH;
    s_max_file_size        = cfg ? cfg->max_file_size : FLASH_STORAGE_DEFAULT_MAX_FILE_SIZE;
    s_max_files            = cfg ? cfg->max_files     : FLASH_STORAGE_DEFAULT_MAX_FILES;

    strncpy(s_dir, dir, FS_PATH_MAX - 1);

    /* Ensure the storage directory exists. */
    if (mkdir_p(s_dir) != 0) {
        fprintf(stderr, "[flash_storage] Cannot create dir %s: %s\n",
                s_dir, strerror(errno));
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    /* Allocate the in-RAM batch buffer. */
    s_batch = (flash_detection_record_t *)malloc(
        s_batch_size * sizeof(flash_detection_record_t));
    if (!s_batch) {
        fprintf(stderr, "[flash_storage] malloc batch failed\n");
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }
    s_batch_count = 0;

    /* Open or create a log file. */
    if (open_or_create_file() != 0) {
        free(s_batch);
        s_batch = NULL;
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    s_total_records = 0;
    s_total_flushes = 0;
    s_initialised   = 1;

    printf("[flash_storage] Initialised. dir=%s batch=%u max_file=%u B max_files=%u\n",
           s_dir, s_batch_size, s_max_file_size, s_max_files);

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

/* ------------------------------------------------------------------ */

int flash_storage_record(int x, int y, int w, int h,
                         float confidence,
                         uint8_t class_id, uint8_t target_num,
                         int frame_width, int frame_height)
{
    if (!s_initialised) return -1;

    pthread_mutex_lock(&s_mutex);

    flash_detection_record_t *r = &s_batch[s_batch_count];
    memset(r, 0, sizeof(*r));
    r->timestamp_us  = now_us_epoch();
    r->x             = x;
    r->y             = y;
    r->w             = w;
    r->h             = h;
    r->confidence    = confidence;
    r->class_id      = class_id;
    r->target_num    = target_num;
    r->frame_width   = (uint16_t)frame_width;
    r->frame_height  = (uint16_t)frame_height;

    s_batch_count++;

    int ret = 0;
    if (s_batch_count >= s_batch_size)
        ret = flush_locked();

    pthread_mutex_unlock(&s_mutex);
    return ret;
}

/* ------------------------------------------------------------------ */

int flash_storage_flush(void)
{
    if (!s_initialised) return -1;
    pthread_mutex_lock(&s_mutex);
    int ret = flush_locked();
    pthread_mutex_unlock(&s_mutex);
    return ret;
}

/* ------------------------------------------------------------------ */

int flash_storage_export_csv(const char *out_path)
{
    if (!out_path) return -1;

    /* Flush pending records first. */
    flash_storage_flush();

    pthread_mutex_lock(&s_mutex);

    FILE *csv = fopen(out_path, "w");
    if (!csv) {
        fprintf(stderr, "[flash_storage] export_csv: cannot open %s: %s\n",
                out_path, strerror(errno));
        pthread_mutex_unlock(&s_mutex);
        return -1;
    }

    fprintf(csv, "timestamp_us,x,y,w,h,confidence,class_id,"
                 "target_num,frame_width,frame_height\n");

    char **names = NULL;
    int n = list_bin_files(&names);
    int total = 0;

    for (int fi = 0; fi < n; fi++) {
        FILE *fp = fopen(names[fi], "rb");
        if (!fp) continue;

        /* Read and validate header. */
        flash_file_header_t hdr;
        if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
            hdr.magic != FLASH_STORAGE_MAGIC) {
            fclose(fp);
            continue;
        }

        flash_detection_record_t rec;
        while (fread(&rec, 1, sizeof(rec), fp) == sizeof(rec)) {
            fprintf(csv,
                    "%llu,%d,%d,%d,%d,%.4f,%u,%u,%u,%u\n",
                    (unsigned long long)rec.timestamp_us,
                    rec.x, rec.y, rec.w, rec.h,
                    rec.confidence,
                    (unsigned)rec.class_id,
                    (unsigned)rec.target_num,
                    (unsigned)rec.frame_width,
                    (unsigned)rec.frame_height);
            total++;
        }
        fclose(fp);
    }

    fclose(csv);

    for (int i = 0; i < n; i++) free(names[i]);
    free(names);

    pthread_mutex_unlock(&s_mutex);
    return total;
}

/* ------------------------------------------------------------------ */

int flash_storage_read_file(const char *file_path,
                            flash_detection_record_t *buf, size_t buf_len)
{
    if (!file_path || !buf || buf_len == 0) return -1;

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        perror("[flash_storage] read_file: fopen");
        return -1;
    }

    flash_file_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        hdr.magic != FLASH_STORAGE_MAGIC) {
        fprintf(stderr, "[flash_storage] read_file: bad magic in %s\n", file_path);
        fclose(fp);
        return -1;
    }

    int count = 0;
    while ((size_t)count < buf_len &&
           fread(&buf[count], 1, sizeof(flash_detection_record_t), fp)
               == sizeof(flash_detection_record_t))
        count++;

    fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ */

void flash_storage_get_stats(flash_storage_stats_t *stats)
{
    if (!stats) return;
    pthread_mutex_lock(&s_mutex);
    stats->total_records_written = s_total_records;
    stats->total_flushes         = s_total_flushes;
    stats->current_file_records  = s_current_file_records;
    stats->num_files             = s_num_files;
    stats->records_in_buffer     = s_batch_count;
    pthread_mutex_unlock(&s_mutex);
}

/* ------------------------------------------------------------------ */

int flash_storage_clear(void)
{
    pthread_mutex_lock(&s_mutex);

    /* Close current file so we can delete it too. */
    if (s_fd >= 0) {
        fsync(s_fd);
        close(s_fd);
        s_fd = -1;
    }
    s_batch_count          = 0;
    s_current_file_records = 0;

    char **names = NULL;
    int n = list_bin_files(&names);
    int ret = 0;

    for (int i = 0; i < n; i++) {
        if (unlink(names[i]) != 0) {
            fprintf(stderr, "[flash_storage] clear: unlink %s: %s\n",
                    names[i], strerror(errno));
            ret = -1;
        }
    }
    s_num_files = 0;

    /* Reopen a fresh file if we are still initialised. */
    if (s_initialised)
        open_new_file();

    for (int i = 0; i < n; i++) free(names[i]);
    free(names);

    pthread_mutex_unlock(&s_mutex);
    return ret;
}

/* ------------------------------------------------------------------ */

void flash_storage_deinit(void)
{
    if (!s_initialised) return;

    pthread_mutex_lock(&s_mutex);

    flush_locked();

    if (s_fd >= 0) {
        fsync(s_fd);
        close(s_fd);
        s_fd = -1;
    }

    free(s_batch);
    s_batch     = NULL;
    s_batch_count = 0;
    s_initialised = 0;

    printf("[flash_storage] Deinit. total_records=%llu total_flushes=%u\n",
           (unsigned long long)s_total_records, s_total_flushes);

    pthread_mutex_unlock(&s_mutex);
}
