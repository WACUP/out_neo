#pragma once
#include "out.h"

#include "outPlugin.h"
#include "outDsound.h"

#include "filters\dvd_graph.h"
#include "win32\cpu.h"

#include "config\DevilConfig.h"

#define OUTPUT_MODE_NORMAL 1
#define OUTPUT_MODE_SPDIF  2

class outMixer : public IOut
{
private:
	outPlugin *m_pOutPlugin;
	outDsound *m_pOutDsound;
	IOut *m_pOut;

	DVDGraph	m_dvd_graph;
	Chunk		m_chunk;
	Speakers	m_in_spk;
	Speakers	m_out_spk;
	CPUMeter	m_cpu;

	//unused
	virtual int Open(Speakers spk, int bufferlenms, int prebufferms) { return 0; }

	//config
	DevilConfig		*m_pConfig;
	char			m_out_plugin[MAX_PATH];
	int				m_user_sample_rate;
	bool			m_use_spdif;
	bool			m_invert_levels;
	int				m_refresh_time;
	bool			m_spdif_close_at_end;
	bool			m_outputchanged;
	int				m_output_mode;

	void ReadConfig();
	void WriteConfig();

public:
	outMixer(void);
	~outMixer(void);

	virtual void Config(HWND hwndParent);
	virtual void About(HWND hwndParent);
	virtual int  Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms);
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

	//Config dialog access
	inline DVDGraph		*get_DvdGraph() { return &m_dvd_graph; }
	inline double		get_CpuUsage() { return m_cpu.usage(); }
	inline void	get_Input(Speakers *spk) { if (spk) *spk = m_in_spk; }
	inline void	get_Output(Speakers *spk) { if (spk) *spk = m_out_spk; }
	inline int	get_SampleRate() { return m_user_sample_rate; }
	inline bool	get_UseSpdif() { return m_use_spdif; }
	inline bool	get_InvertLevels() { return m_invert_levels; }
	inline int	get_RefreshTime() { return m_refresh_time; }
	inline bool	get_SpdifCloseAtEnd() { return m_spdif_close_at_end; }
	inline void	set_InvertLevels(bool value) { m_invert_levels = value; }
	inline void	set_RefreshTime(int value) { m_refresh_time = value; }
	inline void	set_SpdifCloseAtEnd(bool value) { m_spdif_close_at_end = value; }

	void ChangeOutput(int ispk, int ifmt, int rate, bool use_spdif);
	void set_OutputPlugin(const char *path);
};
