/**
 * read_detections.c
 *
 * Standalone reader for flash_storage binary log files.
 * Cross-compile with the same toolchain as the main project, then scp to
 * the device and run from your SSH terminal.
 *
 * Usage (on the device):
 *   ./read_detections                         # print all logs as a table
 *   ./read_detections -f /userdata/uav_detections/detections_*.bin
 *   ./read_detections -c /tmp/export.csv      # dump everything to CSV
 *   ./read_detections --tail 20               # last 20 records across all files
 *   ./read_detections --stats                 # file stats only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Structures – must match flash_storage.h exactly                    */
/* ------------------------------------------------------------------ */

#define FLASH_STORAGE_MAGIC   0x55415644u
#define FLASH_STORAGE_VERSION 1u
#define DEFAULT_DIR           "/userdata/uav_detections"

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
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

static void us_to_str(uint64_t us, char *buf, size_t sz)
{
    time_t secs = (time_t)(us / 1000000ULL);
    struct tm t;
    gmtime_r(&secs, &t);
    snprintf(buf, sz, "%04d-%02d-%02d %02d:%02d:%02d.%06llu UTC",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec,
             (unsigned long long)(us % 1000000ULL));
}

/** Print a horizontal rule. */
static void hrule(void)
{
    printf("%-26s %-5s %-5s %-5s %-5s %-6s %-5s %-4s %s\n",
           "--------------------------",
           "-----", "-----", "-----", "-----",
           "------", "-----", "----",
           "--------");
}

static void print_header(void)
{
    hrule();
    printf("%-26s %-5s %-5s %-5s %-5s %-6s %-5s %-4s %s\n",
           "Timestamp (UTC)",
           "X", "Y", "W", "H",
           "Conf%", "Class", "Tgt",
           "Frame");
    hrule();
}

static void print_record(const flash_detection_record_t *r)
{
    char ts[48];
    us_to_str(r->timestamp_us, ts, sizeof(ts));
    printf("%-26s %-5d %-5d %-5d %-5d %-6.1f %-5u %-4u %ux%u\n",
           ts,
           r->x, r->y, r->w, r->h,
           r->confidence * 100.0f,
           (unsigned)r->class_id,
           (unsigned)r->target_num,
           (unsigned)r->frame_width,
           (unsigned)r->frame_height);
}

/** Compare function for qsort on char* filenames (lexicographic = chrono). */
static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* ------------------------------------------------------------------ */
/*  File list builder                                                   */
/* ------------------------------------------------------------------ */

static int list_bin_files(const char *dir, char ***out)
{
    DIR *dp = opendir(dir);
    if (!dp) { perror(dir); return -1; }

    int count = 0;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        size_t nl = strlen(de->d_name);
        if (nl > 4 && strcmp(de->d_name + nl - 4, ".bin") == 0)
            count++;
    }
    rewinddir(dp);

    char **names = (char **)malloc(count * sizeof(char *));
    if (!names) { closedir(dp); return -1; }

    int idx = 0;
    while ((de = readdir(dp)) != NULL && idx < count) {
        size_t nl = strlen(de->d_name);
        if (nl > 4 && strcmp(de->d_name + nl - 4, ".bin") == 0) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
            names[idx++] = strdup(full);
        }
    }
    closedir(dp);
    qsort(names, idx, sizeof(char *), cmp_str);
    *out = names;
    return idx;
}

/* ------------------------------------------------------------------ */
/*  Operations                                                          */
/* ------------------------------------------------------------------ */

/** Print all records from a single file. Returns record count, -1 on error. */
static long print_file(const char *path, int include_header, long skip_first)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return -1; }

    flash_file_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        hdr.magic != FLASH_STORAGE_MAGIC) {
        fprintf(stderr, "Bad header in %s (not a valid log file)\n", path);
        fclose(fp);
        return -1;
    }

    if (include_header) {
        char ts[48];
        us_to_str(hdr.created_us, ts, sizeof(ts));
        printf("\n=== %s  [created %s] ===\n", path, ts);
        print_header();
    }

    long count = 0;
    flash_detection_record_t r;
    while (fread(&r, 1, sizeof(r), fp) == sizeof(r)) {
        if (count >= skip_first)
            print_record(&r);
        count++;
    }
    fclose(fp);
    return count;
}

/** Print per-file statistics. */
static void cmd_stats(const char *dir)
{
    char **names = NULL;
    int n = list_bin_files(dir, &names);
    if (n <= 0) { printf("No log files found in %s\n", dir); return; }

    printf("\n%-60s %-8s %-8s %s\n", "File", "Records", "Size", "Created (UTC)");
    printf("%-60s %-8s %-8s %s\n",
           "------------------------------------------------------------",
           "-------", "-------", "----------------------------");

    long total_records = 0;
    for (int i = 0; i < n; i++) {
        FILE *fp = fopen(names[i], "rb");
        if (!fp) continue;

        struct stat st;
        stat(names[i], &st);

        flash_file_header_t hdr;
        char ts[48] = "?";
        if (fread(&hdr, 1, sizeof(hdr), fp) == sizeof(hdr) &&
            hdr.magic == FLASH_STORAGE_MAGIC)
            us_to_str(hdr.created_us, ts, sizeof(ts));

        fclose(fp);

        off_t payload = st.st_size - (off_t)sizeof(flash_file_header_t);
        long recs = (payload > 0) ? (long)(payload / sizeof(flash_detection_record_t)) : 0;
        total_records += recs;

        printf("%-60s %-8ld %-8lld %s\n",
               names[i], recs, (long long)st.st_size, ts);

        free(names[i]);
    }
    free(names);
    printf("\nTotal records across all files: %ld\n\n", total_records);
}

/** Export all logs to CSV. */
static long cmd_csv(const char *dir, const char *out_path)
{
    char **names = NULL;
    int n = list_bin_files(dir, &names);
    if (n <= 0) { printf("No log files found in %s\n", dir); return 0; }

    FILE *csv = fopen(out_path, "w");
    if (!csv) { perror(out_path); return -1; }

    fprintf(csv, "timestamp_us,x,y,w,h,confidence,class_id,"
                 "target_num,frame_width,frame_height\n");

    long total = 0;
    for (int i = 0; i < n; i++) {
        FILE *fp = fopen(names[i], "rb");
        if (!fp) { free(names[i]); continue; }

        flash_file_header_t hdr;
        if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
            hdr.magic != FLASH_STORAGE_MAGIC) {
            fclose(fp); free(names[i]); continue;
        }

        flash_detection_record_t r;
        while (fread(&r, 1, sizeof(r), fp) == sizeof(r)) {
            fprintf(csv, "%llu,%d,%d,%d,%d,%.4f,%u,%u,%u,%u\n",
                    (unsigned long long)r.timestamp_us,
                    r.x, r.y, r.w, r.h,
                    r.confidence,
                    (unsigned)r.class_id,
                    (unsigned)r.target_num,
                    (unsigned)r.frame_width,
                    (unsigned)r.frame_height);
            total++;
        }
        fclose(fp);
        free(names[i]);
    }
    free(names);
    fclose(csv);
    printf("Exported %ld records to %s\n", total, out_path);
    return total;
}

/** Export all logs to JSON. */
static long cmd_json(const char *dir, const char *out_path)
{
    char **names = NULL;
    int n = list_bin_files(dir, &names);
    if (n <= 0) { printf("No log files found in %s\n", dir); return 0; }

    int to_stdout = (strcmp(out_path, "-") == 0);
    FILE *js = to_stdout ? stdout : fopen(out_path, "w");
    if (!js) { perror(out_path); return -1; }

    fprintf(js, "[\n");

    long total = 0;
    for (int i = 0; i < n; i++) {
        FILE *fp = fopen(names[i], "rb");
        if (!fp) { free(names[i]); continue; }

        flash_file_header_t hdr;
        if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
            hdr.magic != FLASH_STORAGE_MAGIC) {
            fclose(fp); free(names[i]); continue;
        }

        flash_detection_record_t r;
        while (fread(&r, 1, sizeof(r), fp) == sizeof(r)) {
            char ts[48];
            us_to_str(r.timestamp_us, ts, sizeof(ts));
            if (total > 0)
                fprintf(js, ",\n");
            fprintf(js,
                "  {\n"
                "    \"timestamp_us\": %llu,\n"
                "    \"timestamp\": \"%s\",\n"
                "    \"x\": %d,\n"
                "    \"y\": %d,\n"
                "    \"w\": %d,\n"
                "    \"h\": %d,\n"
                "    \"confidence\": %.4f,\n"
                "    \"class_id\": %u,\n"
                "    \"target_num\": %u,\n"
                "    \"frame_width\": %u,\n"
                "    \"frame_height\": %u\n"
                "  }",
                (unsigned long long)r.timestamp_us,
                ts,
                r.x, r.y, r.w, r.h,
                r.confidence,
                (unsigned)r.class_id,
                (unsigned)r.target_num,
                (unsigned)r.frame_width,
                (unsigned)r.frame_height);
            total++;
        }
        fclose(fp);
        free(names[i]);
    }
    free(names);

    if (total > 0)
        fprintf(js, "\n");
    fprintf(js, "]\n");
    if (!to_stdout) {
        fclose(js);
        printf("Exported %ld records to %s\n", total, out_path);
    }
    return total;
}

/** Print the last N records across all files. */
static void cmd_tail(const char *dir, long want)
{
    /* Load all records into a dynamically growing array. */
    char **names = NULL;
    int n = list_bin_files(dir, &names);
    if (n <= 0) { printf("No log files found in %s\n", dir); return; }

    long capacity = want * 2 + 256;
    flash_detection_record_t *buf =
        (flash_detection_record_t *)malloc(capacity * sizeof(*buf));
    if (!buf) { fprintf(stderr, "malloc failed\n"); return; }
    long count = 0;

    for (int i = 0; i < n; i++) {
        FILE *fp = fopen(names[i], "rb");
        if (!fp) { free(names[i]); continue; }
        flash_file_header_t hdr;
        if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
            hdr.magic != FLASH_STORAGE_MAGIC) {
            fclose(fp); free(names[i]); continue;
        }
        flash_detection_record_t r;
        while (fread(&r, 1, sizeof(r), fp) == sizeof(r)) {
            if (count >= capacity) {
                capacity *= 2;
                buf = (flash_detection_record_t *)realloc(buf, capacity * sizeof(*buf));
                if (!buf) { fclose(fp); goto done; }
            }
            buf[count++] = r;
        }
        fclose(fp);
        free(names[i]);
    }

    {
        long start = (count > want) ? count - want : 0;
        print_header();
        for (long j = start; j < count; j++)
            print_record(&buf[j]);
        printf("\nShowing %ld of %ld total records.\n",
               count - start, count);
    }

done:
    free(buf);
    free(names);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "  (no args)           Print all records from all log files in default dir\n"
        "  -d <dir>            Use <dir> instead of " DEFAULT_DIR "\n"
        "  -f <file> [file...] Print specific .bin file(s)\n"
        "  -c <out.csv>        Export all logs to a CSV file\n"
        "  -j <out.json>       Export all logs to a JSON file\n"
        "  --json              Print all logs as JSON to stdout\n"
        "  --tail <N>          Print the last N records (default 20)\n"
        "  --stats             Show per-file statistics only\n"
        "  -h, --help          This help\n"
        "\n"
        "Examples:\n"
        "  %s\n"
        "  %s --tail 50\n"
        "  %s -c /tmp/export.csv && cat /tmp/export.csv\n"
        "  %s --stats\n"
        "  %s -f /userdata/uav_detections/detections_20260224_120000_000000.bin\n",
        prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    const char *dir     = DEFAULT_DIR;
    const char *csv_out  = NULL;
    const char *json_out = NULL;
    int         do_stats = 0;
    long        tail_n  = 0;

    /* Specific files listed explicitly. */
    const char **explicit_files = NULL;
    int          n_explicit     = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--stats") == 0) {
            do_stats = 1;
        } else if (strcmp(argv[i], "--tail") == 0) {
            if (++i >= argc) { fprintf(stderr, "--tail requires N\n"); return 1; }
            tail_n = atol(argv[i]);
            if (tail_n <= 0) tail_n = 20;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (++i >= argc) { fprintf(stderr, "-c requires output path\n"); return 1; }
            csv_out = argv[i];
        } else if (strcmp(argv[i], "-j") == 0) {
            if (++i >= argc) { fprintf(stderr, "-j requires output path\n"); return 1; }
            json_out = argv[i];
        } else if (strcmp(argv[i], "--json") == 0) {
            json_out = "-";
        } else if (strcmp(argv[i], "-d") == 0) {
            if (++i >= argc) { fprintf(stderr, "-d requires directory\n"); return 1; }
            dir = argv[i];
        } else if (strcmp(argv[i], "-f") == 0) {
            /* Collect all following args that don't start with '-'. */
            i++;
            int start = i;
            while (i < argc && argv[i][0] != '-') i++;
            n_explicit = i - start;
            if (n_explicit <= 0) { fprintf(stderr, "-f requires at least one file\n"); return 1; }
            explicit_files = (const char **)&argv[start];
            i--; /* outer loop will i++ */
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* --- Dispatch --- */

    if (do_stats) {
        cmd_stats(dir);
        return 0;
    }

    if (csv_out) {
        return (cmd_csv(dir, csv_out) < 0) ? 1 : 0;
    }

    if (json_out) {
        return (cmd_json(dir, json_out) < 0) ? 1 : 0;
    }

    if (tail_n > 0) {
        cmd_tail(dir, tail_n);
        return 0;
    }

    if (n_explicit > 0) {
        print_header();
        for (int i = 0; i < n_explicit; i++)
            print_file(explicit_files[i], 0 /*no sub-header*/, 0);
        return 0;
    }

    /* Default: print everything. */
    char **names = NULL;
    int n = list_bin_files(dir, &names);
    if (n <= 0) {
        printf("No log files found in %s\n", dir);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        print_file(names[i], 1 /*with sub-header*/, 0);
        free(names[i]);
    }
    free(names);
    return 0;
}
