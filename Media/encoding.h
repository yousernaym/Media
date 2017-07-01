#pragma once
#include "mfstuff.h"

#pragma pack(push, 8)
struct VideoFormat
{
	UINT32 width;
	UINT32 height;
	UINT32 fps;
	UINT32 bitRate;
	UINT32 audioSampleRate;
};
#pragma pack(pop)

HRESULT createVideoSampleAndBuffer(IMFSample **ppSample, IMFMediaBuffer **ppBuffer);
HRESULT createEmptyAudioSample(IMFSample **ppSample);
//bool beginVideoEnc(char * outputFile, VideoFormat vidFmt, bool _bVideo);
HRESULT copyVideoFrameToBuffer(IMFMediaBuffer *pBuffer, const DWORD *source);
HRESULT createAudioBuffer(IMFMediaBuffer **ppBuffer);
HRESULT writeSamples(IMFSample *pSample, LONGLONG rtStart, LONGLONG rtDuration, LONGLONG audioOffset, bool bFlush);

extern "C"
{
	__declspec(dllexport) bool beginVideoEnc(char *outputFile, VideoFormat vidFmt, bool  _bVideo);
	__declspec(dllexport) bool writeFrameCube(DWORD *destVideoFrame, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset, bool bFlush, const DWORD *cmFace0, const DWORD *cmFace1, const DWORD *cmFace2, const DWORD *cmFace3, const DWORD *cmFace4, const DWORD *cmFace5, int cmFaceSide, int videoFrameX, int videoFrameY);
	__declspec(dllexport) bool writeFrame(const DWORD *videoFrameBuffer, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset, bool bFlush);
	__declspec(dllexport) void endVideoEnc();
}
