#ifndef RGA_HW_ACCEL_H
#define RGA_HW_ACCEL_H

#include "opencv2/core/core.hpp"
#include "yolov5.h"
#include <stdint.h>

/**
 * @brief Hardware-accelerated image resize using RGA
 * 
 * @param src Source image (OpenCV Mat)
 * @param dst Destination image (OpenCV Mat) - will be allocated
 * @param dst_w Destination width
 * @param dst_h Destination height
 */
void rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h);

/**
 * @brief Direct NV12 → RGB888 → Resize → Letterbox → RKNN DMA input
 * 
 * Performs color conversion, resize, and letterbox padding in one RGA operation
 * directly into the RKNN model's input buffer.
 * 
 * @param nv12_ptr Pointer to NV12 format image data
 * @param src_w Source width
 * @param src_h Source height
 * @param ctx RKNN application context containing input memory
 * @param dst_w Destination (model input) width
 * @param dst_h Destination (model input) height
 * @param scale Output parameter for the scale factor used
 * @param left_pad Output parameter for left padding
 * @param top_pad Output parameter for top padding
 */
void rga_letterbox_nv12_to_rknn(
    void* nv12_ptr,
    int src_w, int src_h,
    rknn_app_context_t* ctx,
    int dst_w, int dst_h,
    float* scale,
    int* left_pad,
    int* top_pad
);

/**
 * @brief Draw a box using hardware-accelerated RGA
 * 
 * @param buf RGB888 buffer pointer
 * @param w Buffer width
 * @param h Buffer height
 * @param x Box top-left x coordinate
 * @param y Box top-left y coordinate
 * @param box_w Box width
 * @param box_h Box height
 * @param color_rgb RGB color (0xRRGGBB format)
 * @param thickness Line thickness in pixels (default 2)
 */
void draw_box_rga(void* buf, int w, int h,
                  int x, int y, int box_w, int box_h,
                  uint32_t color_rgb, int thickness = 3);

/**
 * @brief Clear frame buffer to black using RGA
 * 
 * @param buf RGB888 buffer pointer
 * @param w Buffer width
 * @param h Buffer height
 */
void clear_frame(void* buf, int w, int h);

#endif // RGA_HW_ACCEL_H
