#pragma once
#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <math.h>
#include "spk.h"
#include <winamp/out.h>

#define PLUGIN_VERSION L"1.1.7"
#define PLUGIN_NAME TEXT("Not So Neo v") PLUGIN_VERSION
#define PLUGIN_ID 424242

typedef Out_Module* (*WINAMPGETOUTMODULE)();

extern Out_Module g_OutModMaster;

class IOut
{
public:
	virtual void Config(HWND hwndParent) = 0;
	//virtual int  Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms) = 0;
	virtual int  Open(Speakers spk, const int bufferlenms, const int prebufferms) = 0;
	virtual void Close(void) = 0;
	virtual int  Write(const char *buf, const int len) = 0;
	virtual int  CanWrite(void) = 0;
	virtual int  IsPlaying(void) = 0;
	virtual int  Pause(const int pause) = 0;
	virtual void SetVolume(const int volume) = 0;
	virtual void SetPan(const int pan) = 0;
	virtual void Flush(const int t) = 0;
	virtual int  GetOutputTime(void) = 0;
	virtual int  GetWrittenTime(void) = 0;
};

inline Speakers winamp2spk(const int sr, const int nch, const int bps)
{
	short format = FORMAT_PCM16;
	int mask = MODE_2_0;

	switch (bps)
	{
		case 16: format = FORMAT_PCM16; break;
		case 24: format = FORMAT_PCM24; break;
		case 32: format = FORMAT_PCM32; break;
	}

	switch (nch)
	{
		case  1: mask = MODE_1_0; break;
		case  2: mask = MODE_2_0; break;
		case  3: mask = MODE_3_0; break;
		case  4: mask = MODE_2_2; break;
		case  5: mask = MODE_3_1 | CH_MASK_LFE; break;
		case  6: mask = MODE_3_2 | CH_MASK_LFE; break;
	}

	return Speakers(format, mask, sr);
}

inline int spk2bps(const short int format)
{
	switch (format)
	{
		case FORMAT_PCM24: return 24;
		case FORMAT_PCM32: return 32;
		default: return 16;
	}
}

inline int spk2nch(Speakers spk)
{
	switch (spk.mask)
	{
		case MODE_1_0: return 1;
		case MODE_2_0: return 2;
		case MODE_3_0: return 3;
		case MODE_2_2: return 4;
		case MODE_3_1 | CH_MASK_LFE: return 5;
		case MODE_3_2 | CH_MASK_LFE: return 6;
		default:
		{
			switch (spk.original_mask)
			{
				case MODE_1_0: return 1;
				case MODE_2_0: return 2;
				case MODE_3_0: return 3;
				case MODE_2_2: return 4;
				case MODE_3_1 | CH_MASK_LFE: return 5;
				case MODE_3_2 | CH_MASK_LFE: return 6;
				default: return 2;
			}
		}
	}
}