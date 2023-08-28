#pragma once
#include "out.h"
#include "sink\sink_dsound.h"

class outDsound : public IOut
{
private:
	DSoundSink	m_dsound;
	Chunk		m_chunk;
	int			m_volume;

public:
	outDsound(void);
	~outDsound(void);

	virtual void Config(HWND hwndParent);
	virtual void About(HWND hwndParent);
	virtual int  Open(Speakers spk, int bufferlenms, int prebufferms);
	virtual void Close();
	void Reset();
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
