/* -LICENSE-START-
 ** Copyright (c) 2015 Blackmagic Design
 **
 ** Permission is hereby granted, free of charge, to any person or organization
 ** obtaining a copy of the software and accompanying documentation covered by
 ** this license (the "Software") to use, reproduce, display, distribute,
 ** execute, and transmit the Software, and to prepare derivative works of the
 ** Software, and to permit third-parties to whom the Software is furnished to
 ** do so, all subject to the following:
 **
 ** The copyright notices in the Software and this entire statement, including
 ** the above license grant, this restriction and the following disclaimer,
 ** must be included in all copies of the Software, in whole or in part, and
 ** all derivative works of the Software, unless such copies or derivative
 ** works are solely in the form of machine-executable object code generated by
 ** a source language processor.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 ** DEALINGS IN THE SOFTWARE.
 ** -LICENSE-END-
 */
#ifndef __VIDEO_FRAME_TRANSFER_H__
#define __VIDEO_FRAME_TRANSFER_H__

#include <stdexcept>
#include <map>

// NVIDIA GPU Direct For Video with DirectX11 requires the following two headers.
// See the NVIDIA website to check if your graphics card is supported.
#include <DVPAPI.h>
#include <dvpapi_d3d11.h>

struct SyncInfo;


// Class for performing efficient frame memory transfers between the CPU and GPU,
// using NVIDIA GPU Direct.
class VideoFrameTransfer
{
public:
	enum Direction
	{
		CPUtoGPU,
		GPUtoCPU
	};

	VideoFrameTransfer(ID3D11Device* pD3DDevice, unsigned long memSize, void* address, Direction direction);
	~VideoFrameTransfer();

	static bool checkFastMemoryTransferAvailable();
	static bool initialize(ID3D11Device* pD3DDevice, unsigned width, unsigned height, void *captureTexture, void *playbackTexture);
	static void waitAPI(Direction direction);
	static void endAPI(Direction direction);

	bool performFrameTransfer();
	void waitSyncComplete();
	void endSyncComplete();

private:
	static bool isNvidiaDvpAvailable();
	static bool initializeMemoryLocking(unsigned memSize);

	void*						mBuffer;
	unsigned long				mMemSize;
	Direction					mDirection;
	static bool					mInitialized;
	static bool					mUseDvp;
	static unsigned				mWidth;
	static unsigned				mHeight;
	static void*				mCaptureTexture;

	// NVIDIA GPU Direct for Video support
	SyncInfo*					mExtSync;
	SyncInfo*					mGpuSync;
	DVPBufferHandle				mDvpSysMemHandle;
	ID3D11Device*				mpD3DDevice;

	static DVPBufferHandle		mDvpCaptureTextureHandle;
	static DVPBufferHandle		mDvpPlaybackTextureHandle;
	static uint32_t				mBufferAddrAlignment;
	static uint32_t				mBufferGpuStrideAlignment;
	static uint32_t				mSemaphoreAddrAlignment;
	static uint32_t				mSemaphoreAllocSize;
	static uint32_t				mSemaphorePayloadOffset;
	static uint32_t				mSemaphorePayloadSize;
};

#endif