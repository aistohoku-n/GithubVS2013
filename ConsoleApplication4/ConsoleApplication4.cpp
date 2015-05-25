// ConsoleApplication4.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <avrt.h>
#include <thread>
#define _USE_MATH_DEFINES
#include <math.h>
#include <fcntl.h>
#include <io.h>



#define EXIT_ON_ERROR(hres)  \
if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
if ((punk) != NULL)  \
				{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

// At this moment (24th May), thse 2 are the API used from WASAPI,
const IID IID_IAudioClient = __uuidof(IAudioClient);
// IAudio client enables a client to create and initialize an audio stream between an audio applicatoin and athe audio engine or the hardware buffer of an audio device;
// To create and use stream in a word.
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
// IAudioRenderClient enables a client to monitor a stream's data to a rendering end point buffer.

//#############################################################################
//#############################################################################

class AudioSource {  //Audio source, executed at first in main

	// this class works as sound playing part 
private:
	WAVEFORMATEX wfx;
	UINT ch = 1; // if this value is 2, sound output is set at R.

	int cFrame = 0; 

	//int nFrames = 240000;  // length in frames
	//int nFrames = 480000;  // length in frames
	int nFrames = 30000;  // length in frames   //realated to the time of playback

	float freq = 500; //tonal frequency
public:
	void SetChannel(UINT channel) {
		ch = channel;
	}
	HRESULT SetFormat(WAVEFORMATEX *pwfx) {
		wfx = *pwfx;
		return 0;
	}
	HRESULT LoadData(UINT32 bufferFrameCount, BYTE *pData, DWORD *pflags) {
		if (cFrame >= nFrames) {
			*pflags = AUDCLNT_BUFFERFLAGS_SILENT;
			return 0;
		}
		cFrame += bufferFrameCount;
		for (UINT j = 0; j < bufferFrameCount; j++) {
 
			//			INT16 sample = (INT16)(0x7FFF/2 * sin(2 * M_PI*freq*(cFrame + j) / wfx.nSamplesPerSec));  // maimum amp(quantization)  WHAT I put in Buffa is the matter!!!!! function // if too long calc,. then lengthen the buffa size.

			INT16 sample = (INT16)(0x7FFF / 4 * sin(2 * M_PI*freq*(cFrame + j) / wfx.nSamplesPerSec) + 0x7FFF / 4 * sin(4 * M_PI*freq*(cFrame + j) / wfx.nSamplesPerSec)
				+ 0x7FFF / 4 * sin(8 * M_PI*freq*(cFrame + j) / wfx.nSamplesPerSec)
				+ 0x7FFF / 4 * sin(16 * M_PI*freq*(cFrame + j) / wfx.nSamplesPerSec)
				);

			pData[wfx.nBlockAlign*j + 2 * ch - 2] = sample & 0x00FF;//Buffa
			pData[wfx.nBlockAlign*j + 2 * ch - 1] = sample >> 8;//big endian (latter 8bit first, formaer 8bit later)
		}
		*pflags = 0;
		return 0;
	}
};

HRESULT GetStreamFormat(IAudioClient *pAudioClient, WAVEFORMATEX **ppwfx) {
	HRESULT hr;
	*ppwfx = new WAVEFORMATEX;
	(*ppwfx)->wFormatTag = WAVE_FORMAT_PCM;
	(*ppwfx)->nChannels = 2;
	(*ppwfx)->nSamplesPerSec = 48000;
	(*ppwfx)->nAvgBytesPerSec = 192000;
	(*ppwfx)->nBlockAlign = 4;
	(*ppwfx)->wBitsPerSample = 16;
	(*ppwfx)->cbSize = 0;
	hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, *ppwfx, NULL);
	return hr;
}

HRESULT PlayExclusiveStream(AudioSource *pMySource) {
	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = 0;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pEndPoints = NULL;
	IMMDevice *pDevice = NULL;
	IPropertyStore *pProps = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	WAVEFORMATEX *pwfx = NULL;
	HANDLE hEvent = NULL;
	HANDLE hTask = NULL;
	UINT32 bufferFrameCount;
	BYTE *pData;
	UINT devCnt;
	DWORD flags = 0;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = pEnumerator->EnumAudioEndpoints(
		eRender, DEVICE_STATE_ACTIVE, &pEndPoints); //names of devices
	EXIT_ON_ERROR(hr)

		hr = pEndPoints->GetCount(&devCnt); // number of devices 
	EXIT_ON_ERROR(hr)

		for (UINT j = 0; j < devCnt; j++) { // questions to indenticate device
			hr = pEndPoints->Item(j, &pDevice);
			EXIT_ON_ERROR(hr)
				hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
			EXIT_ON_ERROR(hr)
				PROPVARIANT friendlyName;
			PropVariantInit(&friendlyName);
			hr = pProps->GetValue(PKEY_Device_FriendlyName, &friendlyName);
			EXIT_ON_ERROR(hr)
				wprintf(L"\nUse device %d of %d: %ls? (y/n)\n", j + 1, devCnt, friendlyName.pwszVal);
			PropVariantClear(&friendlyName);
			SAFE_RELEASE(pProps)
				if (_getwche() == 'y') {
					wprintf(L"\nUse left or right channel? (l/r)\n");
					if (_getwche() == 'r')
						pMySource->SetChannel(2);//Upper class 
					break;
				}
			CoTaskMemFree(pDevice);
			pDevice = NULL;
		}
	if (pDevice == NULL) {
		hr = E_FAIL;
		goto Exit;
	}

	//hr = pEnumerator->GetDefaultAudioEndpoint(
	//eRender, eConsole, &pDevice);
	//EXIT_ON_ERROR(hr)

	hr = pDevice->Activate(
		IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

		// Call a helper function to negotiate with the audio
		// device for an exclusive-mode stream format.
		hr = GetStreamFormat(pAudioClient, &pwfx);
	EXIT_ON_ERROR(hr)

		// Initialize the stream to play at the minimum latency.
		hr = pAudioClient->GetDevicePeriod(NULL, &hnsRequestedDuration); // buffa size 
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		hnsRequestedDuration,
		hnsRequestedDuration,
		pwfx,
		NULL);
	EXIT_ON_ERROR(hr)

		// Tell the audio source which format to use.
		hr = pMySource->SetFormat(pwfx);
	EXIT_ON_ERROR(hr)

		// Create an event handle and register it for
		// buffer-event notifications.
		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEvent == NULL)	{
		hr = E_FAIL;
		goto Exit;
	}

	hr = pAudioClient->SetEventHandle(hEvent);// event or callback
	EXIT_ON_ERROR(hr);

	// Get the actual size of the two allocated buffers.
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&pRenderClient);
	EXIT_ON_ERROR(hr)

		// To reduce latency, load the first buffer with data
		// from the audio source before starting the stream.
		hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
	EXIT_ON_ERROR(hr)

		hr = pMySource->LoadData(bufferFrameCount, pData, &flags);
	EXIT_ON_ERROR(hr)

		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
	EXIT_ON_ERROR(hr)

		// Ask MMCSS to temporarily boost the thread priority
		// to reduce glitches while the low-latency stream plays.
		DWORD taskIndex = 0;
	hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (hTask == NULL) {
		hr = E_FAIL;
		EXIT_ON_ERROR(hr)
	}

	hr = pAudioClient->Start();  // Start playing.
	EXIT_ON_ERROR(hr)

		// Each loop fills one of the two buffers.
		while (flags != AUDCLNT_BUFFERFLAGS_SILENT) {
			// Wait for next buffer event to be signaled.
			DWORD retval = WaitForSingleObject(hEvent, 2000);
			if (retval != WAIT_OBJECT_0) {
				// Event handle timed out after a 2-second wait.
				pAudioClient->Stop();
				hr = ERROR_TIMEOUT;
				goto Exit;
			}

			// Grab the next empty buffer from the audio device.
			hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
			EXIT_ON_ERROR(hr)

				// Load the buffer with data from the audio source.
				hr = pMySource->LoadData(bufferFrameCount, pData, &flags);
			EXIT_ON_ERROR(hr)

				hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
			EXIT_ON_ERROR(hr)
		}

	// Wait for the last buffer to play before stopping.
	Sleep((DWORD)(hnsRequestedDuration / 10000));

	hr = pAudioClient->Stop();  // Stop playing.
	EXIT_ON_ERROR(hr)

	Exit:
	if (hEvent != NULL) {
		CloseHandle(hEvent);
	}
	if (hTask != NULL) {
		AvRevertMmThreadCharacteristics(hTask);
	}
	//CoTaskMemFree(pwfx);
	
	// orders in this part can be wrong 
	// -> fixed on 22nd May
	delete pwfx;
	
	SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pDevice)
		SAFE_RELEASE(pAudioClient)
		SAFE_RELEASE(pRenderClient)
		return hr;
}

int main() {
	_setmode(_fileno(stdout), _O_U16TEXT); //Sets the file translation mode.
	// program runs without this line.
		
	AudioSource *pMySource = new AudioSource;
	//

	std::thread tPlayer(PlayExclusiveStream, pMySource);

	// std::thread ... standard name space, do a thread procesing 
	tPlayer.join();
	//
	return 0;
}