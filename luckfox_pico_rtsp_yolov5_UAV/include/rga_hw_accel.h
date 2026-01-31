// #ifndef RGA_HW_ACCEL_H
// #define RGA_HW_ACCEL_H

// #include "opencv2/core/core.hpp"
// #include "yolov5.h"

// // // Forward declaration
// // typedef struct _rknn_app_context rknn_app_context_t;

// /**
//  * @brief Hardware-accelerated image resize using RGA
//  * 
//  * @param src Source image (OpenCV Mat)
//  * @param dst Destination image (OpenCV Mat) - will be allocated
//  * @param dst_w Destination width
//  * @param dst_h Destination height
//  */
// void rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h);

// /**
//  * @brief Direct NV12 → RGB888 → Resize → Letterbox → RKNN DMA input
//  * 
//  * Performs color conversion, resize, and letterbox padding in one RGA operation
//  * directly into the RKNN model's input buffer.
//  * 
//  * @param nv12_ptr Pointer to NV12 format image data
//  * @param src_w Source width
//  * @param src_h Source height
//  * @param ctx RKNN application context containing input memory
//  * @param dst_w Destination (model input) width
//  * @param dst_h Destination (model input) height
//  */
// void rga_letterbox_nv12_to_rknn(
//     void* nv12_ptr,
//     int src_w, int src_h,
//     rknn_app_context_t* ctx,
//     int dst_w, int dst_h
// );

// void draw_box_rga(void* buf, int w, int h,
//                   int x, int y, int box_w, int box_h,
//                   uint32_t color_rgb);

// void clear_frame(void* buf, int w, int h);

// #endif // RGA_HW_ACCEL_H
