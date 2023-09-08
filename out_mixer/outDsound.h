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
	virtual int Open(Speakers spk, const int bufferlenms, const int prebufferms);
	virtual void Close(void);
	void Reset(void);
	virtual int Write(const char *buf, const int len);
	virtual int CanWrite(void);
	virtual int IsPlaying(void);
	virtual int Pause(const int pause);
	virtual void SetVolume(const int volume);
	virtual void SetPan(const int pan);
	virtual void Flush(const int t);
	virtual int GetOutputTime(void);
	virtual int GetWrittenTime(void);
};