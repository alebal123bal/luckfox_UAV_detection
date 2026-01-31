#include "rga_hw_accel.h"
#include "im2d.h"
#include "RgaApi.h"
#include "RgaUtils.h"
#include "dma_alloc.h"
#include <stdio.h>
#include <stdexcept>

void rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h)
{
    int src_w = src.cols;
    int src_h = src.rows;
    int src_fmt = RK_FORMAT_RGB_888;
    int dst_fmt = RK_FORMAT_RGB_888;

    size_t src_size = src_w * src_h * 3;
    size_t dst_size = dst_w * dst_h * 3;

    int src_fd = -1;
    int dst_fd = -1;
    void* src_buf = nullptr;
    void* dst_buf = nullptr;

    // Allocate DMA buffers
    if (dma_buf_alloc(RV1106_CMA_HEAP_PATH, src_size, &src_fd, &src_buf) < 0)
        throw std::runtime_error("RGA: failed to allocate source DMA buffer");

    if (dma_buf_alloc(RV1106_CMA_HEAP_PATH, dst_size, &dst_fd, &dst_buf) < 0) {
        dma_buf_free(src_size, &src_fd, src_buf);
        throw std::runtime_error("RGA: failed to allocate dest DMA buffer");
    }

    // Copy from cv::Mat into DMA source buffer
    memcpy(src_buf, src.data, src_size);

    // Import DMA buffers into RGA
    rga_buffer_handle_t src_handle = importbuffer_fd(src_fd, src_size);
    rga_buffer_handle_t dst_handle = importbuffer_fd(dst_fd, dst_size);

    if (!src_handle || !dst_handle) {
        dma_buf_free(src_size, &src_fd, src_buf);
        dma_buf_free(dst_size, &dst_fd, dst_buf);
        throw std::runtime_error("RGA: failed to import DMA buffers");
    }

    rga_buffer_t src_rga = wrapbuffer_handle(src_handle, src_w, src_h, src_fmt);
    rga_buffer_t dst_rga = wrapbuffer_handle(dst_handle, dst_w, dst_h, dst_fmt);
    im_rect src_rect = {};
    im_rect dst_rect = {};

    // Check parameters
    if (imcheck(src_rga, dst_rga, src_rect, dst_rect) != IM_STATUS_NOERROR) {
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
        dma_buf_free(src_size, &src_fd, src_buf);
        dma_buf_free(dst_size, &dst_fd, dst_buf);
        throw std::runtime_error("RGA: check parameters failed");
    }

    int ret = imresize(src_rga, dst_rga);
    if (ret != IM_STATUS_SUCCESS) {
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
        dma_buf_free(src_size, &src_fd, src_buf);
        dma_buf_free(dst_size, &dst_fd, dst_buf);
        throw std::runtime_error("RGA: imresize failed");
    }

    // Copy RGA output back into a normal cv::Mat
    dst.create(dst_h, dst_w, CV_8UC3);
    memcpy(dst.data, dst_buf, dst_size);

    // Free everything
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
    dma_buf_free(src_size, &src_fd, src_buf);
    dma_buf_free(dst_size, &dst_fd, dst_buf);
}
