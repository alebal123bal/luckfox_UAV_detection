/**
 * motion_features.c
 *
 * Computes LLM-friendly motion features from raw UAV detection records.
 * No I/O here — only arithmetic on the record array provided by the caller.
 *
 * See motion_features.h for the public API.
 */

#include "include/motion_features.h"

#include <stdlib.h>
#include <math.h>

/* Per-sample intermediate values, local to this translation unit. */
typedef struct {
    double cx, cy;       /* centroid, pixels  */
    double vx, vy;       /* velocity, px/s    */
    double speed;        /* |v|               */
    double heading_rad;  /* atan2(dy, dx)     */
    float  confidence;
} fsample_t;

motion_feature_t *compute_motion_features(
    const flash_detection_record_t *records,
    long count,
    int *n_out)
{
    *n_out = 0;
    if (!records || count <= 0)
        return NULL;

    /* ---- collect unique target_num values (first-seen order) ---- */
    uint8_t targets[256];
    int n_targets = 0;
    for (long i = 0; i < count; i++) {
        uint8_t t = records[i].target_num;
        int found = 0;
        for (int j = 0; j < n_targets; j++)
            if (targets[j] == t) { found = 1; break; }
        if (!found && n_targets < 256)
            targets[n_targets++] = t;
    }

    motion_feature_t *out =
        (motion_feature_t *)malloc(n_targets * sizeof(*out));
    if (!out)
        return NULL;

    int out_count = 0;

    for (int ti = 0; ti < n_targets; ti++) {
        uint8_t tgt = targets[ti];

        /* ---- extract records for this target ---- */
        long cap = 64;
        flash_detection_record_t *trk =
            (flash_detection_record_t *)malloc(cap * sizeof(*trk));
        if (!trk) continue;
        long trk_count = 0;

        for (long i = 0; i < count; i++) {
            if (records[i].target_num != tgt) continue;
            if (trk_count >= cap) {
                cap *= 2;
                flash_detection_record_t *tmp =
                    (flash_detection_record_t *)realloc(trk, cap * sizeof(*trk));
                if (!tmp) { free(trk); trk = NULL; break; }
                trk = tmp;
            }
            trk[trk_count++] = records[i];
        }
        if (!trk || trk_count == 0) { free(trk); continue; }

        /* ---- compute centroid and inter-frame motion per sample ---- */
        fsample_t *smp = (fsample_t *)malloc(trk_count * sizeof(*smp));
        if (!smp) { free(trk); continue; }

        for (long i = 0; i < trk_count; i++) {
            const flash_detection_record_t *r = &trk[i];
            smp[i].cx         = r->x + r->w / 2.0;
            smp[i].cy         = r->y + r->h / 2.0;
            smp[i].confidence = r->confidence;
            smp[i].vx = smp[i].vy = smp[i].speed = smp[i].heading_rad = 0.0;

            if (i > 0) {
                double dt = (double)(trk[i].timestamp_us - trk[i-1].timestamp_us) / 1e6;
                if (dt > 0.0) {
                    double dx          = smp[i].cx - smp[i-1].cx;
                    double dy          = smp[i].cy - smp[i-1].cy;
                    smp[i].vx          = dx / dt;
                    smp[i].vy          = dy / dt;
                    smp[i].speed       = sqrt(smp[i].vx * smp[i].vx +
                                              smp[i].vy * smp[i].vy);
                    smp[i].heading_rad = atan2(dy, dx);
                }
            }
        }

        /* ---- sliding-window average over the last MOTION_WINDOW samples ---- */
        long win_start = (trk_count > MOTION_WINDOW) ? trk_count - MOTION_WINDOW : 0;
        long win_size  = trk_count - win_start;
        int  vel_count = 0;
        double avg_vx = 0.0, avg_vy = 0.0, avg_speed = 0.0, avg_conf = 0.0;

        for (long i = win_start; i < trk_count; i++) {
            avg_conf += smp[i].confidence;
            if (i > 0) {   /* first sample has no velocity */
                avg_vx    += smp[i].vx;
                avg_vy    += smp[i].vy;
                avg_speed += smp[i].speed;
                vel_count++;
            }
        }
        avg_conf /= (double)win_size;
        if (vel_count > 0) {
            avg_vx    /= vel_count;
            avg_vy    /= vel_count;
            avg_speed /= vel_count;
        }

        /* ---- normalized centroid from the most recent sample in the window ---- */
        const flash_detection_record_t *last = &trk[trk_count - 1];
        double nx = (last->frame_width  > 0)
                    ? smp[trk_count - 1].cx / last->frame_width  : 0.0;
        double ny = (last->frame_height > 0)
                    ? smp[trk_count - 1].cy / last->frame_height : 0.0;

        /* ---- write output entry ---- */
        out[out_count].nx              = nx;
        out[out_count].ny              = ny;
        out[out_count].avg_vx          = avg_vx;
        out[out_count].avg_vy          = avg_vy;
        out[out_count].avg_speed       = avg_speed;
        out[out_count].avg_heading_rad = atan2(avg_vy, avg_vx);
        out[out_count].avg_confidence  = avg_conf;
        out[out_count].samples         = win_size;
        out[out_count].target_id       = tgt;
        out_count++;

        free(smp);
        free(trk);
    }

    *n_out = out_count;
    return out;
}
