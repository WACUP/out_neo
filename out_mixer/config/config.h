#ifndef CONFIG_H
#define CONFIG_H

#include "tab.h"
#include "controls.h"
#include "..\resource.h"
#include "..\outMixer.h"

class ConfigDlg : public TabSheet
{
public:
	static ConfigDlg *create_main   (outMixer *_outMixer);
	static ConfigDlg *create_mixer  (outMixer *_outMixer);
	static ConfigDlg *create_gains  (outMixer *_outMixer);
#ifdef USE_SPDIF
	static ConfigDlg *create_spdif  (outMixer *_outMixer);
#endif
	//static ConfigDlg *create_about  (outMixer *_outMixer);

	ConfigDlg(UINT dlg_res, outMixer *_outMixer);

private:
	outMixer		*m_outMixer;
	DVDGraph		*m_dvd_graph;
	AudioProcessor	*m_proc;

	bool		m_visible;
	bool		m_refresh;

	///////////////////////////
	// GUI objects
	///////////////////////////

#ifdef LEGACY_CODE
	// Interface
	DoubleEdit	edt_refresh_time;
#endif
	// AGC
	DoubleEdit	edt_attack;
	DoubleEdit	edt_release;
	// DRC
	DoubleEdit	edt_drc_power;
	DoubleEdit	edt_drc_level;
	// Gain control
	DoubleEdit	edt_master;
	DoubleEdit	edt_gain;
	DoubleEdit	edt_voice;
	DoubleEdit	edt_sur;
	DoubleEdit	edt_lfe;
	// I/O Gains
	DoubleEdit	edt_in_gains[NCHANNELS];
	DoubleEdit	edt_out_gains[NCHANNELS];
	// Delay
	DoubleEdit	edt_delay[NCHANNELS];
	// Bass redirection
	DoubleEdit	edt_bass_freq;
	// Matrix
	DoubleEdit	edt_matrix[NCHANNELS][NCHANNELS];
#if 0
	// About
	LinkButton	lnk_home;
	LinkButton	lnk_forum;
#endif

	///////////////////////////
	// Values
	///////////////////////////

	// Input/Output Formats
	Speakers	old_in_spk;
	Speakers	in_spk;
	Speakers	out_spk;
#ifdef USE_SPDIF
	bool		use_spdif;
#endif
	// interface options
	bool		invert_levels;
#ifdef LEGACY_CODE
	int			refresh_time;
#endif
	TCHAR old_info[4096];
	// AGC options
	bool		auto_gain;
	bool		normalize;
	sample_t	attack;
	sample_t	release;
	// DRC
	bool		drc;
	sample_t	drc_power;
	sample_t	drc_level;
	// Master gain
	sample_t	master;
	sample_t	gain;
	// Mix levels
	sample_t	clev;
	sample_t	slev;
	sample_t	lfelev;
	// Input/output levels
	sample_t input_levels[NCHANNELS];
	sample_t output_levels[NCHANNELS];
	// Input/output gains
	sample_t	input_gains[NCHANNELS];
	sample_t	output_gains[NCHANNELS];
	// Delay
	bool		delay;
	float		delays[NCHANNELS];
	int			delay_units;
	// Matrix options
	bool		auto_matrix;
	bool		normalize_matrix;
	bool		voice_control;
	bool		expand_stereo;
	// Bass redirection
	bool		bass_redir;
	int			bass_freq;
	// Matrix
	matrix_t	old_matrix;
	matrix_t	matrix;
#ifdef USE_SPDIF
	// SPDIF
	int			spdif_dts_mode;
	int			spdif_dts_conv;
	bool		spdif_use_detector;
	bool		spdif_encode;
	bool		spdif_stereo_pt;
	int			spdif_bitrate;
	bool		spdif_as_pcm;
	bool		spdif_check_sr;
	bool		spdif_allow_48;
	bool		spdif_allow_44;
	bool		spdif_allow_32;
	bool		spdif_query_sink;
	bool		spdif_close_at_end;
#endif

	///////////////////////////
	// Methods
	///////////////////////////
	void switch_on();
	void switch_off();
	BOOL message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void update();
	void init_plugin_list();

	void init_controls();
	void reload_state();
	void update_static_controls();
	void update_dynamic_controls();
	void update_matrix_controls();

	/////////////////////////////////////////////////////////////////////////////
	// Handle control notifications
	void command(int control, int message);
};

#endif