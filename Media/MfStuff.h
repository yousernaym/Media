#pragma once

#include <InitGuid.h>
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <assert.h>
#include <iostream>
#include <vector>

//extern IMFMediaSession *pMediaSession;
#define __STDC_CONSTANT_MACROS

extern "C"
{
	__declspec(dllexport) BOOL initMF();
	__declspec(dllexport) BOOL closeMF();
	__declspec(dllexport) char *getAudioFilePath();
	__declspec(dllexport) BOOL openAudioFile(char* file);
	__declspec(dllexport) BOOL closeAudioFile();

	#include <libavutil/avassert.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/opt.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/timestamp.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>
}

const int maxFileNameLength = 1000;
size_t mbToWcString(wchar_t dest[], char source[]);
HRESULT waitForEvent(IMFMediaEventGenerator *pGenerator, MediaEventType type);

template <class T> void SafeRelease(T **ppT);
template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}