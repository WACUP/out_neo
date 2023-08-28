#pragma once
#include "out.h"

class outPlugin : public IOut
{
public:
	outPlugin(void);
	~outPlugin(void);

	virtual void Config(HWND hwndParent);
	virtual void About(HWND hwndParent);
	virtual int  Open(Speakers spk, int bufferlenms, int prebufferms);
	virtual void Close();
	virtual int  Write(char *buf, int len);
	virtual int  CanWrite();
	virtual int  IsPlaying();
	virtual int  Pause(int pause);
	virtual void SetVolume(int volume);
	virtual void SetPan(int pan);
	virtual void Flush(int t);
	virtual int  GetOutputTime();
	virtual int  GetWrittenTime();
};
