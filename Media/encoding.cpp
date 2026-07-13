/*
Video file creation, based mostly on Ffmpeg muxing example:
https://ffmpeg.org/doxygen/trunk/muxing_8c_source.html
With a few lines from the AAC Transcoding example:
https://ffmpeg.org/doxygen/3.4/transcode__aac_8c_source.html
and Moviemaker-cpp:
https://github.com/apc-llc/moviemaker-cpp
*/

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
	#include <libavutil/channel_layout.h>
	#include <libavformat/avio.h>
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
	int64_t dest_pts;
} OutputStream;

static OutputStream video_st = { 0 }, audio_st = { 0 };
static const AVOutputFormat* fmt;
static AVFormatContext* output_format_context = NULL;
static AVFormatContext* audio_input_format_context = NULL;
static const AVCodec* video_codec = NULL;
static int encode_video, have_video, encode_audio, have_audio;
static int64_t audioOffsetTimestamp;
// Input audio stream time base, used to rescale timestamps when the audio is stream-copied
// (no encoder — e.g. MP3/WMA masters that FFmpeg can mux but not encode).
static AVRational audio_in_time_base;

static int write_frame(AVFormatContext* fmt_ctx, OutputStream* ost, AVPacket* pkt)
{
	/* Rescale packet timestamps to the output stream's timebase. For an encoded stream the source
	 * timebase is the encoder's; for a stream-copied audio track (no encoder) it's the input
	 * stream's timebase (the muxer may have changed ost->st->time_base while writing the header). */
	AVRational src_time_base = ost->enc ? ost->enc->time_base : audio_in_time_base;
	av_packet_rescale_ts(pkt, src_time_base, ost->st->time_base);
	ost->dest_pts = pkt->pts;
	pkt->stream_index = ost->st->index;
	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static BOOL add_stream(OutputStream* ost, AVFormatContext* oc, const AVCodec** codec, enum AVCodecID codec_id, const VideoFormat &vidFmt)
{
	AVCodecContext* c;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec))
	{
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		return FALSE;
	}
	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) 
	{
		fprintf(stderr, "Could not allocate stream\n");
		return FALSE;
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c)
	{
		fprintf(stderr, "Could not alloc an encoding context\n");
		return FALSE;
	}
	ost->enc = c;
	switch ((*codec)->type)
	{
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		/* Resolution must be a multiple of two. */
		c->width = vidFmt.width;
		c->height = vidFmt.height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		c->time_base = ost->st->time_base = { 100, (int)(vidFmt.fps * 100) };
		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = AV_PIX_FMT_YUV420P;

		ost->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
		if (!ost->sws_ctx)
		{
			fprintf(stderr,
				"Could not initialize the conversion context\n");
			return FALSE;
		}
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	return TRUE;
}

/* Add the audio output stream. The audio is always stream-copied (write_audio_frame remuxes the
 * input packets unchanged), so no encoder is involved: the input stream's codec parameters — which
 * carry the true sample rate, channel layout and extradata — are copied verbatim. ost->enc stays
 * NULL; write_frame uses audio_in_time_base to rescale timestamps for this stream. */
static BOOL add_audio_stream_copy(OutputStream* ost, AVFormatContext* oc, AVStream* in_stream)
{
	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st)
	{
		fprintf(stderr, "Could not allocate audio stream\n");
		return FALSE;
	}
	ost->st->id = oc->nb_streams - 1;
	if (avcodec_parameters_copy(ost->st->codecpar, in_stream->codecpar) < 0)
	{
		fprintf(stderr, "Could not copy audio codec parameters\n");
		return FALSE;
	}
	ost->st->codecpar->codec_tag = 0;   // let the muxer pick a tag valid for the output container
	ost->st->time_base = in_stream->time_base;
	ost->enc = NULL;                    // no encoder: this stream is copied, not encoded
	return TRUE;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext* oc, OutputStream* ost)
{
	AVPacket pkt = { 0 }; // data and size must be 0;
	int ret;

	/* av_init_packet is deprecated; the zero-initialised packet above is filled
	 * in by av_read_frame, which is all this stack packet is used for. */
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
	
	//If first packet and audio should be delayed, fill beginning with silence
	static int64_t ptsOffset;
	static int64_t posOffset;
	if (pkt.pts == 0)
	{
		ptsOffset = 0;
		posOffset = 0;
	}
	if (audioOffsetTimestamp > 0 && pkt.pts == 0)
	{
		posOffset = pkt.pos;
		int audioSilentPackets = (int)(audioOffsetTimestamp / pkt.duration);
		while (audioSilentPackets-- > 0)
		{
			//It seems the packet gets unusable after the call to av_write_interleaved, so allocate/deallocate every iteration
			AVPacket* silentPkt = av_packet_alloc();
			silentPkt->data = (uint8_t*)calloc(pkt.size, 1);
			silentPkt->pts = silentPkt->dts = ptsOffset;
			silentPkt->pos = posOffset;
			silentPkt->size = pkt.size;
			silentPkt->duration = pkt.duration;
			silentPkt->flags = pkt.flags;
			ret = write_frame(output_format_context, ost, silentPkt);
			ptsOffset += pkt.duration;
			posOffset += pkt.size;
			free(silentPkt->data);  //Is this really needed or is ownership transferred to FFmpeg?
			av_packet_free(&silentPkt);
		}
	}
	
	pkt.pts += ptsOffset;
	pkt.dts += ptsOffset;
	pkt.pos += posOffset;
	if (pkt.pts < 0)
		return 0;
	
	ret = write_frame(output_format_context, ost, &pkt);
	if (ret < 0) 
		fprintf(stderr, "Error while writing audio frame: %d\n", ret);

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
		return NULL;
	}
	return picture;
}

static BOOL open_video(AVFormatContext* oc, const AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg, const char* crf)
{
	int ret;
	AVCodecContext* c = ost->enc;
	AVDictionary* opt = NULL;
	av_dict_copy(&opt, opt_arg, 0);

	av_dict_set(&opt, "crf", crf, 0); //Constant qualitty mode

	if (codec->id == AV_CODEC_ID_H264)
		av_dict_set(&opt, "preset", "veryfast", 0);
	else if (codec->id == AV_CODEC_ID_VP9)
	{
		av_dict_set(&opt, "b:v", "0", 0); //Constant qualitty mode
		av_dict_set(&opt, "deadline", "good", 0); //Encoding speed vs compression
		av_dict_set(&opt, "quality", "good", 0); //Same as deadline?
		av_dict_set(&opt, "row-mt", "1", 0); //Row-based multithreading
		if (!strcmp(crf, "0"))
			av_dict_set(&opt, "lossless", "1", 0);
	}

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not open video codec: %d\n", ret);
		return FALSE;
	}
	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) 
	{
		fprintf(stderr, "Could not allocate video frame\n");
		return FALSE;
	}
	
	//Source frame
	ost->src_frame = alloc_picture(AV_PIX_FMT_RGB24, c->width, c->height);
	if (!ost->src_frame)
	{
		fprintf(stderr, "Could not allocate temporary picture\n");
		return FALSE;
	}
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not copy the stream parameters\n");
		return FALSE;
	}
	return TRUE;
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
	ost->frame->pts = ost->pts++;
	return ost->frame;
}


/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext* oc, OutputStream* ost, const uint8_t* pixels)
{
	int ret;
	AVCodecContext* c = ost->enc;
	AVFrame* frame = get_video_frame(ost, pixels);

	/* hand the frame to the encoder (FFmpeg 5+ send/receive API; the old
	 * avcodec_encode_video2 was removed in FFmpeg 5.0) */
	ret = avcodec_send_frame(c, frame);
	if (ret < 0)
	{
		fprintf(stderr, "Error sending video frame to encoder: %d\n", ret);
		exit(1);
	}
	/* drain whatever packets are now available */
	AVPacket* pkt = av_packet_alloc();
	while (ret >= 0)
	{
		ret = avcodec_receive_packet(c, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0)
		{
			fprintf(stderr, "Error encoding video frame: %d\n", ret);
			av_packet_free(&pkt);
			exit(1);
		}
		if (write_frame(oc, ost, pkt) < 0)
		{
			fprintf(stderr, "Error while writing video frame: %d\n", ret);
			av_packet_free(&pkt);
			exit(1);
		}
		av_packet_unref(pkt);
	}
	av_packet_free(&pkt);
	return 0;
}

static void close_stream(OutputStream* ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->src_frame);
	sws_freeContext(ost->sws_ctx);
	ost->sws_ctx = NULL;
	ost->pts = ost->dest_pts = 0;
	ost->st = NULL;
}

void freeResources()
{
	/* Close each codec. */
	if (have_video)
		close_stream(&video_st);
	if (have_audio)
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

BOOL beginVideoEnc(char *outputFile, char* audioFile, VideoFormat vidFmt, double audioOffsetSeconds, BOOL spherical, BOOL spherical_stereo, AVCodecID video_codec_id, const char* crf)
{
	encode_video = have_video = 1;
	encode_audio = have_audio = audioFile != NULL && strlen(audioFile) > 0;
	AVDictionary* opt = NULL;
	BOOL ret;
	/* allocate the output media context */
	avformat_alloc_output_context2(&output_format_context, NULL, NULL, outputFile);
	if (!output_format_context)
	{
		freeResources();
		return FALSE;
	}
	fmt = output_format_context->oformat;
	
	/* Add the video stream using the default format codecs
	 * and initialize the codec. */
	if (!add_stream(&video_st, output_format_context, &video_codec, video_codec_id, vidFmt))
	{
		freeResources();
		return FALSE;
	}
	//Spherical metadata
	if (spherical)
	{
		char stereo_arg[100];
		if (spherical_stereo)
			strcpy_s(stereo_arg, sizeof(stereo_arg), "<GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>");
		else
			stereo_arg[0] = 0;
		char metadata_string[1000];
		sprintf_s(metadata_string, sizeof(metadata_string), "<rdf:SphericalVideo xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\"> <GSpherical:Spherical>true</GSpherical:Spherical> <GSpherical:Stitched>true</GSpherical:Stitched> <GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType> %s </rdf:SphericalVideo>", stereo_arg);
		av_dict_set(&video_st.st->metadata, "spherical-video", metadata_string, 0);
	}

	if (encode_audio)
	{
		/** Open the input file to read from it. */
		if ((ret = avformat_open_input(&audio_input_format_context, audioFile, NULL, NULL)))
		{
			fprintf(stderr, "Could not open input file '%s' (error '%d')\n", audioFile, ret);
			freeResources();
			return FALSE;
		}

		/** Get information on the input file (number of streams etc.). */
		if ((ret = avformat_find_stream_info(audio_input_format_context, NULL)))
		{
			fprintf(stderr, "Could not open find stream info (error '%d')\n", ret);
			freeResources();
			return FALSE;
		}

		/* The audio is always stream-copied into the output (write_audio_frame just remuxes the
		 * input packets — nothing is re-encoded). The old code nevertheless allocated and opened an
		 * *encoder* for the input's codec id, deriving the sample rate from time_base.den. That only
		 * works for formats whose demuxer uses a 1/sample_rate timebase (WAV, M4A); for MP3 the
		 * bogus rate made the encoder fail to open ("couldn't initialize video encoding"), and for
		 * WMA (ASF timebase = 1/1000) it silently tagged the stream as 1000 Hz. Copying the input
		 * stream's codec parameters instead is correct for every format the container accepts. */
		AVStream* audio_in_stream = audio_input_format_context->streams[0];
		audio_in_time_base = audio_in_stream->time_base;
		if (!add_audio_stream_copy(&audio_st, output_format_context, audio_in_stream))
		{
			freeResources();
			return FALSE;
		}
		/* Offset in input-stream timebase units: the silence-padding loop in write_audio_frame
		 * counts packets via pkt.duration, which av_read_frame reports in that timebase. */
		audioOffsetTimestamp = (int64_t)(audioOffsetSeconds / av_q2d(audio_in_time_base));
	}

	/* Now that all the parameters are set, we can open the video codec and
	 * allocate the necessary encode buffers. */
	if (encode_video)
	{
		if (!open_video(output_format_context, video_codec, &video_st, opt, crf))
		{
			freeResources();
			return FALSE;
		}
	}
	av_dump_format(output_format_context, 0, outputFile, 1);
	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&output_format_context->pb, outputFile, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			fprintf(stderr, "Could not open '%s': %d\n", outputFile, ret);
			freeResources();
			return FALSE;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(output_format_context, &opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Error occurred when opening output file: %d\n", ret);
		freeResources();
		return FALSE;
	}
	return TRUE;
}

BOOL writeFrame(const DWORD *sourceVideoFrame)
{
	if (encode_video)
		encode_video = !write_video_frame(output_format_context, &video_st, (uint8_t*)sourceVideoFrame);
	while (encode_audio && (!encode_video || video_st.dest_pts > audio_st.dest_pts))
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
	freeResources();
}

