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

HRESULT createVideoSampleAndBuffer(IMFSample **ppSample, IMFMediaBuffer **ppBuffer);
HRESULT createEmptyAudioSample(IMFSample **ppSample);
//BOOL beginVideoEnc(char * outputFile, VideoFormat vidFmt, BOOL _bVideo);
HRESULT copyVideoFrameToBuffer(IMFMediaBuffer *pBuffer, const DWORD *source);
HRESULT createAudioBuffer(IMFMediaBuffer **ppBuffer);
HRESULT writeSamples(IMFSample *pSample, LONGLONG rtStart, LONGLONG rtDuration, LONGLONG audioOffset);

extern "C"
{
	__declspec(dllexport) BOOL beginVideoEnc(char *outputFile, char *audioFile, VideoFormat vidFmt, BOOL  _bVideo);
	__declspec(dllexport) BOOL writeFrameCube(DWORD *destVideoFrame, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset, const DWORD *cmFace0, const DWORD *cmFace1, const DWORD *cmFace2, const DWORD *cmFace3, const DWORD *cmFace4, const DWORD *cmFace5, int cmFaceSide, int videoFrameX, int videoFrameY);
	__declspec(dllexport) BOOL writeFrame(const DWORD *videoFrameBuffer, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset);
	__declspec(dllexport) void endVideoEnc();
}
