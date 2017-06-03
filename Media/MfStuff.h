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

extern "C"
{
	__declspec(dllexport) BOOL initMF();
	__declspec(dllexport) BOOL closeMF();
	__declspec(dllexport) char *getAudioFilePath();
	__declspec(dllexport) BOOL openAudioFile(char* file);
	__declspec(dllexport) BOOL closeAudioFile();
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