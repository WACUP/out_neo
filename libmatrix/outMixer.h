#pragma once

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <wchar.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>


#define cbrtf(v) pow((double)v, 1.0/3.0)
#define lroundf(v) (long) (v + (v >= 0.0f ? 0.5f : -0.5f))



#include "filters\dvd_graph.h"
#include "win32\cpu.h"
#include "config\DevilConfig.h"

class TabDlg;

class outMixer
{
private:
	DVDGraph	m_dvd_graph;
	Chunk		m_chunk;
	Speakers	m_in_spk;
	Speakers	m_out_spk;
	CPUMeter	m_cpu;
	float		m_ratio;

	uint8_t		*p_empty_buffer;
	size_t		i_empty_size;

	//config
	static TabDlg	*p_config_dlg;
	DevilConfig		*m_pConfig;
	int				m_output_mode;
	bool			m_invert_levels;
	int				m_refresh_time;
	bool			m_asis;

	void ReadConfig();
	void WriteConfig();

public:
	outMixer(vlc_fourcc_t i_format, int i_mode, unsigned int i_rate);
	~outMixer(void);


	void Process(uint8_t *p_buffer, size_t i_size);
	Chunk *getChunk(void);
	void reset(void) { m_dvd_graph.reset(); }

	inline uint32_t	getChannels(void)
	{
		switch (m_out_spk.mask)
		{
			case MODE_1_0:    return AOUT_CHAN_CENTER;
			case MODE_2_0:    return AOUT_CHANS_2_0;
			case MODE_QUADRO: return AOUT_CHANS_4_0;
			case MODE_3_2:    return AOUT_CHANS_5_0;
			case MODE_5_1:    return AOUT_CHANS_5_1;
		}
		return AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
	}
	inline int	getInputNbChannels(void)
	{
		switch (m_in_spk.mask)
		{
			case MODE_1_0:    return 1;
			case MODE_2_0:    return 2;
			case MODE_QUADRO: return 4;
			case MODE_3_2:    return 5;
			case MODE_5_1:    return 6;
		}
		return 2;
	}
	inline int	getNbChannels(void) 
	{
		switch (m_out_spk.mask)
		{
			case 0       :    return getInputNbChannels();
			case MODE_1_0:    return 1;
			case MODE_2_0:    return 2;
			case MODE_QUADRO: return 4;
			case MODE_3_2:    return 5;
			case MODE_5_1:    return 6;
		}
		return 2;
	}
	inline int	getRate(void) { return m_out_spk.sample_rate; }
	inline float	getRatio(void) { return m_ratio; }

	void Config(HWND hwndParent);
	double GetOutputTime();

	//Config dialog access
	inline DVDGraph		*get_DvdGraph() { return &m_dvd_graph; }
	inline double		get_CpuUsage() { return m_cpu.usage(); }
	inline void			get_Input(Speakers *spk) { if (spk) *spk = m_in_spk; }
	inline void			get_Output(Speakers *spk, bool *asis) { if (spk) *spk = m_out_spk; if (asis) *asis = m_asis; }
	inline void			set_OutputMode(int value) { m_out_spk.mask = value; m_asis = value == 0; }
	inline bool			get_InvertLevels() { return m_invert_levels; }
	inline void			set_InvertLevels(bool value) { m_invert_levels = value; }
	inline int			get_RefreshTime() { return m_refresh_time; }
	inline void			set_RefreshTime(int value) { m_refresh_time = value; }
};
