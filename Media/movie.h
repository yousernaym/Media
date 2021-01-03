// Adapted from https://stackoverflow.com/questions/34511312/how-to-encode-a-video-from-several-images-generated-in-a-c-program-without-wri

#ifndef MOVIE_H
#define MOVIE_H

#include <stdint.h>
#include <string>
#include <vector>

extern "C"
{
	//#include <x264.h>
	#include <libswscale/swscale.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/mathematics.h>
	#include <libavformat/avformat.h>
	#include <libavutil/opt.h>
	#include <libswresample/swresample.h>
	#include <libavutil/avassert.h>
}

class MovieWriter
{
	const unsigned int width, height;
	unsigned int iframe;

	SwsContext* swsCtx;
	AVOutputFormat* fmt;
	AVStream* stream;
	AVFormatContext* fc;
	AVCodecContext* c;
	AVPacket pkt;
	AVRational srcTimeBase;

	AVFrame *rgbpic, *yuvpic;

	std::vector<uint8_t> pixels;

public :

	MovieWriter(const std::string& filename, const unsigned int width, const unsigned int height);

	void addFrame(const uint8_t* pixels);
	
	~MovieWriter();
};

#endif // MOVIE_H

