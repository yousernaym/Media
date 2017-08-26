#include "encoding.h"
#include <Codecapi.h>
#define _USE_MATH_DEFINES
#include <math.h>

const float PI = 3.1415926535f;
const float PId2 = PI / 2.0f;

IMFSourceReader *pSourceReader = 0;
IMFSinkWriter *pWriter = 0;
IMFMediaBuffer *pVideoFrameBuffer = 0;
IMFSample *pVideoFrameSample = 0;
//int currentVideoSample;
DWORD videoStreamIndex;
DWORD audioStreamIndex;
LONGLONG lastAudioTimeStamp;
IMFSample *pEmptyAudioSample = 0;
//int currentAudioSample;
DWORD audioSrstreamFlags;
bool bAudio;
bool bVideo;
bool bSingleVideoBuffer = false;

VideoFormat videoFormat;

HRESULT createVideoSampleAndBuffer(IMFSample **ppSample, IMFMediaBuffer **ppBuffer)
{
	IMFSample *pSample;
	IMFMediaBuffer *pBuffer;
	const LONG cbWidth = 4 * videoFormat.width;
	const DWORD cbBuffer = cbWidth * videoFormat.height;
	 
    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
			
	if (SUCCEEDED(hr))
		hr = MFCreateSample(&pSample);
	if (SUCCEEDED(hr))
		hr = pSample->AddBuffer(pBuffer);
	*ppBuffer = pBuffer;
	*ppSample = pSample;
	
	return hr;
}
HRESULT createEmptyAudioSample(IMFSample **ppSample)
{
	//int length = 3200;
	int length = 5880;
	IMFMediaBuffer *pBuffer;
	HRESULT hr=S_OK;
	hr = MFCreateMemoryBuffer(length, &pBuffer);
	BYTE *pData;
	if (SUCCEEDED(hr))
		hr = pBuffer->Lock(&pData, NULL, NULL);
	if (SUCCEEDED(hr))
	{
		ZeroMemory(pData, length);
		hr = pBuffer->Unlock();
	}
	if (SUCCEEDED(hr))
		hr = pBuffer->SetCurrentLength(length);

	if (SUCCEEDED(hr))
		hr = MFCreateSample(ppSample);
	if (SUCCEEDED(hr))
		hr = (*ppSample)->AddBuffer(pBuffer);
	return hr;
}

bool beginVideoEnc(char *outputFile, VideoFormat vidFmt, bool  _bVideo)
{
    lastAudioTimeStamp = 0;
	//currentVideoSample = 0;
	//currentAudioSample = 0;
	audioSrstreamFlags = 0;
	
	bVideo = _bVideo;

	static WCHAR wOutputFile[1000];
	mbToWcString(wOutputFile, outputFile);
	bAudio = pSourceReader != 0;

	videoFormat = vidFmt;
	IMFMediaType *pMediaType = NULL;
        
	//IMFAttributes *writerAttributes;
	//MFCreateAttributes(&writerAttributes, 1);
	//writerAttributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
	HRESULT hr = MFCreateSinkWriterFromURL(wOutputFile, NULL, NULL, &pWriter);
	//SafeRelease(&writerAttributes);
	
    if (bVideo)
	{
		if (SUCCEEDED(hr) && bSingleVideoBuffer)
			hr = createVideoSampleAndBuffer(&pVideoFrameSample, &pVideoFrameBuffer);
		
		// Set the sink video output media type.
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);   
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);     
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		 if(SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_AVG_BITRATE, videoFormat.bitRate);   
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
		if (SUCCEEDED(hr))
			hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, videoFormat.width, videoFormat.height);
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, videoFormat.fps, 1);
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		if (SUCCEEDED(hr))
		{		
			hr = pWriter->AddStream(pMediaType, &videoStreamIndex);   
			SafeRelease(&pMediaType);
		}
		
		 //Set the sink video input media type.
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);   
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);   
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
		if (SUCCEEDED(hr))
			hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, videoFormat.width, videoFormat.height);
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, videoFormat.fps, 1);   
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_PIXEL_ASPECT_RATIO, videoFormat.aspectNumerator, videoFormat.aspectDenominator);
    
		if (SUCCEEDED(hr))
		{
			hr = pWriter->SetInputMediaType(videoStreamIndex, pMediaType, NULL);   
			SafeRelease(&pMediaType);
		}
	}
    
    if (bAudio) //audio
	{
		//Set the sink audio output type
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
		//if (SUCCEEDED(hr))
			//hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_FLAC);
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, videoFormat.audioSampleRate);
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000 );
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, 2 );
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
		
		if (SUCCEEDED(hr))
		{
			hr = pWriter->AddStream(pMediaType, &audioStreamIndex);   
			SafeRelease(&pMediaType);
		}

		//Set the intermediate audio media type (sink input and source reader output)
	
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, videoFormat.audioSampleRate);
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, 2 );
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

		if (SUCCEEDED(hr))
			hr = pSourceReader->SetCurrentMediaType(0, 0, pMediaType);
		SafeRelease(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pSourceReader->GetCurrentMediaType(0, &pMediaType);

		if (SUCCEEDED(hr))
			hr = pWriter->SetInputMediaType(audioStreamIndex, pMediaType, NULL);
		SafeRelease(&pMediaType);	
	
		if (SUCCEEDED(hr))
			hr = createEmptyAudioSample(&pEmptyAudioSample);
	}

	// Tell the sink writer to start accepting data.
    if (SUCCEEDED(hr))
		hr = pWriter->BeginWriting();
    
    if (FAILED(hr))
		endVideoEnc();
    return SUCCEEDED(hr);
}

DWORD sampleCubeMap(float coordX, float coordY, float coordZ, const DWORD *face0, const DWORD *face1, const DWORD *face2, const DWORD *face3, const DWORD *face4, const DWORD *face5, int faceSide);
bool writeFrameCube(DWORD *videoFrameBuffer, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset, const DWORD *cmFace0, const DWORD *cmFace1, const DWORD *cmFace2, const DWORD *cmFace3, const DWORD *cmFace4, const DWORD *cmFace5, int cmFaceSide, int videoFrameX, int videoFrameY)
{
	for (int y = 0; y < videoFrameY; y++)
	{
		for (int x = 0; x < videoFrameX; x++)
		{
			float normX = 2.0f * x / videoFrameX - 1.0f;
			float normY = 2.0f * y / videoFrameY - 1.0f;
			float theta = -normX * PI;
			float phi = normY * PId2;
			//float phi = (float)Math.Asin(y);
			float cmCoordX = cos(phi) * cos(theta);
			float cmCoordY = sin(phi);
			float cmCoordZ = cos(phi) * sin(theta);
			videoFrameBuffer[y * videoFrameX + x] = sampleCubeMap(cmCoordX, cmCoordY, cmCoordZ, cmFace0, cmFace1, cmFace2, cmFace3, cmFace4, cmFace5, cmFaceSide);
		}
	}
	return writeFrame(videoFrameBuffer, rtStart, rtDuration, audioOffset);
}

DWORD sampleCubeMap(float coordX, float coordY, float coordZ, const DWORD *face0, const DWORD *face1, const DWORD *face2, const DWORD *face3, const DWORD *face4, const DWORD *face5, int faceSide)
{
	const DWORD *face;
	float absX = abs(coordX);
	float absY = abs(coordY);
	float absZ = abs(coordZ);

	bool isXPositive = coordX > 0;
	bool isYPositive = coordY > 0;
	bool isZPositive = coordZ > 0;

	float maxAxis, uc, vc;

	// POSITIVE X
	if (isXPositive && absX >= absY && absX >= absZ)
	{
		// u (0 to 1) goes from +z to -z
		// v (0 to 1) goes from -y to +y
		maxAxis = absX;
		uc = -coordZ;
		vc = coordY;
		face = face0;
	}
	// NEGATIVE X
	else if (!isXPositive && absX >= absY && absX >= absZ)
	{
		// u (0 to 1) goes from -z to +z
		// v (0 to 1) goes from -y to +y
		maxAxis = absX;
		uc = coordZ;
		vc = coordY;
		face = face1;
	}
	// POSITIVE Y
	else if (isYPositive && absY >= absX && absY >= absZ)
	{
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from +z to -z
		maxAxis = absY;
		uc = coordX;
		vc = -coordZ;
		face = face2;
	}
	// NEGATIVE Y
	else if (!isYPositive && absY >= absX && absY >= absZ)
	{
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from -z to +z
		maxAxis = absY;
		uc = coordX;
		vc = coordZ;
		face = face3;
	}
	// POSITIVE Z
	else if (isZPositive && absZ >= absX && absZ >= absY)
	{
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from -y to +y
		maxAxis = absZ;
		uc = coordX;
		vc = coordY;
		face = face4;
	}
	// NEGATIVE Z
	else //if (!isZPositive && absZ >= absX && absZ >= absY)
	{
		// u (0 to 1) goes from +x to -x
		// v (0 to 1) goes from -y to +y
		maxAxis = absZ;
		uc = -coordX;
		vc = coordY;
		face = face5;
	}

	// Convert range from -1 to 1 to 0 to cubemap size
	int u = (int)(0.5f * (uc / maxAxis + 1.0f) * (faceSide - 1));
	int v = (int)(0.5f * (vc / maxAxis + 1.0f) * (faceSide - 1));
	return face[v * faceSide + u];
}

bool writeFrame(const DWORD *sourceVideoFrame, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset)
{
    if (rtDuration == 0)
		MFFrameRateToAverageTimePerFrame(videoFormat.fps, 1, (UINT64*)&rtDuration);
	
	IMFSample *pSample = NULL;
    IMFMediaBuffer *pBuffer = NULL;

	HRESULT hr = S_OK;
	if (bVideo)
	{
		if (bSingleVideoBuffer == true)
		{
			pSample = pVideoFrameSample;
			pBuffer = pVideoFrameBuffer;
		}
		else
			hr = createVideoSampleAndBuffer(&pSample, &pBuffer);
		if (SUCCEEDED(hr))
			hr = copyVideoFrameToBuffer(pBuffer, sourceVideoFrame);
	}

	if (SUCCEEDED(hr))
		hr = writeSamples(pSample, rtStart, rtDuration, LONGLONG(audioOffset * 10000000.0));
	
	if (!bSingleVideoBuffer)
	{
		SafeRelease(&pBuffer);
		SafeRelease(&pSample);
	}

	if (FAILED(hr))
		endVideoEnc();
	return SUCCEEDED(hr);
}

HRESULT copyVideoFrameToBuffer(IMFMediaBuffer *pBuffer, const DWORD *source)
{
	const LONG cbWidth = 4 * videoFormat.width;
	const DWORD cbBuffer = cbWidth * videoFormat.height;
	BYTE *pData = NULL;

    // Lock the buffer and copy the video frame to the buffer.
    HRESULT hr = pBuffer->Lock(&pData, NULL, NULL);
		
    if (SUCCEEDED(hr))
		hr = MFCopyImage(
            pData,                      // Destination buffer.
            cbWidth,                    // Destination stride.
            (BYTE*)source,				// First row in source image.
            cbWidth,                    // Source stride.
            cbWidth,                    // Image width in bytes.
            videoFormat.height          // Image height in pixels.
            );
		
    if (pBuffer)
        hr = pBuffer->Unlock();
		
    // Set the data length of the buffer.
    if (SUCCEEDED(hr))
		hr = pBuffer->SetCurrentLength(cbBuffer);
	return hr;
}

HRESULT writeSamples(IMFSample *pSample, LONGLONG rtStart, LONGLONG rtDuration, LONGLONG audioOffset)
{
	HRESULT hr = S_OK;
	//Video sample----------------------
	// Create a media sample and add the buffer to the sample.
    if (bVideo)
	{
		// Set the time stamp and the duration.
		if (SUCCEEDED(hr))
			hr = pSample->SetSampleTime(rtStart);
		if (SUCCEEDED(hr))
			hr = pSample->SetSampleDuration(rtDuration);
   
		// Send the sample to the Sink Writer.
		if (SUCCEEDED(hr))
			hr = pWriter->WriteSample(videoStreamIndex, pSample);
	}

	//---------------------------------------------
	//Audio sample
	//----------------------------------------------
	if (bAudio)
	{
		IMFSample *pSrSample=0;
		if (audioSrstreamFlags & MF_SOURCE_READERF_ENDOFSTREAM || rtStart < audioOffset - rtDuration)
		{
			if (SUCCEEDED(hr))
				hr = pEmptyAudioSample->SetSampleDuration(rtDuration);
			if (SUCCEEDED(hr))
				hr = pEmptyAudioSample->SetSampleTime(rtStart);
			if (SUCCEEDED(hr))
				hr = pWriter->WriteSample(audioStreamIndex, pEmptyAudioSample);
		}
		else
		{
			while (lastAudioTimeStamp + audioOffset <= rtStart)
			{
				if (SUCCEEDED(hr))
					hr = pSourceReader->ReadSample(0, 0, 0, &audioSrstreamFlags, &lastAudioTimeStamp, &pSrSample);
				if (pSrSample && lastAudioTimeStamp + audioOffset < 0)
					continue;
				if (!pSrSample)
					break;
				if (SUCCEEDED(hr))
					hr = pSrSample->SetSampleTime(lastAudioTimeStamp + audioOffset);
				if (SUCCEEDED(hr))
					hr = pWriter->WriteSample(audioStreamIndex, pSrSample);
				SafeRelease(&pSrSample);
				if (FAILED(hr))
					break;
			};
		}
	}
	rtStart += rtDuration;
//}
	//----------------------------------------------
	return hr;
}

void endVideoEnc()
{
	if (pWriter)
		pWriter->Finalize();
	SafeRelease(&pVideoFrameBuffer);
	SafeRelease(&pVideoFrameSample);
	SafeRelease(&pEmptyAudioSample);
	SafeRelease(&pWriter);
}

BOOL openAudioFileForEncoding(const WCHAR *file)
{
	HRESULT hr = MFCreateSourceReaderFromURL(file, 0, &pSourceReader);
	return SUCCEEDED(hr);
}

BOOL closeAudioFileForEncoding()
{
	SafeRelease(&pSourceReader);
	return TRUE;
}