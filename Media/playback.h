#pragma once
#include "MfStuff.h"

extern "C"
{
	__declspec(dllexport) double getAudioLength();
	__declspec(dllexport) BOOL playbackIsRunning();
	__declspec(dllexport) double getPlaybackPos();
	__declspec(dllexport) BOOL openFileForPlayback(char *file);
	__declspec(dllexport) BOOL startPlaybackAtTime(double timeS);
	__declspec(dllexport) BOOL startPlayback();
	__declspec(dllexport) BOOL stopPlayback();
	__declspec(dllexport) BOOL pausePlayback();
}

HRESULT createMediaSource(PCWSTR sURL, IMFMediaSource **ppSource);
HRESULT createPlaybackTopology(IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology);
HRESULT addBranchToPartialTopology(IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd);
HRESULT createMediaSinkActivate(IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate);
HRESULT addSourceNode(IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode);
HRESULT addOutputNode(IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode);
