#pragma once
#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <math.h>
#include "spk.h"
#include <winamp/out.h>

#define PLUGIN_VERSION L"1.0"
#define PLUGIN_NAME TEXT("Not So Neo v") PLUGIN_VERSION
#define PLUGIN_ID 424242

#ifndef NULLSOFT_OUTH

#define OUT_VER 0x10

typedef struct 
{
	int version;				// module version (OUT_VER)
	char *description;			// description of module, with version string
	int id;						// module id. each input module gets its own. non-nullsoft modules should
								// be >= 65536. 

	HWND hMainWindow;			// winamp's main window (filled in by winamp)
	HINSTANCE hDllInstance;		// DLL instance handle (filled in by winamp)

	void (*Config)(HWND hwndParent); // configuration dialog 
	void (*About)(HWND hwndParent);  // about dialog

	void (*Init)();				// called when loaded
	void (*Quit)();				// called when unloaded

	int (*Open)(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms); 
					// returns >=0 on success, <0 on failure
					// NOTENOTENOTE: bufferlenms and prebufferms are ignored in most if not all output plug-ins. 
					//    ... so don't expect the max latency returned to be what you asked for.
					// returns max latency in ms (0 for diskwriters, etc)
					// bufferlenms and prebufferms must be in ms. 0 to use defaults. 
					// prebufferms must be <= bufferlenms

	void (*Close)();	// close the ol' output device.

	int (*Write)(char *buf, int len);	
					// 0 on success. Len == bytes to write (<= 8192 always). buf is straight audio data. 
					// 1 returns not able to write (yet). Non-blocking, always.

	int (*CanWrite)();	// returns number of bytes possible to write at a given time. 
						// Never will decrease unless you call Write (or Close, heh)

	int (*IsPlaying)(); // non0 if output is still going or if data in buffers waiting to be
						// written (i.e. closing while IsPlaying() returns 1 would truncate the song

	int (*Pause)(int pause); // returns previous pause state

	void (*SetVolume)(int volume); // volume is 0-255
	void (*SetPan)(int pan); // pan is -128 to 128

	void (*Flush)(int t);	// flushes buffers and restarts output at time t (in ms) 
							// (used for seeking)

	int (*GetOutputTime)(); // returns played time in MS
	int (*GetWrittenTime)(); // returns time written in MS (used for synching up vis stuff)

} Out_Module;
#endif

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
	int format = FORMAT_PCM16;
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

inline int spk2bps(Speakers spk)
{
	switch (spk.format)
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