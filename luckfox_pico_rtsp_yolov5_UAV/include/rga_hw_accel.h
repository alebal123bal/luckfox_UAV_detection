#ifndef RGA_HW_ACCEL_H
#define RGA_HW_ACCEL_H

#include "opencv2/core/core.hpp"

/**
 * @brief Hardware-accelerated image resize using RGA
 * 
 * @param src Source image (OpenCV Mat)
 * @param dst Destination image (OpenCV Mat) - will be allocated
 * @param dst_w Destination width
 * @param dst_h Destination height
 */
void rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h);

#endif // RGA_HW_ACCEL_H
