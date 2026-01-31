// #include "rga_hw_accel.h"
// #include "im2d.h"
// #include "RgaApi.h"
// #include "RgaUtils.h"
// #include "dma_alloc.h"
// #include "yolov5.h"
// #include <stdio.h>
// #include <stdexcept>

// void rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h)
// {
//     int src_w = src.cols;
//     int src_h = src.rows;
//     int src_fmt = RK_FORMAT_RGB_888;
//     int dst_fmt = RK_FORMAT_RGB_888;

//     size_t src_size = src_w * src_h * 3;
//     size_t dst_size = dst_w * dst_h * 3;

//     int src_fd = -1;
//     int dst_fd = -1;
//     void* src_buf = nullptr;
//     void* dst_buf = nullptr;

//     // Allocate DMA buffers
//     if (dma_buf_alloc(RV1106_CMA_HEAP_PATH, src_size, &src_fd, &src_buf) < 0)
//         throw std::runtime_error("RGA: failed to allocate source DMA buffer");

//     if (dma_buf_alloc(RV1106_CMA_HEAP_PATH, dst_size, &dst_fd, &dst_buf) < 0) {
//         dma_buf_free(src_size, &src_fd, src_buf);
//         throw std::runtime_error("RGA: failed to allocate dest DMA buffer");
//     }

//     // Copy from cv::Mat into DMA source buffer
//     memcpy(src_buf, src.data, src_size);

//     // Import DMA buffers into RGA
//     rga_buffer_handle_t src_handle = importbuffer_fd(src_fd, src_size);
//     rga_buffer_handle_t dst_handle = importbuffer_fd(dst_fd, dst_size);

//     if (!src_handle || !dst_handle) {
//         dma_buf_free(src_size, &src_fd, src_buf);
//         dma_buf_free(dst_size, &dst_fd, dst_buf);
//         throw std::runtime_error("RGA: failed to import DMA buffers");
//     }

//     rga_buffer_t src_rga = wrapbuffer_handle(src_handle, src_w, src_h, src_fmt);
//     rga_buffer_t dst_rga = wrapbuffer_handle(dst_handle, dst_w, dst_h, dst_fmt);
//     im_rect src_rect = {};
//     im_rect dst_rect = {};

//     // Check parameters
//     if (imcheck(src_rga, dst_rga, src_rect, dst_rect) != IM_STATUS_NOERROR) {
//         releasebuffer_handle(src_handle);
//         releasebuffer_handle(dst_handle);
//         dma_buf_free(src_size, &src_fd, src_buf);
//         dma_buf_free(dst_size, &dst_fd, dst_buf);
//         throw std::runtime_error("RGA: check parameters failed");
//     }

//     int ret = imresize(src_rga, dst_rga);
//     if (ret != IM_STATUS_SUCCESS) {
//         releasebuffer_handle(src_handle);
//         releasebuffer_handle(dst_handle);
//         dma_buf_free(src_size, &src_fd, src_buf);
//         dma_buf_free(dst_size, &dst_fd, dst_buf);
//         throw std::runtime_error("RGA: imresize failed");
//     }

//     // Copy RGA output back into a normal cv::Mat
//     dst.create(dst_h, dst_w, CV_8UC3);
//     memcpy(dst.data, dst_buf, dst_size);

//     // Free everything
//     releasebuffer_handle(src_handle);
//     releasebuffer_handle(dst_handle);
//     dma_buf_free(src_size, &src_fd, src_buf);
//     dma_buf_free(dst_size, &dst_fd, dst_buf);
// }

// void draw_box_rga(void* buf, int w, int h,
//                   int x, int y, int box_w, int box_h,
//                   uint32_t color_rgb)
// {
//     rga_buffer_t img = wrapbuffer_virtualaddr(buf, w, h, RK_FORMAT_RGB_888);

//     // Top border
//     im_rect t = {x, y, box_w, 2};
//     imfill(img, t, color_rgb);

//     // Bottom
//     im_rect b = {x, y + box_h - 2, box_w, 2};
//     imfill(img, b, color_rgb);

//     // Left
//     im_rect l = {x, y, 2, box_h};
//     imfill(img, l, color_rgb);

//     // Right
//     im_rect r = {x + box_w - 2, y, 2, box_h};
//     imfill(img, r, color_rgb);
// }

// void clear_frame(void* buf, int w, int h)
// {
//     rga_buffer_t img = wrapbuffer_virtualaddr(buf, w, h, RK_FORMAT_RGB_888);
//     im_rect rect = {0, 0, w, h};
//     imfill(img, rect, 0x000000); // black
// }

// // Direct NV12 → RGB888 → Resize → Letterbox → RKNN DMA input
// void rga_letterbox_nv12_to_rknn(
//     void* nv12_ptr,
//     int src_w, int src_h,
//     rknn_app_context_t* ctx,
//     int dst_w, int dst_h
// ){
//     float scaleX = (float)dst_w / src_w;
//     float scaleY = (float)dst_h / src_h;
//     scale = (scaleX < scaleY) ? scaleX : scaleY;

//     int new_w = src_w * scale;
//     int new_h = src_h * scale;

//     leftPadding = (dst_w - new_w) / 2;
//     topPadding  = (dst_h - new_h) / 2;

//     rknn_tensor_mem* dst_mem = ctx->input_mems[0];
//     uint8_t* out_ptr = (uint8_t*)dst_mem->virt_addr;

//     int dst_stride = dst_w * 3; // RGB888

//     // Source: NV12 buffer from VI
//     rga_buffer_t src = wrapbuffer_virtualaddr(
//         nv12_ptr,
//         src_w,
//         src_h,
//         RK_FORMAT_YCbCr_420_SP
//     );

//     // Destination: full padded RGB888 buffer (RKNN input)
//     rga_buffer_t dst_full = wrapbuffer_virtualaddr(
//         out_ptr,
//         dst_w,
//         dst_h,
//         RK_FORMAT_RGB_888
//     );

//     // Letterbox scale
//     float sx = (float)dst_w / (float)src_w;
//     float sy = (float)dst_h / (float)src_h;
//     float s  = (sx < sy ? sx : sy);

//     int new_w = (int)(src_w * s);
//     int new_h = (int)(src_h * s);

//     int pad_x = (dst_w - new_w) / 2;
//     int pad_y = (dst_h - new_h) / 2;

//     // Sub-rectangle for resized area
//     rga_buffer_t dst_sub = wrapbuffer_virtualaddr(
//         out_ptr + pad_y * dst_stride + pad_x * 3,
//         new_w,
//         new_h,
//         RK_FORMAT_RGB_888
//     );

//     // Fill entire destination image with black
//     im_rect full_rect;
//     full_rect.x = 0;
//     full_rect.y = 0;
//     full_rect.width = dst_w;
//     full_rect.height = dst_h;

//     imfill(dst_full, full_rect, 0);  // RGB = (0,0,0)

//     // Resize + NV12→RGB conversion into the padded region
//     imresize(src, dst_sub);
// }