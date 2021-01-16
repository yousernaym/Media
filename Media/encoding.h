#pragma once
#include "mfstuff.h"

#pragma pack(push, 8)
struct VideoFormat
{
	UINT32 width;
	UINT32 height;
	float fps;
	UINT32 bitRate;
	UINT32 audioSampleRate;
	UINT32 aspectNumerator;
	UINT32 aspectDenominator;
};
#pragma pack(pop)

extern "C"
{
	__declspec(dllexport) BOOL beginVideoEnc(char *outputFile, char *audioFile, VideoFormat vidFmt, double audioOffsetSeconds, BOOL spherical, BOOL spherical_stereo, enum AVCodecID video_codec_id, const char* crf);
	__declspec(dllexport) BOOL writeFrame(const DWORD *videoFrameBuffer);
	__declspec(dllexport) void endVideoEnc();
}
