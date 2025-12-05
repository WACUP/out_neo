#pragma once
#include "out.h"

#include "outPlugin.h"
#include "outDsound.h"

#include "filters\dvd_graph.h"

#include "config\DevilConfig.h"

#define OUTPUT_MODE_NORMAL 1
#define OUTPUT_MODE_SPDIF  2

class outMixer : public IOut
{
private:
	outPlugin *m_pOutPlugin;

public:
#ifndef _WIN64
	DevilConfig* m_pConfig;
#endif

private:
#ifdef USE_SPDIF
	outDsound *m_pOutDsound;
#endif
	IOut *m_pOut;

	DVDGraph m_dvd_graph;
	Chunk m_chunk;
	Speakers m_in_spk;
	Speakers m_out_spk;

	//unused
	virtual int Open(Speakers spk, const int bufferlenms, const int prebufferms) { return 0; }

	//config
	TCHAR m_out_plugin[32];
	int m_user_sample_rate;
	short int m_user_format;
#ifdef USE_SPDIF
	bool m_use_spdif : 1;
	bool m_spdif_close_at_end : 1;
#endif
	bool m_invert_levels : 1;
	bool m_outputchanged : 1;
	bool m_output_as_is : 1;
	bool m_format_as_is : 1;
	char m_output_mode;
#ifdef LEGACY_CODE
	int m_refresh_time;
#endif

	void ReadConfig(void);
	void WriteConfig(void);

public:
	outMixer(void);
	~outMixer(void);

#ifdef _WIN64
	DevilConfig *m_pConfig;
#endif

	virtual void Config(HWND hwndParent);
	virtual int Open(const int samplerate, const int numchannels, const int bitspersamp,
										   const int bufferlenms, const int prebufferms);
	virtual void Close(void);
	virtual int Write(const char *buf, const int len);
	virtual int CanWrite(void);
	virtual int IsPlaying(void);
	virtual int Pause(const int pause);
	virtual void SetVolume(const int volume);
	virtual void SetPan(const int pan);
	virtual void Flush(const int t);
	virtual int GetOutputTime(void);
	virtual int GetWrittenTime(void);

	//Config dialog access
	inline DVDGraph *get_DvdGraph() { return &m_dvd_graph; }
	inline void get_Input(Speakers *spk) { if (spk) *spk = m_in_spk; }
	inline void get_Output(Speakers *spk) { if (spk) *spk = m_out_spk; }
	inline int get_SampleRate() { return m_user_sample_rate; }
	inline int get_Format() { return m_user_format; }
#ifdef USE_SPDIF
	inline bool get_UseSpdif() { return m_use_spdif; }
	inline bool get_SpdifCloseAtEnd() { return m_spdif_close_at_end; }
	inline void set_SpdifCloseAtEnd(bool value) { m_spdif_close_at_end = value; }
#endif
	inline bool get_InvertLevels() { return m_invert_levels; }
	inline void set_InvertLevels(bool value) { m_invert_levels = value; }
#ifdef LEGACY_CODE
	inline int get_RefreshTime() { return m_refresh_time; }
	inline void set_RefreshTime(int value) { m_refresh_time = value; }
#endif

#ifdef USE_SPDIF
	void ChangeOutput(const int ispk, const int ifmt, const int rate, const bool use_spdif, const bool update_as_is);
#else
	void ChangeOutput(const int ispk, const int ifmt, const int rate, const bool update_as_is);
#endif
	void set_OutputPlugin(const TCHAR *path);
	const TCHAR* get_OutputPlugin(void);
};