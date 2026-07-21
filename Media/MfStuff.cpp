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
// Not a nest count: callers must pair one closeMF per successful initMF. A second initMF
// before closeMF overwrites this flag, so only one CoUninitialize runs (app/tests call once).
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
	// Paths arrive as UTF-8 from the managed side (LPUTF8Str). Use CP_UTF8 rather than
	// mbstowcs_s, which follows the CRT locale / ANSI code page and would corrupt
	// non-ASCII filenames (e.g. under a non-Latin user profile).
	if (!source)
	{
		dest[0] = 0;
		return 0;
	}
	int written = MultiByteToWideChar(CP_UTF8, 0, source, -1, dest, maxFileNameLength);
	if (written <= 0)
	{
		dest[0] = 0;
		return 0;
	}
	return static_cast<size_t>(written);
}

HRESULT waitForEvent(IMFMediaEventGenerator *pGenerator, MediaEventType type)
{
	// Block until the target event arrives, or until an async failure is reported.
	// MEError (and any event with a failed GetStatus) must exit — otherwise a missing
	// audio endpoint leaves StartPlayback spinning forever on the event queue.
	for (;;)
	{
		IMFMediaEvent *pEvent = nullptr;
		HRESULT hr = pGenerator->GetEvent(0, &pEvent);
		if (FAILED(hr))
			return hr;

		MediaEventType eventType = MEUnknown;
		hr = pEvent->GetType(&eventType);

		HRESULT eventStatus = S_OK;
		if (SUCCEEDED(hr))
			hr = pEvent->GetStatus(&eventStatus);

		SafeRelease(&pEvent);

		if (FAILED(hr))
			return hr;
		if (FAILED(eventStatus))
			return eventStatus;
		if (eventType == MEError)
			return E_FAIL;
		if (eventType == type)
			return S_OK;
	}
}

