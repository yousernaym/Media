#include "MfStuff.h"
#include "encoding.h"
#include <Codecapi.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "movie.h"
#include "audioTranscoding.h"

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC

const float PI = 3.1415926535f;
const float PId2 = PI / 2.0f;

VideoFormat videoFormat;

typedef struct OutputStream {
	AVStream* st;
	AVCodecContext* enc;
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	AVFrame* frame;
	AVFrame* src_frame;
	float t, tincr, tincr2;
	struct SwsContext* sws_ctx;
	struct SwrContext* swr_ctx;
} OutputStream;

MovieWriter* writer = NULL;
OutputStream video_st = { 0 }, audio_st = { 0 };
AVOutputFormat* fmt;
AVFormatContext* oc;
AVFormatContext* audioInputFmtCtx;
AVCodec* audio_codec, * video_codec;

int have_video = 0, have_audio = 0;
int encode_video = 0, encode_audio = 0;

//static int open_audio_file(const char* filename)
//{
//	int ret;
//	unsigned int i;
//	audioInputFmtCtx = NULL;
//	if ((ret = avformat_open_input(&audioInputFmtCtx, filename, NULL, NULL)) < 0)
//	{
//		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
//		return ret;
//	}
//	if ((ret = avformat_find_stream_info(audioInputFmtCtx, NULL)) < 0) 
//	{
//		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
//		return ret;
//	}
//	AVStream* stream;
//	AVCodecContext* codec_ctx;
//	stream = audioInputFmtCtx->streams[0];
//	//codec_ctx = stream->codec;
//	AVCodec* codec = avcodec_find_encoder(audioInputFmtCtx->audio_codec_id);
//	codec_ctx = avcodec_alloc_context3(codec);
//
//	if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
//	{
//		/* Open decoder */
//		ret = avcodec_open2(codec_ctx, avcodec_find_decoder(codec_ctx->codec_id), NULL);
//		if (ret < 0) 
//		{
//			av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
//			return ret;
//		}
//	}
//	av_dump_format(audioInputFmtCtx, 0, filename, 0);
//	return 0;
//}

static int write_frame(AVFormatContext* fmt_ctx, const AVRational* time_base, AVStream* st, AVPacket* pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;
	/* Write the compressed frame to the media file. */
	//log_packet(fmt_ctx, pkt);
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
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		//c->bit_rate = 384000;
		c->sample_rate = 48000;
		/*if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 48000)
					c->sample_rate = 48000;
			}
		}*/
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
		//c->bit_rate = 400000;
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

		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B-frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
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
static AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt,
	uint64_t channel_layout,
	int sample_rate, int nb_samples)
{
	AVFrame* frame = av_frame_alloc();
	int ret;
	if (!frame) {
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}
	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;
	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}
	return frame;
}

static void open_audio(AVFormatContext* oc, AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
	AVCodecContext* encCtx;
	AVCodecContext* decCtx;
	int nb_samples;
	int ret;
	AVDictionary* opt = NULL;
	encCtx = ost->enc;
	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(encCtx, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open audio codec: %d\n", ret);
		exit(1);
	}
	/* init signal generator */
	ost->t = 0;
	ost->tincr = 2 * M_PI * 110.0 / encCtx->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / encCtx->sample_rate / encCtx->sample_rate;
	if (encCtx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = encCtx->frame_size;
	ost->frame = alloc_audio_frame(encCtx->sample_fmt, encCtx->channel_layout,
		encCtx->sample_rate, nb_samples);
	ost->src_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, encCtx->channel_layout,
		encCtx->sample_rate, nb_samples);
	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, encCtx);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		fprintf(stderr, "Could not allocate resampler context\n");
		exit(1);
	}
	/* set options */
	av_opt_set_int(ost->swr_ctx, "in_channel_count", encCtx->channels, 0);
	av_opt_set_int(ost->swr_ctx, "in_sample_rate", encCtx->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", encCtx->sample_fmt, 0);
	av_opt_set_int(ost->swr_ctx, "out_channel_count", encCtx->channels, 0);
	av_opt_set_int(ost->swr_ctx, "out_sample_rate", encCtx->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", encCtx->sample_fmt, 0);
	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		fprintf(stderr, "Failed to initialize the resampling context\n");
		exit(1);
	}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame* get_audio_frame(OutputStream* ost)
{
	AVFrame* frame = ost->src_frame;
	int j, i, v;
	int16_t* q = (int16_t*)frame->data[0];
	/* check if we want to generate more frames */
	/*if (av_compare_ts(ost->next_pts, ost->enc->time_base,
		STREAM_DURATION, { 1, 1 }) >= 0)
		return NULL;*/
	for (j = 0; j < frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->enc->channels; i++)
			*q++ = v;
		ost->t += ost->tincr;
		ost->tincr += ost->tincr2;
	}
	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;
	return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext* oc, OutputStream* ost)
{
	AVCodecContext* c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame* frame;
	int ret;
	int got_packet;
	int dst_nb_samples;
	av_init_packet(&pkt);
	c = ost->enc;
	frame = get_audio_frame(ost);
	if (frame) 
	{
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = (int)av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples, c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);
		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);
		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx, ost->frame->data, dst_nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
		if (ret < 0) 
		{
			fprintf(stderr, "Error while converting\n");
			exit(1);
		}
		frame = ost->frame;
		frame->pts = av_rescale_q(ost->samples_count, { 1, c->sample_rate }, c->time_base);
		ost->samples_count += dst_nb_samples;
	}
	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) 
	{
		fprintf(stderr, "Error encoding audio frame: %d\n", ret);
		exit(1);
	}
	if (got_packet) 
	{
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) 
		{
			fprintf(stderr, "Error while writing audio frame: %d\n",
				ret);
			exit(1);
		}
	}
	return (frame || got_packet) ? 0 : 1;
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
	if (ret < 0) {
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
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec: %d\n", ret);
		exit(1);
	}
	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
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

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame* pict, int frame_index,
	int width, int height)
{
	int x, y, i, ret;
	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	ret = av_frame_make_writable(pict);
	if (ret < 0)
		exit(1);
	i = frame_index;
	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

static AVFrame* get_video_frame(OutputStream* ost, const uint8_t* pixels)
{
	AVCodecContext* c = ost->enc;
	/* check if we want to generate more frames */
	/*if (av_compare_ts(ost->next_pts, c->time_base,
		STREAM_DURATION, { 1, 1 }) >= 0)
		return NULL;*/

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
	//fill_yuv_image(ost->src_frame, ost->next_pts, c->width, c->height);
	sws_scale(ost->sws_ctx,
		(const uint8_t* const*)ost->src_frame->data, ost->src_frame->linesize,
		0, c->height, ost->frame->data, ost->frame->linesize);
	
	ost->frame->pts = ost->next_pts++;
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
	if (ret < 0) {
		fprintf(stderr, "Error encoding video frame: %d\n", ret);
		exit(1);
	}
	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	}
	else {
		ret = 0;
	}
	if (ret < 0) {
		fprintf(stderr, "Error while writing video frame: %d\n", ret);
		exit(1);
	}
	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext* oc, OutputStream* ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->src_frame);
	sws_freeContext(ost->sws_ctx);
	ost->sws_ctx = NULL;
	swr_free(&ost->swr_ctx);
}


BOOL beginVideoEnc(char *outputFile, char *audioFile, VideoFormat vidFmt, BOOL  _bVideo)
{
	/*writer = new MovieWriter(outputFile, vidFmt.width, vidFmt.height);
	return TRUE;*/
	AVDictionary* opt = NULL;
	BOOL ret;
	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, "dummy.mov");
	if (!oc)
		return FALSE;
	fmt = oc->oformat;
	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	have_video = 1;
	encode_video = 1;
	fmt = oc->oformat;
	add_stream(&video_st, oc, &video_codec, AV_CODEC_ID_H264, vidFmt);
	//add_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_PCM_S24LE, vidFmt);
	//add_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_AAC, vidFmt);
	/*if (initAudio(audioFile, oc))
		return FALSE;*/
	have_audio = 1;
	encode_audio = 1;
	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if (have_video)
		open_video(oc, video_codec, &video_st, opt);
	//if (have_audio)
		//open_audio(oc, audio_codec, &audio_st, opt);
	av_dump_format(oc, 0, outputFile, 1);
	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, outputFile, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s': %d\n", outputFile,
				ret);
			return FALSE;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) 
	{
		fprintf(stderr, "Error occurred when opening output file: %d\n", ret);
		return FALSE;
	}
	return TRUE;
}

BOOL writeFrame(const DWORD *sourceVideoFrame, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset)
{
	if (writer)
	{
		writer->addFrame((uint8_t*)sourceVideoFrame);
		return TRUE;
	}
	//if (encode_video &&
	//	(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
	//		audio_st.next_pts, audio_st.enc->time_base) <= 0)) 
	//{
	///*	(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
	//	getAudioPts(), getAudioTimeBase()) <= 0)) 
	//{*/
	//	encode_video = !write_video_frame(oc, &video_st, (uint8_t*)sourceVideoFrame);
	//}
	//else {
	//	encode_audio = !write_audio_frame(oc, &audio_st);
	//	//encode_audio = !writeAudioFrame(oc);
	//}

	encode_video = !write_video_frame(oc, &video_st, (uint8_t*)sourceVideoFrame);
	//encode_audio = !writeAudioFrame(oc);
	return encode_audio | encode_video;
}
//
//HRESULT writeSamples(IMFSample *pSample, LONGLONG rtStart, LONGLONG rtDuration, LONGLONG audioOffset)
//{
//	HRESULT hr = S_OK;
//	//Video sample----------------------
//	// Create a media sample and add the buffer to the sample.
//    if (bVideo)
//	{
//		// Set the time stamp and the duration.
//		if (SUCCEEDED(hr))
//			hr = pSample->SetSampleTime(rtStart);
//		if (SUCCEEDED(hr))
//			hr = pSample->SetSampleDuration(rtDuration);
//   
//		// Send the sample to the Sink Writer.
//		if (SUCCEEDED(hr))
//			hr = pWriter->WriteSample(videoStreamIndex, pSample);
//	}
//
//	//---------------------------------------------
//	//Audio sample
//	//----------------------------------------------
//	if (bAudio)
//	{
//		IMFSample *pSrSample=0;
//		if (audioSrstreamFlags & MF_SOURCE_READERF_ENDOFSTREAM || rtStart < audioOffset - rtDuration)
//		{
//			if (SUCCEEDED(hr))
//				hr = pEmptyAudioSample->SetSampleDuration(rtDuration);
//			if (SUCCEEDED(hr))
//				hr = pEmptyAudioSample->SetSampleTime(rtStart);
//			if (SUCCEEDED(hr))
//				hr = pWriter->WriteSample(audioStreamIndex, pEmptyAudioSample);
//		}
//		else
//		{
//			while (lastAudioTimeStamp + audioOffset <= rtStart)
//			{
//				if (SUCCEEDED(hr))
//					hr = pSourceReader->ReadSample(0, 0, 0, &audioSrstreamFlags, &lastAudioTimeStamp, &pSrSample);
//				if (pSrSample && lastAudioTimeStamp + audioOffset < 0)
//					continue;
//				if (!pSrSample)
//					break;
//				if (SUCCEEDED(hr))
//					hr = pSrSample->SetSampleTime(lastAudioTimeStamp + audioOffset);
//				if (SUCCEEDED(hr))
//					hr = pWriter->WriteSample(audioStreamIndex, pSrSample);
//				SafeRelease(&pSrSample);
//				if (FAILED(hr))
//					break;
//			};
//		}
//	}
//	rtStart += rtDuration;
////}
//	//----------------------------------------------
//	return hr;
//}

void endVideoEnc()
{
	if (writer)
	{
		delete writer;
		writer = NULL;
		return;
	}
	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(oc);
	/* Close each codec. */
	if (have_video)
		close_stream(oc, &video_st);
	audioCleanup(&oc);
	return;
	//if (have_audio)
		//close_stream(oc, &audio_st);
	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc->pb);
	/* free the stream */
	avformat_free_context(oc);
}


