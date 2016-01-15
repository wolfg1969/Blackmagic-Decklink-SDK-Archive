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
#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <objbase.h>		// Necessary for COM
#include <comutil.h>
#include "DeckLinkAPI_h.h"

// List of known pixel formats and their matching display names
const BMDPixelFormat	gKnownPixelFormats[]		= {bmdFormat8BitYUV, bmdFormat10BitYUV, bmdFormat8BitARGB, bmdFormat8BitBGRA, bmdFormat10BitRGB, (BMDPixelFormat)0};
const char *			gKnownPixelFormatNames[]	= {" 8-bit YUV", "10-bit YUV", "8-bit ARGB", "8-bit BGRA", "10-bit RGB", NULL};

void	print_attributes (IDeckLink* deckLink);
void	print_output_modes (IDeckLink* deckLink);
void	print_capabilities (IDeckLink* deckLink);


int	_tmain (int argc, _TCHAR* argv[])
{
	IDeckLinkIterator*			deckLinkIterator;
	IDeckLinkAPIInformation*	deckLinkAPIInformation;
	IDeckLink*					deckLink;
	int							numDevices = 0;
	HRESULT						result;
	
	// Initialize COM on this thread
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		fprintf(stderr, "Initialization of COM failed - result = %08x.\n", result);
		return 1;
	}
	
	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&deckLinkIterator);
	if (FAILED(result))
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		return 1;
	}
	
	// We can get the version of the API like this:
	result = deckLinkIterator->QueryInterface(IID_IDeckLinkAPIInformation, (void**)&deckLinkAPIInformation);
	if (result == S_OK)
	{
		LONGLONG		deckLinkVersion;
		int				dlVerMajor, dlVerMinor, dlVerPoint;
		
		// We can also use the BMDDeckLinkAPIVersion flag with GetString
		deckLinkAPIInformation->GetInt(BMDDeckLinkAPIVersion, &deckLinkVersion);
		
		dlVerMajor = (deckLinkVersion & 0xFF000000) >> 24;
		dlVerMinor = (deckLinkVersion & 0x00FF0000) >> 16;
		dlVerPoint = (deckLinkVersion & 0x0000FF00) >> 8;
		
		printf("DeckLinkAPI version: %d.%d.%d\n", dlVerMajor, dlVerMinor, dlVerPoint);
		
		deckLinkAPIInformation->Release();
	}
	
	// Enumerate all cards in this system
	while (deckLinkIterator->Next(&deckLink) == S_OK)
	{
		BSTR		deviceNameBSTR = NULL;
		
		// Increment the total number of DeckLink cards found
		numDevices++;
		if (numDevices > 1)
			printf("\n\n");
		
		// *** Print the model name of the DeckLink card
		result = deckLink->GetModelName(&deviceNameBSTR);
		if (result == S_OK)
		{
			_bstr_t		deviceName(deviceNameBSTR, false);
			
			printf("=============== %s ===============\n\n", (char*)deviceName);
		}

		// ** Print all DeckLink Attributes
		print_attributes(deckLink);
		
		// ** List the video output display modes supported by the card
		print_output_modes(deckLink);
		
		// ** List the input and output capabilities of the card
		print_capabilities(deckLink);
		
		// Release the IDeckLink instance when we've finished with it to prevent leaks
		deckLink->Release();
	}
	
	
	// If no DeckLink cards were found in the system, inform the user
	if (numDevices == 0)
		printf("No Blackmagic Design devices were found.\n");
	printf("\n");
	
	// Uninitalize COM on this thread
	CoUninitialize();
	
	// Wait for any key press before exiting
	_getch();
	
	return 0;
}

void	print_attributes (IDeckLink* deckLink)
{
	IDeckLinkAttributes*				deckLinkAttributes = NULL;
	BSTR								name = NULL;
	BOOL								supported;
	HRESULT								result;
	LONGLONG							count;

	// Query the DeckLink for its attributes interface
	result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkAttributes interface - result = %08x\n", result);
		goto bail;
	}

	// List attributes and their value
	printf("Attribute list:\n");
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkHasSerialPort, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Serial port present ?", (supported == TRUE) ? "Yes" : "No");
		
		if(supported == TRUE)
		{
			result = deckLinkAttributes->GetString(BMDDeckLinkSerialPortDeviceName, &name);
			if (result == S_OK)
			{
				_bstr_t		portName(name, false);
				printf(" %-40s %s\n", "Serial port name:", (char *) portName);
			}
			else
			{
				fprintf(stderr, "Could not query the serial port name attribute- result = %08x\n", result);
			}	
		}
	}
	else
	{
		fprintf(stderr, "Could not query the serial port presence attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetInt(BMDDeckLinkNumberOfSubDevices, &count);
	if (result == S_OK)
	{
		printf(" %-40s %d\n", "Number of sub-devices:",  count);
		if (count != 0)
		{
			result = deckLinkAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &count);
			if (result == S_OK)
			{
				printf(" %-40s %d\n", "Sub-device index:",  count);
			}
			else
			{
				fprintf(stderr, "Could not query the sub-device index attribute- result = %08x\n", result);
			}
		}
	}
	else
	{
		fprintf(stderr, "Could not query the number of sub-device attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &count);
	if (result == S_OK)
	{
		printf(" %-40s %d\n", "Maximum number of audio channels: ", count);
	}
	else
	{
		fprintf(stderr, "Could not query the internal keying attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Input mode detection supported ?", (supported == TRUE) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the input mode detection attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInternalKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Internal keying supported ? ", (supported == TRUE) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the internal keying attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsExternalKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "External keying supported ?", (supported == TRUE) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the external keying attribute- result = %08x\n", result);
	}

	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHDKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "HD-mode keying supported ?", (supported == TRUE) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the HD-mode keying attribute- result = %08x\n", result);
	}

bail:
	printf("\n");
}

void	print_output_modes (IDeckLink* deckLink)
{
	IDeckLinkOutput*					deckLinkOutput = NULL;
	IDeckLinkDisplayModeIterator*		displayModeIterator = NULL;
	IDeckLinkDisplayMode*				displayMode = NULL;
	HRESULT								result;	
	
	// Query the DeckLink for its configuration interface
	result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkOutput interface - result = %08x\n", result);
		goto bail;
	}
	
	// Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
	result = deckLinkOutput->GetDisplayModeIterator(&displayModeIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the video output display mode iterator - result = %08x\n", result);
		goto bail;
	}
	
	// List all supported output display modes
	printf("Supported video output display modes:\n");
	while (displayModeIterator->Next(&displayMode) == S_OK)
	{
		BSTR			displayModeBSTR = NULL;
		
		result = displayMode->GetName(&displayModeBSTR);
		if (result == S_OK)
		{
			_bstr_t					modeName(displayModeBSTR, false);
			int						modeWidth;
			int						modeHeight;
			BMDTimeValue			frameRateDuration;
			BMDTimeScale			frameRateScale;
			int						pixelFormatIndex = 0; // index into the gKnownPixelFormats / gKnownFormatNames arrays
			BMDDisplayModeSupport	displayModeSupport;
			
			// Obtain the display mode's properties
			modeWidth = displayMode->GetWidth();
			modeHeight = displayMode->GetHeight();
			displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);
			printf(" %-20s \t %d x %d \t %7g FPS\t", (char*)modeName, modeWidth, modeHeight, (double)frameRateScale / (double)frameRateDuration);

			// Print the supported pixel formats for this display mode
			while ((gKnownPixelFormats[pixelFormatIndex] != 0) && (gKnownPixelFormatNames[pixelFormatIndex] != NULL))
			{
				if ((deckLinkOutput->DoesSupportVideoMode(displayMode->GetDisplayMode(), gKnownPixelFormats[pixelFormatIndex], bmdVideoOutputFlagDefault, &displayModeSupport, NULL) == S_OK)
					&& (displayModeSupport != bmdDisplayModeNotSupported))					
				{
					printf("%s\t", gKnownPixelFormatNames[pixelFormatIndex]);					
				}
				pixelFormatIndex++;
			}
			
			printf("\n");
		}
		
		// Release the IDeckLinkDisplayMode object to prevent a leak
		displayMode->Release();
	}
	
	printf("\n");
	
bail:
	// Ensure that the interfaces we obtained are released to prevent a memory leak
	if (displayModeIterator != NULL)
		displayModeIterator->Release();
	
	if (deckLinkOutput != NULL)
		deckLinkOutput->Release();
}


void	print_capabilities (IDeckLink* deckLink)
{
	IDeckLinkAttributes*		deckLinkAttributes = NULL;
	LONGLONG					ports;
	int							itemCount;
	HRESULT						result;	
	
	// Query the DeckLink for its configuration interface
	result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkAttributes interface - result = %08x\n", result);
		goto bail;
	}
		
	printf("Supported video output connections:\n  ");
	itemCount = 0;
	result = deckLinkAttributes->GetInt(BMDDeckLinkVideoOutputConnections, &ports);
	if (result == S_OK)
	{
		if (ports & bmdVideoConnectionSDI)
		{
			itemCount++;
			printf("SDI");
		}
		
		if (ports & bmdVideoConnectionHDMI)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("HDMI");
		}
		
		if (ports & bmdVideoConnectionOpticalSDI)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Optical SDI");
		}
		
		if (ports & bmdVideoConnectionComponent)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Component");
		}
		
		if (ports & bmdVideoConnectionComposite)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Composite");
		}
		
		if (ports & bmdVideoConnectionSVideo)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("S-Video");
		}
	}
	else
	{
		fprintf(stderr, "Could not obtain the list of output ports - result = %08x\n", result);
		goto bail;
	}
	
	printf("\n\n");
	
	printf("Supported video input connections:\n  ");
	itemCount = 0;
	result = deckLinkAttributes->GetInt(BMDDeckLinkVideoInputConnections, &ports);
	if (result == S_OK)
	{
		if (ports & bmdVideoConnectionSDI)
		{
			itemCount++;
			printf("SDI");
		}
		
		if (ports & bmdVideoConnectionHDMI)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("HDMI");
		}
		
		if (ports & bmdVideoConnectionOpticalSDI)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Optical SDI");
		}
		
		if (ports & bmdVideoConnectionComponent)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Component");
		}
		
		if (ports & bmdVideoConnectionComposite)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("Composite");
		}
		
		if (ports & bmdVideoConnectionSVideo)
		{
			if (itemCount++ > 0)
				printf(", ");
			printf("S-Video");
		}
	}
	else
	{
		fprintf(stderr, "Could not obtain the list of input ports - result = %08x\n", result);
		goto bail;
	}	
	printf("\n");
	
bail:
	if (deckLinkAttributes != NULL)
		deckLinkAttributes->Release();
}
