/* -LICENSE-START-
** Copyright (c) 2009 Blackmagic Design
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
//
//  SyncController.mm
//  Signal Generator
//

#import "SyncController.h"


const uint32_t		kAudioWaterlevel = 48000;

// SD 75% Colour Bars
static uint32_t gSD75pcColourBars[8] =
{
	0xeb80eb80, 0xa28ea22c, 0x832c839c, 0x703a7048,
	0x54c654b8, 0x41d44164, 0x237223d4, 0x10801080
};

// HD 75% Colour Bars
static uint32_t gHD75pcColourBars[8] =
{
	0xeb80eb80, 0xa888a82c, 0x912c9193, 0x8534853f,
	0x3fcc3fc1, 0x33d4336d, 0x1c781cd4, 0x10801080
};

@implementation SyncController

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
	IDeckLinkIterator*			deckLinkIterator = NULL;
	BOOL						success = NO;
	
	// **** Find a DeckLink instance and obtain video output interface
	deckLinkIterator = CreateDeckLinkIteratorInstance();
	if (deckLinkIterator == NULL)
	{
		NSRunAlertPanel(@"This application requires the DeckLink drivers installed.", @"Please install the Blackmagic DeckLink drivers to use the features of this application.", @"OK", nil, nil);
		goto bail;
	}
	
	// Connect to the first DeckLink instance
	if (deckLinkIterator->Next(&deckLink) != S_OK)
	{
		NSRunAlertPanel(@"This application requires a DeckLink PCI card.", @"You will not be able to use the features of this application until a DeckLink PCI card is installed.", @"OK", nil, nil);
		goto bail;
	}
	
	// Obtain the audio/video output interface (IDeckLinkOutput)
	if (deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput) != S_OK)
		goto bail;
	
	// Create a delegate class to allow the DeckLink API to call into our code
	playerDelegate = new PlaybackDelegate(self, deckLinkOutput);
	if (playerDelegate == NULL)
		goto bail;
	// Provide the delegate to the audio and video output interfaces
	deckLinkOutput->SetScheduledFrameCompletionCallback(playerDelegate);
	deckLinkOutput->SetAudioCallback(playerDelegate);
	
	
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	IDeckLinkDisplayModeIterator*		displayModeIterator;
	IDeckLinkDisplayMode*				deckLinkDisplayMode;
	
	[videoFormatPopup removeAllItems];
	if (deckLinkOutput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		goto bail;
	while (displayModeIterator->Next(&deckLinkDisplayMode) == S_OK)
	{
		CFStringRef		modeName;
		
		if (deckLinkDisplayMode->GetName(&modeName) == S_OK)
		{
			// Add this item to the video format poup menu
			[videoFormatPopup addItemWithTitle:(NSString*)modeName];
			// Save the IDeckLinkDisplayMode in the menu item's tag
			[[videoFormatPopup itemAtIndex:[videoFormatPopup numberOfItems]-1]  setTag:(NSInteger)deckLinkDisplayMode];
			CFRelease(modeName);
		}
	}
	displayModeIterator->Release();
	
	
	deckLinkOutput->SetScreenPreviewCallback(CreateCocoaScreenPreview(previewView));
	
	success = YES;
	
bail:
	if (success == NO)
	{
		// Release any resources that were partially allocated
		if (deckLinkOutput != NULL)
		{
			deckLinkOutput->Release();
			deckLinkOutput = NULL;
		}
		//
		if (deckLink != NULL)
		{
			deckLink->Release();
			deckLink = NULL;
		}
		
		// Disable the user interface if we could not succsssfully connect to a DeckLink device
		[startButton setEnabled:NO];
		[self enableInterface:NO];
	}
	
	if (deckLinkIterator != NULL)
		deckLinkIterator->Release();
}

- (void)enableInterface:(BOOL)enable
{
	// Set the enable state of user interface elements
	[outputSignalPopup setEnabled:enable];
	[audioChannelPopup setEnabled:enable];
	[audioSampleDepthPopup setEnabled:enable];
	[videoFormatPopup setEnabled:enable];
}


- (IBAction)toggleStart:(id)sender
{
	if (running == NO)
		[self startRunning];
	else
		[self stopRunning];
}

- (void)startRunning
{
	IDeckLinkDisplayMode*	videoDisplayMode = NULL;
	
	// Determine the audio and video properties for the output stream
	outputSignal = (OutputSignal)[outputSignalPopup indexOfSelectedItem];
	audioChannelCount = [[audioChannelPopup selectedItem] tag];
	audioSampleDepth = [[audioSampleDepthPopup selectedItem] tag];
	audioSampleRate = bmdAudioSampleRate48kHz;
	//
	// - Extract the IDeckLinkDisplayMode from the display mode popup menu (stashed in the item's tag)
	videoDisplayMode = (IDeckLinkDisplayMode*)[[videoFormatPopup selectedItem] tag];
	frameWidth = videoDisplayMode->GetWidth();
	frameHeight = videoDisplayMode->GetHeight();
	videoDisplayMode->GetFrameRate(&frameDuration, &frameTimescale);
	// Calculate the number of frames per second, rounded up to the nearest integer.  For example, for NTSC (29.97 FPS), framesPerSecond == 30.
	framesPerSecond = (frameTimescale + (frameDuration-1))  /  frameDuration;
	
	// Set the video output mode
	if (deckLinkOutput->EnableVideoOutput(videoDisplayMode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK)
		goto bail;
	
	// Set the audio output mode
	if (deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, audioSampleDepth, audioChannelCount, bmdAudioOutputStreamTimestamped) != S_OK)
		goto bail;
	
	
	// Generate one second of audio tone
	audioSamplesPerFrame = ((audioSampleRate * frameDuration) / frameTimescale);
	audioBufferSampleLength = (framesPerSecond * audioSampleRate * frameDuration) / frameTimescale;
	audioBuffer = malloc(audioBufferSampleLength * audioChannelCount * (audioSampleDepth / 8));
	if (audioBuffer == NULL)
		goto bail;
	FillSine(audioBuffer, audioBufferSampleLength, audioChannelCount, audioSampleDepth);
	
	// Generate a frame of black
	if (deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &videoFrameBlack) != S_OK)
		goto bail;
	FillBlack(videoFrameBlack);
	
	// Generate a frame of colour bars
	if (deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &videoFrameBars) != S_OK)
		goto bail;
	FillColourBars(videoFrameBars);
	
	
	
	// Begin video preroll by scheduling a second of frames in hardware
	totalFramesScheduled = 0;
	for (int i = 0; i < framesPerSecond; i++)
		[self scheduleNextFrame:YES];
	
	// Begin audio preroll.  This will begin calling our audio callback, which will start the DeckLink output stream.
	totalAudioSecondsScheduled = 0;
	if (deckLinkOutput->BeginAudioPreroll() != S_OK)
		goto bail;
	
	// Success; update the UI
	running = YES;
	[startButton setTitle:@"Stop"];
	// Disable the user interface while running (prevent the user from making changes to the output signal)
	[self enableInterface:NO];
	
	return;
	
bail:
	// *** Error-handling code.  Cleanup any resources that were allocated. *** //
	[self stopRunning];
}


- (void)stopRunning
{
	// Stop the audio and video output streams immediately
	deckLinkOutput->StopScheduledPlayback(0, NULL, 0);
	//
	deckLinkOutput->DisableAudioOutput();
	deckLinkOutput->DisableVideoOutput();
	
	if (videoFrameBlack != NULL)
		videoFrameBlack->Release();
	videoFrameBlack = NULL;
	
	if (videoFrameBars != NULL)
		videoFrameBars->Release();
	videoFrameBars = NULL;
	
	if (audioBuffer != NULL)
		free(audioBuffer);
	audioBuffer = NULL;
	
	// Success; update the UI
	running = NO;
	[startButton setTitle:@"Start"];
	// Re-enable the user interface when stopped
	[self enableInterface:YES];
}


- (void)scheduleNextFrame:(BOOL)prerolling
{
	if (prerolling == NO)
	{
		// If not prerolling, make sure that playback is still active
		if (running == NO)
			return;
	}
	
	if (outputSignal == kOutputSignalPip)
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
		{
			// On each second, schedule a frame of bars
			if (deckLinkOutput->ScheduleVideoFrame(videoFrameBars, (totalFramesScheduled * frameDuration), frameDuration, frameTimescale) != S_OK)
				return;
		}
		else
		{
			// Schedue frames of black
			if (deckLinkOutput->ScheduleVideoFrame(videoFrameBlack, (totalFramesScheduled * frameDuration), frameDuration, frameTimescale) != S_OK)
				return;
		}
	}
	else
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
		{
			// On each second, schedule a frame of black
			if (deckLinkOutput->ScheduleVideoFrame(videoFrameBlack, (totalFramesScheduled * frameDuration), frameDuration, frameTimescale) != S_OK)
				return;
		}
		else
		{
			// Schedue frames of color bars
			if (deckLinkOutput->ScheduleVideoFrame(videoFrameBars, (totalFramesScheduled * frameDuration), frameDuration, frameTimescale) != S_OK)
				return;
		}
	}
	
	totalFramesScheduled += 1;
}

- (void)writeNextAudioSamples
{
	// Write one second of audio to the DeckLink API.
	
	if (outputSignal == kOutputSignalPip)
	{
		// Schedule one-frame of audio tone
		if (deckLinkOutput->ScheduleAudioSamples(audioBuffer, audioSamplesPerFrame, (totalAudioSecondsScheduled * audioBufferSampleLength), audioSampleRate, NULL) != S_OK)
			return;
	}
	else
	{
		// Schedule one-second (minus one frame) of audio tone
		if (deckLinkOutput->ScheduleAudioSamples(audioBuffer, (audioBufferSampleLength - audioSamplesPerFrame), (totalAudioSecondsScheduled * audioBufferSampleLength) + audioSamplesPerFrame, audioSampleRate, NULL) != S_OK)
			return;
	}
	
	totalAudioSecondsScheduled += 1;
}

@end


/*****************************************/

PlaybackDelegate::PlaybackDelegate (SyncController* owner, IDeckLinkOutput* deckLinkOutput)
{
	mController = owner;
	mDeckLinkOutput = deckLinkOutput;
}

HRESULT		PlaybackDelegate::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	// When a video frame has been 
	[mController scheduleNextFrame:NO];
	return S_OK;
}

HRESULT		PlaybackDelegate::ScheduledPlaybackHasStopped ()
{
	return S_OK;
}

HRESULT		PlaybackDelegate::RenderAudioSamples (bool preroll)
{
	// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
	[mController writeNextAudioSamples];
	
	if (preroll)
	{
		// Start audio and video output
		mDeckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
	}
	
	return S_OK;
}


/*****************************************/


void	FillSine (void* audioBuffer, uint32_t samplesToWrite, uint32_t channels, uint32_t sampleDepth)
{
	if (sampleDepth == 16)
	{
		int16_t*		nextBuffer;
		
		nextBuffer = (int16_t*)audioBuffer;
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int16_t		sample;
			
			sample = (int16_t)(24576.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (int32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
	else if (sampleDepth == 32)
	{
		int32_t*		nextBuffer;
		
		nextBuffer = (int32_t*)audioBuffer;
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int32_t		sample;
			
			sample = (int32_t)(1610612736.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (int32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
}

void	FillColourBars (IDeckLinkVideoFrame* theFrame)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t*		bars;
	
	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	if (width > 720)
	{
		bars = gHD75pcColourBars;
	}
	else
	{
		bars = gSD75pcColourBars;
	}

	for (uint32_t y = 0; y < height; y++)
	{
	    for (uint32_t x = 0; x < width; x+=2)
		{
				*(nextWord++) = bars[(x * 8) / width];
		}
	}
}

void	FillBlack (IDeckLinkVideoFrame* theFrame)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t		wordsRemaining;
	
	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	wordsRemaining = (width*2 * height) / 4;
	
	while (wordsRemaining-- > 0)
		*(nextWord++) = 0x10801080;
}
