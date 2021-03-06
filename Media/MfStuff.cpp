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
BOOL initMF()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
		return FALSE;
    hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		CoUninitialize();
		return FALSE;
	}
	
	return TRUE;
}

BOOL closeMF()
{
	BOOL b = closeAudioFile();
	//if (b)
	b = SUCCEEDED(MFShutdown());
	CoUninitialize();
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

