#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <termios.h>

#include "rtsp_demo.h"
#include "luckfox_mpi.h"
#include "yolov5.h"
#include "uart_comm.h"
// #include "rga_hw_accel.h"

#include "im2d.hpp"
#include "RgaUtils.h"
#include "im2d_common.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#define DISP_WIDTH  720
#define DISP_HEIGHT 480

// disp size
int width    = DISP_WIDTH;
int height   = DISP_HEIGHT;

// Serial terminal
#define SERIAL_PORT_NUM 3  // UART3

// model size
int model_width = 640;
int model_height = 640;	
float scale ;
int leftPadding ;
int topPadding  ;

// Profiling
static inline long long now_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void mapCoordinates(int *x, int *y) {	
	int mx = *x - leftPadding;
	int my = *y - topPadding;

    *x = (int)((float)mx / scale);
    *y = (int)((float)my / scale);
}

void draw_box_rga(void* buf, int w, int h,
                  int x, int y, int box_w, int box_h,
                  uint32_t color_rgb)
{
    rga_buffer_t img = wrapbuffer_virtualaddr(buf, w, h, RK_FORMAT_RGB_888);

    // Top border
    im_rect t = {x, y, box_w, 2};
    imfill(img, t, color_rgb);

    // Bottom
    im_rect b = {x, y + box_h - 2, box_w, 2};
    imfill(img, b, color_rgb);

    // Left
    im_rect l = {x, y, 2, box_h};
    imfill(img, l, color_rgb);

    // Right
    im_rect r = {x + box_w - 2, y, 2, box_h};
    imfill(img, r, color_rgb);
}

void clear_frame(void* buf, int w, int h)
{
    rga_buffer_t img = wrapbuffer_virtualaddr(buf, w, h, RK_FORMAT_RGB_888);
    im_rect rect = {0, 0, w, h};
    imfill(img, rect, 0x000000); // black
}

// Direct NV12 → RGB888 → Resize → Letterbox → RKNN DMA input
void rga_letterbox_nv12_to_rknn(
    void* nv12_ptr,
    int src_w, int src_h,
    rknn_app_context_t* ctx,
    int dst_w, int dst_h
){
    // -------------------------------------------------
    // 1. Compute scale + padding (GLOBAL VARIABLES)
    // -------------------------------------------------
    float scaleX = (float)dst_w / src_w;
    float scaleY = (float)dst_h / src_h;
    scale = (scaleX < scaleY) ? scaleX : scaleY;

    int new_w = src_w * scale;
    int new_h = src_h * scale;

    leftPadding = (dst_w - new_w) / 2;
    topPadding  = (dst_h - new_h) / 2;

    // -------------------------------------------------
    // 2. Setup RKNN output buffer
    // -------------------------------------------------
    rknn_tensor_mem* dst_mem = ctx->input_mems[0];
    uint8_t* out_ptr = (uint8_t*)dst_mem->virt_addr;

    int dst_stride = dst_w * 3; // RGB88824bpp

    // -------------------------------------------------
    // 3. Source NV12
    // -------------------------------------------------
    rga_buffer_t src = wrapbuffer_virtualaddr(
        nv12_ptr,
        src_w,
        src_h,
        RK_FORMAT_YCbCr_420_SP
    );

    // -------------------------------------------------
    // 4. Destination FULL padded RGB
    // -------------------------------------------------
    rga_buffer_t dst_full = wrapbuffer_virtualaddr(
        out_ptr,
        dst_w,
        dst_h,
        RK_FORMAT_RGB_888
    );

    // -------------------------------------------------
    // 5. Clear the tensor (letterbox padding)
    // -------------------------------------------------
    im_rect full_rect = {0, 0, dst_w, dst_h};
    imfill(dst_full, full_rect, 0x000000);

    // -------------------------------------------------
    // 6. Sub-rectangle for scaled region
    // -------------------------------------------------
    rga_buffer_t dst_sub = wrapbuffer_virtualaddr(
        out_ptr + topPadding * dst_stride + leftPadding * 3,
        new_w,
        new_h,
        RK_FORMAT_RGB_888
    );

    // -------------------------------------------------
    // 7. NV12 → RGB + resize into padded area
    // -------------------------------------------------
    imresize(src, dst_sub);
}



int main(int argc, char *argv[]) {
    system("RkLunch-stop.sh");
	RK_S32 s32Ret = 0; 
	int sX,sY,eX,eY; 
		
	// Rknn model
	char text[16];
	rknn_app_context_t rknn_app_ctx;	
	object_detect_result_list od_results;
    int ret;
	const char *model_path = "./model/yolov5.rknn";
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));	
	init_yolov5_model(model_path, &rknn_app_ctx);
	printf("init rknn model success!\n");
	init_post_process();

	//h264_frame	
	VENC_STREAM_S stFrame;	
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	RK_U64 H264_PTS = 0;
	RK_U32 H264_TimeRef = 0; 
	VIDEO_FRAME_INFO_S stViFrame;
	
	// Create Pool
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3 ;
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
	//PoolCfg.bPreAlloc = RK_FALSE;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
	printf("Create Pool success !\n");	

	// Get MB from Pool 
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);
	
	// Build h264_frame
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat =  RK_FMT_RGB888; 
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
	// Use memory safe operations
	cv::Mat frame_dma(height, width, CV_8UC3, data);
	cv::Mat frame;
	frame_dma.copyTo(frame);

	// rkaiq init
	RK_BOOL multi_sensor = RK_FALSE;	
	const char *iq_dir = "/etc/iqfiles";
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);

	// rkmpi init
	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}

	// rtsp init	
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;
	g_rtsplive = create_rtsp_demo(554);
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());
	
	// vi init
	vi_dev_init();
	vi_chn_init(0, width, height);

	// venc init
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
	venc_init(0, width, height, enCodecType);

	printf("venc init success\n");	

	// Init serial port
	int serial_fd = uart_init(SERIAL_PORT_NUM, 115200);
	if (serial_fd < 0) {
		return 1;
	}

	// Test UART connection
	uart_printf(serial_fd, "Hello world!\n");


	// Profiling
	long long t0, t1, t2;

	while (1)
	{
		// -----------------------------
		// 1. GET CAMERA FRAME (NV12)
		// -----------------------------
		s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, 0);
		if (s32Ret != RK_SUCCESS) 
			continue;

		void* vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

		t0 = now_us();

		// -----------------------------
		// 2. PREPROCESS → RKNN TENSOR
		// -----------------------------
		rga_letterbox_nv12_to_rknn(
			vi_data, width, height,
			&rknn_app_ctx,
			640, 640
		);

		t1 = now_us();

		// -----------------------------
		// 3. RUN YOLO INFERENCE
		// -----------------------------
		inference_yolov5_model(&rknn_app_ctx, &od_results);

		t2 = now_us();

		// -----------------------------
		// 4. COPY NV12 CAMERA → RGB888 DMA BUFFER
		// -----------------------------
		rga_buffer_t src_nv12 = wrapbuffer_virtualaddr(
			vi_data, width, height, RK_FORMAT_YCbCr_420_SP
		);
		rga_buffer_t dst_rgb = wrapbuffer_virtualaddr(
			data, width, height, RK_FORMAT_RGB_888
		);

		// Convert NV12 camera to RGB888 display buffer
		imcvtcolor(src_nv12, dst_rgb, 
				RK_FORMAT_YCbCr_420_SP, 
				RK_FORMAT_RGB_888);

		// -----------------------------
		// 5. DRAW YOLO BOXES (ON RGB BUFFER)
		// -----------------------------
		for (int i = 0; i < od_results.count; i++)
		{
			object_detect_result* det = &(od_results.results[i]);

			int sX = det->box.left;
			int sY = det->box.top;
			int eX = det->box.right;
			int eY = det->box.bottom;

			// Map inference coords back to screen coords
			mapCoordinates(&sX, &sY);
			mapCoordinates(&eX, &eY);
			
			printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det->cls_id),
							 sX, sY, eX, eY, det->prop);
							 
			draw_box_rga(data, width, height,
						sX, sY,
						eX - sX, eY - sY,
						0x00FF00);   // GREEN
		}

		// -----------------------------
		// 6. SEND RGB BUFFER TO ENCODER
		// -----------------------------
		h264_frame.stVFrame.pMbBlk   = src_Blk;
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS   = TEST_COMM_GetNowUs();

		RK_MPI_VENC_SendFrame(0, &h264_frame, 0);

		// -----------------------------
		// 7. GET ENCODED STREAM → RTSP
		// -----------------------------
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, 0);
		if (s32Ret == RK_SUCCESS)
		{
			void* pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);

			rtsp_tx_video(g_rtsp_session,
						(uint8_t*)pData,
						stFrame.pstPack->u32Len,
						stFrame.pstPack->u64PTS);

			rtsp_do_event(g_rtsplive);
		}

		// -----------------------------
		// 8. RELEASE BUFFERS
		// -----------------------------
		RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		RK_MPI_VENC_ReleaseStream(0, &stFrame);

		// -----------------------------
		// 9. PROFILING PRINT
		// -----------------------------
		printf("RGA Preprocess=%lld ms | NPU Inference=%lld ms\n",
			(t1 - t0) / 1000,
			(t2 - t1) / 1000);
	} // while(1)


	// Destory MB
	RK_MPI_MB_ReleaseMB(src_Blk);
	// Destory Pool
	RK_MPI_MB_DestroyPool(src_Pool);
	
	RK_MPI_VI_DisableChn(0, 0);
	RK_MPI_VI_DisableDev(0);

	SAMPLE_COMM_ISP_Stop(0);
	
	RK_MPI_VENC_StopRecvFrame(0);
	RK_MPI_VENC_DestroyChn(0);

	free(stFrame.pstPack);

	if (g_rtsplive)
		rtsp_del_demo(g_rtsplive);
	
	RK_MPI_SYS_Exit();

	// Close UART
	uart_close(serial_fd);

	// Release rknn model
    release_yolov5_model(&rknn_app_ctx);		
	deinit_post_process();
	
	return 0;
}
