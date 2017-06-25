#include "encoding.h"
#include <Codecapi.h>

GUID VIDEO_ENCODING_FORMAT = MFVideoFormat_H264;
//GUID VIDEO_ENCODING_FORMAT = MFVideoFormat_WMV1;
//GUID VIDEO_ENCODING_FORMAT = MFVideoFormat_MSS2;
//GUID VIDEO_ENCODING_FORMAT = MFVideoFormat_MP4S;
GUID VIDEO_INPUT_FORMAT = MFVideoFormat_RGB32;
//GUID AUDIO_OUTPUT_FORMAT = MFAudioFormat_WMAudioV9;
//GUID AUDIO_OUTPUT_FORMAT = MFAudioFormat_WMAudio_Lossless;
GUID AUDIO_OUTPUT_FORMAT = MFAudioFormat_AAC;
//GUID AUDIO_OUTPUT_FORMAT = MFAudioFormat_PCM;

IMFSourceReader *pSourceReader = 0;
IMFSinkWriter *pWriter = 0;
IMFMediaBuffer *pVideoFrameBuffer = 0;
IMFSample *pVideoFrameSample = 0;
int currentVideoSample;
DWORD videoStreamIndex;
DWORD audioStreamIndex;
LONGLONG lastAudioTimeStamp;
IMFSample *pEmptyAudioSample = 0;
int currentAudioSample;
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
		//for (int i=0;i<length;i++)
			//pData[i] = rand();
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
	currentVideoSample = 0;
	currentAudioSample = 0;
	audioSrstreamFlags = 0;
	
	bVideo = _bVideo;

	static WCHAR wOutputFile[1000];
	mbToWcString(wOutputFile, outputFile);
	//static WCHAR wAudioFile[1000];
	//bAudio = audioFile && audioFile[0] != 0;
	//if (bAudio)
		//mbToWcString(wAudioFile, audioFile);
	//bAudio = audioMemFileSize > 0;
	bAudio = true;

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
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, VIDEO_ENCODING_FORMAT);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
		
		
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_AVG_BITRATE, videoFormat.bitRate);   
		if (SUCCEEDED(hr))
			hr = pMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
		if (SUCCEEDED(hr))
			hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, videoFormat.width, videoFormat.height);
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, videoFormat.fps, 1);
		//if (SUCCEEDED(hr))
			//hr = MFSetAttributeRatio(pMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
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
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, VIDEO_INPUT_FORMAT);
		//if (SUCCEEDED(hr))
			//hr = pMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
		if (SUCCEEDED(hr))
			hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, videoFormat.width, videoFormat.height);
		if (SUCCEEDED(hr))
			hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, videoFormat.fps, 1);   
		//if (SUCCEEDED(hr))
			//hr = MFSetAttributeRatio(pMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
    
		//if (SUCCEEDED(hr))
	//		hr = pMediaType->SetGUID(MF_MT_SUBTYPE, VIDEO_INPUT_FORMAT);     
		if (SUCCEEDED(hr))
		{
			hr = pWriter->SetInputMediaType(videoStreamIndex, pMediaType, NULL);   
			SafeRelease(&pMediaType);
		}
	}
    
    if (bAudio)
	{
		//Set the sink audio output type
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, AUDIO_OUTPUT_FORMAT);
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, videoFormat.audioSampleRate);
		int bytesPerSec;
		int blockAlign;
		if (videoFormat.audioSampleRate == 48000)
		{
			bytesPerSec = 24000;
			blockAlign = 4096;
		}
		else if (videoFormat.audioSampleRate == 44100)
		{
			bytesPerSec = 32004;
			blockAlign = 5945;
			//bytesPerSec  = 144004;
			//blockAlign = 13375;
		}
		else
			assert(0);
		//blockAlign = 4;
		//bytesPerSec = 192000;
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSec );
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, 2 );
		if (SUCCEEDED(hr))
			hr=pMediaType->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	      
		//if (SUCCEEDED(hr))
			//pMediaType->SetUINT32( MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1 ) ;
		//if (SUCCEEDED(hr))
			//pMediaType->SetUINT32( MF_MT_ALL_SAMPLES_INDEPENDENT, 1 ) ;
		//if (SUCCEEDED(hr))
			//pMediaType->SetUINT32( MF_MT_FIXED_SIZE_SAMPLES, 1 ) ;
		
		if (SUCCEEDED(hr))
			pMediaType->SetUINT32( MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign ) ;
	
	
		if (SUCCEEDED(hr))
		{
			hr = pWriter->AddStream(pMediaType, &audioStreamIndex);   
			SafeRelease(&pMediaType);
		}

		//Set the intermediate audio media type (sink input and source reader output)
	
		//Create source reader
		if (SUCCEEDED(hr))
		{	
			//hr = openAudioFileForEncoding(wAudioFile);
			/*IMFByteStream *audioByteStream;
			hr = MFCreateTempFile(MF_FILE_ACCESSMODE::MF_ACCESSMODE_READWRITE, MF_FILE_OPENMODE::MF_OPENMODE_RESET_IF_EXIST, MF_FILE_FLAGS::MF_FILEFLAGS_NONE, &audioByteStream);
			if (SUCCEEDED(hr))
			{
				ULONG bytesWritten = 0;
				hr = audioByteStream->Write(audioMemFile, audioMemFileSize, &bytesWritten);
				if (SUCCEEDED(hr) && bytesWritten == audioMemFileLength)
					hr = MFCreateSourceReaderFromByteStream(audioByteStream, 0, &pSourceReader);
			}*/
		}
		//IMFMediaType *native = 0;
		//if (SUCCEEDED(hr))
		//	hr = pSourceReader->GetNativeMediaType(0, 0, &native);
		//UINT32 samplingRate;
		//native->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplingRate);
	
		if (SUCCEEDED(hr))
			hr = MFCreateMediaType(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (SUCCEEDED(hr))
			hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		//if (SUCCEEDED(hr))
			//hr=pMediaType->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
		//if (SUCCEEDED(hr))
		//	hr=pMediaType->SetUINT32( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2*samplingRate*2 );
		//if (SUCCEEDED(hr))
		//	hr=pMediaType->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, 2 );
		//if (SUCCEEDED(hr))
		//	hr=pMediaType->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
		////if (SUCCEEDED(hr))
		//	//pMediaType->SetUINT32( MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1 );
		//if (SUCCEEDED(hr))
		//	pMediaType->SetUINT32( MF_MT_AUDIO_BLOCK_ALIGNMENT, 4 );
	
		if (SUCCEEDED(hr))
			hr = pSourceReader->SetCurrentMediaType(0, 0, pMediaType);
		SafeRelease(&pMediaType);
		if (SUCCEEDED(hr))
			hr = pSourceReader->GetCurrentMediaType(0, &pMediaType);

		/*UINT32 sps;
		pMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sps);
		UINT32 bps;
		pMediaType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bps);
		UINT32 nc;
		pMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nc);
		UINT32 bits;
		pMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
		UINT32 block;
		pMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &block);*/
		
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

bool writeFrame(const DWORD *sourceVideoFrame, LONGLONG rtStart, LONGLONG &rtDuration, double audioOffset, bool bFlush)
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
		hr = writeSamples(pSample, rtStart, rtDuration, LONGLONG(audioOffset * 10000000.0), bFlush);
	
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

HRESULT writeSamples(IMFSample *pSample, LONGLONG rtStart, LONGLONG rtDuration, LONGLONG audioOffset, bool bFlush)
{
	HRESULT hr;
//for (int f = 0;f<2000;f++)
//{
	hr = S_OK;
	//IMFSample *pSample = 0;
	//Video sample----------------------
	// Create a media sample and add the buffer to the sample.
    if (bVideo)
	{
		//SafeRelease(&pVideoFrameSample);
		//MFCreateSample(&pVideoFrameSample);
		//if (SUCCEEDED(hr) && currentVideoSample == 0)	
			//hr = pVideoFrameSample->SetUINT32( MFSampleExtension_Discontinuity, TRUE );
		//if (SUCCEEDED(hr))
			//hr = pVideoFrameSample->AddBuffer(pVideoFrameBuffer);
		//if (SUCCEEDED(hr) && currentVideoSample == 1)
			//hr = pVideoFrameSample->SetUINT32( MFSampleExtension_Discontinuity, FALSE );
		
		// Set the time stamp and the duration.
		if (SUCCEEDED(hr))
			hr = pSample->SetSampleTime(rtStart);
		if (SUCCEEDED(hr))
			hr = pSample->SetSampleDuration(rtDuration);
   
		// Send the sample to the Sink Writer.
		if (SUCCEEDED(hr))
			hr = pWriter->WriteSample(videoStreamIndex, pSample);
	
		currentVideoSample++;
		//if (SUCCEEDED(hr) && bFlush)
			//pWriter->Flush(videoStreamIndex);
	}
	

	//---------------------------------------------
	//Audio sample
	//----------------------------------------------
	if (bAudio)
	{
		int loops = 0;
		IMFSample *pSrSample=0;
	
		if (audioSrstreamFlags & MF_SOURCE_READERF_ENDOFSTREAM || rtStart < audioOffset - rtDuration)
		{
			if (SUCCEEDED(hr))
			{
				if (currentAudioSample == 0)
					hr = pEmptyAudioSample->SetUINT32( MFSampleExtension_Discontinuity, TRUE );
				else if (currentAudioSample == 1)
					hr = pEmptyAudioSample->SetUINT32( MFSampleExtension_Discontinuity, FALSE );
			}
			
			if (SUCCEEDED(hr))
				hr = pEmptyAudioSample->SetSampleDuration(rtDuration);
			if (SUCCEEDED(hr))
				hr = pEmptyAudioSample->SetSampleTime(rtStart);
			if (SUCCEEDED(hr))
				hr = pWriter->WriteSample(audioStreamIndex, pEmptyAudioSample);
			currentAudioSample++;
			//if (currentAudioSample % 1 == 0)
				//pWriter->Flush(audioStreamIndex);

			//SafeRelease(&pSrSample);
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
				{
					if (SUCCEEDED(hr))
						hr = pWriter->Flush(audioStreamIndex);
					currentAudioSample = 0;
					break;
				}
				if (currentAudioSample == 0 && SUCCEEDED(hr))
					hr = pSrSample->SetUINT32( MFSampleExtension_Discontinuity, TRUE );

				//LONGLONG duration;
				//if (SUCCEEDED(hr))
					//pSrSample->GetSampleDuration(&duration);
				if (SUCCEEDED(hr))
					hr = pSrSample->SetSampleTime(lastAudioTimeStamp + audioOffset);
				//if (SUCCEEDED(hr))
					//hr = pSrSample->SetSampleDuration(rtDuration);
				if (SUCCEEDED(hr))
					hr = pWriter->WriteSample(audioStreamIndex, pSrSample);
				SafeRelease(&pSrSample);
				
				currentAudioSample++;
				//if (currentAudioSample % 100 == 0)
					//pWriter->Flush(audioStreamIndex);
				if (FAILED(hr))
					break;
				loops++;
				//if (loops==35)
					//break;
			};
		}
		//std::cout<<loops<<std::endl;
	}
	rtStart += rtDuration;
//}
	//----------------------------------------------
	return hr;
}

//bool addAudioSampleToVideo(LONGLONG rtStart, LONGLONG &rtDuration, bool bFlush)
//{
//	return true;
//}

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