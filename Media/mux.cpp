#include "MfStuff.h"
#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC
// a wrapper around a single output AVStream

typedef struct OutputStream {
	AVStream* st;
	AVCodecContext* enc;
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	AVFrame* frame;
	AVFrame* tmp_frame;
	float t, tincr, tincr2;
	struct SwsContext* sws_ctx;
	struct SwrContext* swr_ctx;
} OutputStream;

//static void log_packet(const AVFormatContext* fmt_ctx, const AVPacket* pkt)
//{
//	AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
//	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
//		av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
//		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
//		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
//		pkt->stream_index);
//}



/**************************************************************/
/* media file output */
int main(int argc, char** argv)
{


	
	return 0;
}
