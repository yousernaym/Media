#include "MfStuff.h"

const int MAX_AUDIO_FILENAME_LENGTH = 1000;
char audioFileName[MAX_AUDIO_FILENAME_LENGTH];

//From encoding.cpp
BOOL openAudioFileForEncoding(const WCHAR *file); 
BOOL closeAudioFileForEncoding();
//From playback.cpp
BOOL openAudioFileForPlayback(const WCHAR *file); 
BOOL closeAudioFileForPlayback();

#pragma region dll_exports
// True when initMF's CoInitializeEx returned S_OK/S_FALSE (we must balance with CoUninitialize).
// False when COM was already on another apartment (RPC_E_CHANGED_MODE) — do not uninit.
static bool s_coInitOwned = false;

BOOL initMF()
{
	// S_FALSE = already initialized on this thread; RPC_E_CHANGED_MODE = different apartment
	// (e.g. MTA test host) — MF still works, so don't fail the call.
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	bool coInitOwned = SUCCEEDED(hr);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		return FALSE;
	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		if (coInitOwned)
			CoUninitialize();
		return FALSE;
	}

	s_coInitOwned = coInitOwned;
	return TRUE;
}

BOOL closeMF()
{
	BOOL b = closeAudioFile();
	//if (b)
	b = SUCCEEDED(MFShutdown());
	if (s_coInitOwned)
	{
		CoUninitialize();
		s_coInitOwned = false;
	}
	return b;
}

char *getAudioFilePath()
{
	return audioFileName;
}

BOOL openAudioFile(char* file)
{
	strcpy_s(audioFileName, file);
	if (!file || file[0] == 0)
		return TRUE;
	closeAudioFile();
	static wchar_t wfile[maxFileNameLength];
	mbToWcString(wfile, file);
	BOOL b = openAudioFileForPlayback(wfile);
	//BOOL b2 = openAudioFileForEncoding(wfile);
	return b;//&& b2;
}
BOOL closeAudioFile()
{
	BOOL b = closeAudioFileForPlayback();
	//BOOL b2 = closeAudioFileForEncoding();
	return b;//&& b2;
}
#pragma endregion

size_t mbToWcString(wchar_t dest[], char source[])
{
	size_t chars;
	mbstowcs_s(&chars, dest, maxFileNameLength, source, maxFileNameLength);
	return chars;
}

HRESULT waitForEvent(IMFMediaEventGenerator *pGenerator, MediaEventType type)
{
	IMFMediaEvent *pEvent = 0;
	HRESULT hr = S_OK;
	for(;;)
	{
		if (SUCCEEDED(hr))
			hr = pGenerator->GetEvent(0, &pEvent);
		MediaEventType eventType;
		if (SUCCEEDED(hr))
			hr = pEvent->GetType(&eventType);
		SafeRelease(&pEvent);
		if (eventType == type || FAILED(hr))
			break;
	}
	return hr;
}

