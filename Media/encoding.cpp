#include "encoding.h"

extern "C"
{
	#include <libswscale/swscale.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/mathematics.h>
	#include <libavformat/avformat.h>
	#include <libavutil/opt.h>
	#include <libswresample/swresample.h>
	#include <libavutil/avassert.h>
}

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC

VideoFormat videoFormat;

typedef struct OutputStream 
{
	AVStream* st;
	AVCodecContext* enc;
	int64_t pts;
	AVFrame* frame;
	AVFrame* src_frame;
	struct SwsContext* sws_ctx;
} OutputStream;

static OutputStream video_st = { 0 }, audio_st = { 0 };
static AVOutputFormat* fmt;
static AVFormatContext* output_format_context = NULL;
static AVFormatContext* audio_input_format_context = NULL;
static AVCodec* audio_codec = NULL, * video_codec = NULL;
static int encode_video, encode_audio;

static int write_frame(AVFormatContext* fmt_ctx, const AVRational* time_base, AVStream* st, AVPacket* pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;
	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream* ost, AVFormatContext* oc, AVCodec** codec, enum AVCodecID codec_id, const VideoFormat &vidFmt)
{
	AVCodecContext* c;
	int i;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec))
	{
		fprintf(stderr, "Could not find encoder for '%s'\n",
		avcodec_get_name(codec_id));
		exit(1);
	}
	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) 
	{
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c)
	{
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;
	switch ((*codec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->sample_rate = 48000;
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		ost->st->time_base = { 1, c->sample_rate };
		break;
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		/* Resolution must be a multiple of two. */
		c->width = vidFmt.width;
		c->height = vidFmt.height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		ost->st->time_base = { 100, (int)(vidFmt.fps * 100) };
		c->time_base = ost->st->time_base;
		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = AV_PIX_FMT_YUV420P;

		ost->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
		if (!ost->sws_ctx)
		{
			fprintf(stderr,
				"Could not initialize the conversion context\n");
			exit(1);
		}
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */
static void open_audio(AVFormatContext* oc, AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
	int ret;
	AVCodecContext* c;
	AVDictionary* opt = NULL;
	c = ost->enc;
	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not open audio codec: %d\n", ret);
		exit(1);
	}
	
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext* oc, OutputStream* ost)
{
	AVPacket pkt = { 0 }; // data and size must be 0;
	int ret;
	av_init_packet(&pkt);
	if ((ret = av_read_frame(audio_input_format_context, &pkt)) < 0)
	{
		if (ret == AVERROR_EOF)
			return ret;
		else
		{
			fprintf(stderr, "Could not read frame (error '%d')\n", ret);
			return ret;
		}
	}
	
	ost->pts = pkt.pts;
	ret = write_frame(output_format_context, &ost->st->time_base, ost->st, &pkt);
	//pkt.stream_index = ost->st->index;
	//ret = av_interleaved_write_frame(oc, &pkt);
	if (ret < 0) 
	{
		fprintf(stderr, "Error while writing audio frame: %d\n", ret);
		exit(1);
	}
	return ret;
}

/**************************************************************/
/* video output */
static AVFrame* alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame* picture;
	int ret;
	picture = av_frame_alloc();
	if (!picture)
		return NULL;
	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;
	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0)
	{
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}
	return picture;
}

static void open_video(AVFormatContext* oc, AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
	int ret;
	AVCodecContext* c = ost->enc;
	AVDictionary* opt = NULL;
	av_dict_copy(&opt, opt_arg, 0);
	av_dict_set(&opt, "preset", "ultrafast", 0);
	av_dict_set(&opt, "crf", "1", 0);
	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not open video codec: %d\n", ret);
		exit(1);
	}
	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) 
	{
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	
	//Source frame
	ost->src_frame = NULL;
	ost->src_frame = alloc_picture(AV_PIX_FMT_RGB24, c->width, c->height);
	if (!ost->src_frame)
	{
		fprintf(stderr, "Could not allocate temporary picture\n");
		exit(1);
	}
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
}

static AVFrame* get_video_frame(OutputStream* ost, const uint8_t* pixels)
{
	AVCodecContext* c = ost->enc;

	// The AVFrame data will be stored as RGBRGBRGB... row-wise,
	// from left to right and from top to bottom.
	AVFrame* rgbpic = ost->src_frame;
	for (int y = 0; y < c->height; y++)
	{
		for (int x = 0; x < c->width; x++)
		{
			// rgbpic->linesize[0] is equal to width.
			rgbpic->data[0][y * rgbpic->linesize[0] + 3 * x + 0] = pixels[y * 4 * c->width + 4 * x + 2];
			rgbpic->data[0][y * rgbpic->linesize[0] + 3 * x + 1] = pixels[y * 4 * c->width + 4 * x + 1];
			rgbpic->data[0][y * rgbpic->linesize[0] + 3 * x + 2] = pixels[y * 4 * c->width + 4 * x + 0];
		}
	}
	sws_scale(ost->sws_ctx, (const uint8_t* const*)ost->src_frame->data, ost->src_frame->linesize, 0, c->height, ost->frame->data, ost->frame->linesize);
	ost->frame->pts = ost->pts;
	ost->pts += 1;
	return ost->frame;
}


/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext* oc, OutputStream* ost, const uint8_t* pixels)
{
	int ret;
	AVCodecContext* c;
	AVFrame* frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };
	c = ost->enc;
	frame = get_video_frame(ost, pixels);
	av_init_packet(&pkt);
	/* encode the image */
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) 
	{
		fprintf(stderr, "Error encoding video frame: %d\n", ret);
		exit(1);
	}
	if (got_packet) 
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	else 
		ret = 0;
	if (ret < 0) 
	{
		fprintf(stderr, "Error while writing video frame: %d\n", ret);
		exit(1);
	}
	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(OutputStream* ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->src_frame);
	sws_freeContext(ost->sws_ctx);
	ost->sws_ctx = NULL;
	ost->pts = 0;
	ost->st = NULL;
}

BOOL beginVideoEnc(char *outputFile, char* audioFile, VideoFormat vidFmt, BOOL  _bVideo)
{
	encode_audio = encode_video = true;
	AVDictionary* opt = NULL;
	BOOL ret;
	/* allocate the output media context */
	avformat_alloc_output_context2(&output_format_context, NULL, NULL, "dummy.mov");
	if (!output_format_context)
		return FALSE;
	fmt = output_format_context->oformat;
	
	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	add_stream(&video_st, output_format_context, &video_codec, AV_CODEC_ID_H264, vidFmt);
		
	if (encode_audio)
	{
		/** Open the input file to read from it. */
		if ((ret = avformat_open_input(&audio_input_format_context, audioFile, NULL, NULL)))
		{
			fprintf(stderr, "Could not open input file '%s' (error '%d')\n", audioFile, ret);
			audio_input_format_context = NULL;
			return ret;
		}

		/** Get information on the input file (number of streams etc.). */
		if ((ret = avformat_find_stream_info(audio_input_format_context, NULL)))
		{
			fprintf(stderr, "Could not open find stream info (error '%d')\n", ret);
			avformat_close_input(&audio_input_format_context);
			return ret;
		}
		add_stream(&audio_st, output_format_context, &audio_codec, AV_CODEC_ID_PCM_S16LE, vidFmt);
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if (encode_video)
		open_video(output_format_context, video_codec, &video_st, opt);
	if (encode_audio)
		open_audio(output_format_context, audio_codec, &audio_st, opt);
	av_dump_format(output_format_context, 0, outputFile, 1);
	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&output_format_context->pb, outputFile, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			fprintf(stderr, "Could not open '%s': %d\n", outputFile,
				ret);
			return FALSE;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(output_format_context, &opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Error occurred when opening output file: %d\n", ret);
		return FALSE;
	}
	return TRUE;
}

BOOL writeFrame(const DWORD *sourceVideoFrame, double audioOffset)
{
	if (encode_video)
		encode_video = !write_video_frame(output_format_context, &video_st, (uint8_t*)sourceVideoFrame);
	while (encode_audio && (!encode_video || av_compare_ts(video_st.pts, video_st.enc->time_base, audio_st.pts, audio_st.enc->time_base)) > 0)
		encode_audio = !write_audio_frame(output_format_context, &audio_st);
	return encode_audio | encode_video;
}

void endVideoEnc()
{
	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(output_format_context);
	/* Close each codec. */
	if (encode_video)
		close_stream(&video_st);
	if (encode_audio)
	{
		if (audio_input_format_context)
			avformat_close_input(&audio_input_format_context);
		close_stream(&audio_st);
	}
	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&output_format_context->pb);
	/* free the stream */
	avformat_free_context(output_format_context);
}


