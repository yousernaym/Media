#pragma once
extern "C"
{
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
	#include "libavcodec/avcodec.h"
	#include "libavutil/audio_fifo.h"
	#include "libavutil/avassert.h"
	#include "libavutil/avstring.h"
	#include "libavutil/frame.h"
	#include "libavutil/opt.h"
	#include "libswresample/swresample.h"
}
void audioCleanup();
int writeAudioFrame(AVFormatContext* output_format_context);
int initAudio(const char* audioFilename, AVFormatContext* output_format_context);
AVRational getAudioTimeBase();
int64_t getAudioPts();
AVFormatContext* getInputFormatContext();
AVCodecContext* getInputCodecContext();
int decode_audio_frame(AVFrame* frame, AVFormatContext* input_format_context, AVCodecContext* input_codec_context, int* data_present, int* finished);

