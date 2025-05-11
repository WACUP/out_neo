#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <loader/loader/paths.h>
#include <loader/loader/utils.h>
#include <nu/autowide.h>
#include "config.h"

extern Out_Module * g_pModSlave;
extern outMixer* g_pMixer;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////

const double min_gain_level  = -20.0;
const double max_gain_level  = +20.0;
const double min_level = -50.0;
const int ticks = 5;

const int idc_level_in[6]   = { IDC_IN_L,  IDC_IN_C,  IDC_IN_R,  IDC_IN_SL,  IDC_IN_SR,  IDC_IN_LFE };
const int idc_level_out[6]  = { IDC_OUT_L, IDC_OUT_C, IDC_OUT_R, IDC_OUT_SL, IDC_OUT_SR, IDC_OUT_SW };

const int idc_slider_in[6]  = { IDC_SLI_IN_L,  IDC_SLI_IN_C,  IDC_SLI_IN_R,  IDC_SLI_IN_SL,  IDC_SLI_IN_SR,  IDC_SLI_IN_LFE  };
const int idc_slider_out[6] = { IDC_SLI_OUT_L, IDC_SLI_OUT_C, IDC_SLI_OUT_R, IDC_SLI_OUT_SL, IDC_SLI_OUT_SR, IDC_SLI_OUT_LFE };
const int idc_edt_in[6]     = { IDC_EDT_IN_L,  IDC_EDT_IN_C,  IDC_EDT_IN_R,  IDC_EDT_IN_SL,  IDC_EDT_IN_SR,  IDC_EDT_IN_LFE  };
const int idc_edt_out[6]    = { IDC_EDT_OUT_L, IDC_EDT_OUT_C, IDC_EDT_OUT_R, IDC_EDT_OUT_SL, IDC_EDT_OUT_SR, IDC_EDT_OUT_LFE };

const int idc_edt_delay[6]  = { IDC_EDT_DL, IDC_EDT_DC, IDC_EDT_DR, IDC_EDT_DSL, IDC_EDT_DSR, IDC_EDT_DLFE };

const int matrix_controls[6][6] =
{
	{ IDC_EDT_L_L,   IDC_EDT_C_L,   IDC_EDT_R_L,   IDC_EDT_SL_L,   IDC_EDT_SR_L,   IDC_EDT_LFE_L },
	{ IDC_EDT_L_C,   IDC_EDT_C_C,   IDC_EDT_R_C,   IDC_EDT_SL_C,   IDC_EDT_SR_C,   IDC_EDT_LFE_C },
	{ IDC_EDT_L_R,   IDC_EDT_C_R,   IDC_EDT_R_R,   IDC_EDT_SL_R,   IDC_EDT_SR_R,   IDC_EDT_LFE_R },
	{ IDC_EDT_L_SL,  IDC_EDT_C_SL,  IDC_EDT_R_SL,  IDC_EDT_SL_SL,  IDC_EDT_SR_SL,  IDC_EDT_LFE_SL },
	{ IDC_EDT_L_SR,  IDC_EDT_C_SR,  IDC_EDT_R_SR,  IDC_EDT_SL_SR,  IDC_EDT_SR_SR,  IDC_EDT_LFE_SR },
	{ IDC_EDT_L_LFE, IDC_EDT_C_LFE, IDC_EDT_R_LFE, IDC_EDT_SL_LFE, IDC_EDT_SR_LFE, IDC_EDT_LFE_LFE }
};


///////////////////////////////////////////////////////////////////////////////
// Tools
///////////////////////////////////////////////////////////////////////////////
inline void cr2crlf(char *_buf, int _size)
{
	int cnt = 0;

	char *src;
	char *dst;

	src = _buf;
	dst = _buf + _size;
	while (*src && src < dst)
	{
		if (*src == '\n')
			cnt++;
		src++;
	}

	dst = src + cnt;
	if (dst > _buf + _size)
		dst = _buf + _size;

	while (src != dst)
	{
		*dst-- = *src--;
		if (src[1] == '\n')
			*dst-- = '\r';
	}
}

const TCHAR *spklist[] = 
{
	TEXT("AS IS (no change)"),
	TEXT("1/0 - mono"),
	TEXT("2/0 - stereo"),
	TEXT("3/0 - 3 front"),
	TEXT("2/2 - quadro"),
	TEXT("3/1+SW 4.1 surround"),
	TEXT("3/2+SW 5.1 channels"),
	TEXT("Dolby Surround/ProLogic"),
	TEXT("Dolby ProLogic II")
};

inline int mode_index(Speakers spk)
{
	switch (spk.relation)
	{
		case RELATION_DOLBY:   return 7;
		case RELATION_DOLBY2:  return 8;
		default:
		{
			switch (spk.mask)
			{
				case 0:            return 0;
				case MODE_1_0:     return 1;
				case MODE_2_0:     return 2;
				case MODE_3_0:     return 3;
				case MODE_2_2:     return 4;
				case MODE_3_1 | CH_MASK_LFE: return 5;
				case MODE_3_2 | CH_MASK_LFE: return 6;
			}
		}
	}
	return 0;
}

const TCHAR *fmtlist[] = 
{
	TEXT("16-bit"),
	TEXT("24-bit"),
	TEXT("32-bit")
};

inline int format_index(Speakers spk)
{
	switch (spk.format)
	{
		case FORMAT_PCM16:    return 0;
		case FORMAT_PCM24:    return 1;
		case FORMAT_PCM32:    return 2;
	}
	return 0;
}

const TCHAR *ratelist[] =
{
	TEXT("AS IS (no change)"),
	TEXT("8 kHz"),
	TEXT("11.025 kHz"),
	TEXT("22.050 kHz"),
	TEXT("24 kHz"),
	TEXT("32 kHz"),
	TEXT("44.1 kHz"),
	TEXT("48 kHz"),
	TEXT("96 kHz"),
	TEXT("192 kHz")
};

inline int rate_index(int rate)
{
	switch (rate)
	{
		case 0:        return 0;
		case 8000:     return 1;
		case 11025:    return 2;
		case 22050:    return 3;
		case 24000:    return 4;
		case 32000:    return 5;
		case 44100:    return 6;
		case 48000:    return 7;
		case 96000:    return 8;
		case 192000:   return 9;
	}
	return 0;
}

const TCHAR *units_list[] =
{
	TEXT("Samples"),
	TEXT("Millisecs"),
	TEXT("Meters"),
	TEXT("Centimeters"),
	TEXT("Feet"),
	TEXT("Inches")
};

inline int unit_index(int units)
{
	switch (units)
	{
		case DELAY_SP: return 0;
		case DELAY_MS: return 1;
		case DELAY_M : return 2;
		case DELAY_CM: return 3;
		case DELAY_FT: return 4;
		case DELAY_IN: return 5;
		default:       return 0;
	};
}

inline int unit_from_index(int index)
{
	switch (index)
	{
		case 0:  return DELAY_SP;
		case 1:  return DELAY_MS;
		case 2:  return DELAY_M;
		case 3:  return DELAY_CM;
		case 4:  return DELAY_FT;
		case 5:  return DELAY_IN;
		default: return DELAY_SP;
	};
}

const int bitrate_list[] =
{
   32,  40,  48,  56,  64,  80,  96, 112, 128, 
  160, 192, 224, 256, 320, 384, 448, 512, 576, 640 
};

inline int bitrate_index(int bitrate)
{
	bitrate /= 1000;
	for (int i = 0; i < ARRAYSIZE(bitrate_list); i++)
		if (bitrate_list[i] >= bitrate)
			return i;
	return ARRAYSIZE(bitrate_list) - 1;
}

inline int bitrate_from_index(int index)
{
	if (index >= 0 && index < ARRAYSIZE(bitrate_list))
		return bitrate_list[index] * 1000;
	else
		return 448000; // default bitrate
}

///////////////////////////////////////////////////////////////////////////////
// Initialization / Deinitialization
///////////////////////////////////////////////////////////////////////////////
ConfigDlg *ConfigDlg::create_main(outMixer *_outMixer)
{ return new ConfigDlg(IDD_MAIN, _outMixer); }

ConfigDlg *ConfigDlg::create_mixer(outMixer *_outMixer)
{ return new ConfigDlg(IDD_MIXER, _outMixer); }

ConfigDlg *ConfigDlg::create_gains(outMixer *_outMixer)
{ return new ConfigDlg(IDD_GAINS, _outMixer); }

#ifdef USE_SPDIF
ConfigDlg *ConfigDlg::create_spdif(outMixer *_outMixer)
{ return new ConfigDlg(IDD_SPDIF, _outMixer); }
#endif

/*ConfigDlg *ConfigDlg::create_about(outMixer *_outMixer)
{ return new ConfigDlg(IDD_ABOUT, _outMixer); }*/

ConfigDlg::ConfigDlg(UINT _dlg_res, outMixer *_outMixer) 
:TabSheet(_dlg_res)
{
	m_outMixer = _outMixer;
	m_dvd_graph = m_outMixer->get_DvdGraph();
	m_proc = &m_dvd_graph->proc;
}

void ConfigDlg::switch_on()
{
	/////////////////////////////////////
	// Refresh dialog state
	m_refresh = true;
	init_controls();
	update();

	/////////////////////////////////////
	// Start timers
#ifdef LEGACY_CODE
	SetTimer(hwnd, 1, refresh_time, 0);	// for all dynamic controls
#else
	SetTimer(hwnd, 1, m_outMixer->get_RefreshTime(), 0);	// for all dynamic controls
#endif

	TabSheet::switch_on();
}

void ConfigDlg::switch_off()
{
	KillTimer(hwnd, 1);
	TabSheet::switch_off();
}

///////////////////////////////////////////////////////////////////////////////
// Handle messages
///////////////////////////////////////////////////////////////////////////////

BOOL ConfigDlg::message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			command(LOWORD(wParam), HIWORD(wParam));
			return 1;
		}
		case WM_HSCROLL:
		case WM_VSCROLL:
		{
			command(GetDlgCtrlID((HWND)lParam), LOWORD(wParam));
			return 1;
		}
		case WM_TIMER:
		{
			if (IsWindowVisible(hwnd))
				if (m_visible)
				{
					switch (wParam)
					{
						case 1:
							reload_state();
							if (in_spk != old_in_spk)
								update_static_controls();
							update_dynamic_controls();
							break;
					}
				}
				else
				{
					m_refresh = true;
					m_visible = true;
					update();
				}
			else
				m_visible = false;
			return 1;
		}
	}

	return TabSheet::message(hwnd, uMsg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////////
// Controls initalization/update
///////////////////////////////////////////////////////////////////////////////

void ConfigDlg::update()
{
	reload_state();
	update_dynamic_controls();
	update_static_controls();
}

Out_Module* get_out_plugin(LPCWSTR file_name)
{
	WINAMPGETOUTMODULE winampGetOutModule = ((file_name != (LPCWSTR)CB_ERR) ? (WINAMPGETOUTMODULE)GetProcAddress(
																	  GetDllHandle(GetPaths()->winamp_plugin_dir,
																	  file_name, NULL), "winampGetOutModule") : 0);
	return (winampGetOutModule ? winampGetOutModule() : NULL);
}

void ConfigDlg::init_plugin_list()
{
	WCHAR szFullpath[MAX_PATH] = { 0 };
	WCHAR szMasterName[MAX_PATH] = { 0 };
	WCHAR *pszMasterName;
	WCHAR szSlaveName[MAX_PATH] = { 0 };
	WCHAR *pszSlaveName = 0;

	// get master plugin file name
	GetModuleFileName(g_OutModMaster.hDllInstance, szMasterName, MAX_PATH - 6 - 1);
	pszMasterName = (LPWSTR)FindPathFileName(szMasterName);

	// get slave plugin file name
	if (g_pMixer)
	{
		pszSlaveName = (LPWSTR)g_pMixer->get_OutputPlugin();
	}
	else
	{
		if (g_pModSlave && g_pModSlave->hDllInstance)
		{
			GetModuleFileName(g_pModSlave->hDllInstance, szSlaveName, MAX_PATH - 6 - 1);
			pszSlaveName = (LPWSTR)FindPathFileName(szSlaveName);
		}
	}

	BOOL re = TRUE;
	LPWIN32_FIND_DATA File = NULL;
	HANDLE hSearch = SearchFiles(szFullpath, GetPaths()->winamp_plugin_dir, L"out_*.dll", &File);
	SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_RESETCONTENT, 0, 0);
	if (File && (hSearch != INVALID_HANDLE_VALUE))
	{
		int index = -1;
		while (re)
		{
			if (IsValidPluginFile(File->cFileName))
			{
				if (pszMasterName && !SameStr(pszMasterName, File->cFileName) &&
					!IsDllBlocked(File->cFileName) && CanLoadPlugin(File->cFileName, TRUE))
				{
					Out_Module* out_plugin = get_out_plugin(File->cFileName);

					LPCWSTR desc = (out_plugin ? (((out_plugin->version == OUT_VER_U) ?
									(wchar_t *)out_plugin->description : ConvertANSI(
									out_plugin->description, -1, CP_ACP, NULL, 0, NULL))) : NULL);

					SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_SETITEMDATA,
					SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_ADDSTRING, 0, (LPARAM)
									   (desc && *desc ? desc : File->cFileName)),
									   (LPARAM)SafeWideDup(File->cFileName));
					++index;

					if (desc && (out_plugin->version == OUT_VER))
					{
						SafeFree((void*)desc);
					}
				}

				if (pszSlaveName && (wcscmp(pszSlaveName, File->cFileName) == 0))
				{
					SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_SETCURSEL, index, 0);
				}
			}

			re = FindNextFile(hSearch, File);
		}
	}
	SearchClose(hSearch, File);
}

void ConfigDlg::init_controls()
{
	int ch;

	/////////////////////////////////////
#if 0
	// Links
	lnk_home.link(hwnd, IDC_LNK_HOME);
	lnk_forum.link(hwnd, IDC_LNK_FORUM);
#endif

	/////////////////////////////////////
	// Build and environment info
	TCHAR info[1024] = {0};

	/*_tcsncpy(info, valib_build_info(), ARRAYSIZE(info));
	info[ARRAYSIZE(info)-1] = 0;
	cr2crlf(info, ARRAYSIZE(info));
	SetDlgItemText(hwnd, IDC_EDT_ENV, info);

	_tcsncpy(info, valib_credits(), ARRAYSIZE(info));
	info[ARRAYSIZE(info)-1] = 0;
	cr2crlf(info, ARRAYSIZE(info));
	SetDlgItemText(hwnd, IDC_EDT_CREDITS, info);*/

	/////////////////////////////////////
	// Version
	/*TCHAR ver1[255] = {0};
	TCHAR ver2[255] = {0};
	GetDlgItemText(parent, IDC_VER, ver1, ARRAYSIZE(ver1));
	_tprintf(ver2, ver1, PLUGIN_NAME);
	SetDlgItemText(parent, IDC_VER, ver2);*/

	/////////////////////////////////////
	// Speakers
	SendDlgItemMessage(hwnd, IDC_CMB_SPK, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(spklist); i++)
		SendDlgItemMessage(hwnd, IDC_CMB_SPK, CB_ADDSTRING, 0, (LPARAM) spklist[i]);

	/////////////////////////////////////
	// Formats
	SendDlgItemMessage(hwnd, IDC_CMB_FORMAT, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(fmtlist); i++)
		SendDlgItemMessage(hwnd, IDC_CMB_FORMAT, CB_ADDSTRING, 0, (LPARAM) fmtlist[i]);

	/////////////////////////////////////
	// Sample Rates
	SendDlgItemMessage(hwnd, IDC_CMB_RATE, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(ratelist); i++)
		SendDlgItemMessage(hwnd, IDC_CMB_RATE, CB_ADDSTRING, 0, (LPARAM) ratelist[i]);

	/////////////////////////////////////
	// Output plugin
	init_plugin_list();

	/////////////////////////////////////
	// Interface
#ifdef LEGACY_CODE
	edt_refresh_time.link(hwnd, IDC_EDT_REFRESH_TIME);
#endif

	/////////////////////////////////////
	// I/O Levels
	for (ch = 0; ch < NCHANNELS; ch++)
	{
		SendDlgItemMessage(hwnd, idc_level_in[ch],  PBM_SETBARCOLOR, 0, RGB(0, 128, 0));
		SendDlgItemMessage(hwnd, idc_level_out[ch], PBM_SETBARCOLOR, 0, RGB(0, 128, 0));
		SendDlgItemMessage(hwnd, idc_level_in[ch],  PBM_SETRANGE, 0, MAKELPARAM(0, -min_level * ticks));
		SendDlgItemMessage(hwnd, idc_level_out[ch], PBM_SETRANGE, 0, MAKELPARAM(0, -min_level * ticks));
	}

	/////////////////////////////////////
	// AGC
	edt_attack.link(hwnd, IDC_EDT_ATTACK);
	edt_release.link(hwnd, IDC_EDT_RELEASE);

	/////////////////////////////////////
	// DRC
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_POWER, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_POWER, TBM_SETTIC, 0, 0);
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_LEVEL, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_LEVEL, TBM_SETTIC, 0, 0);
	edt_drc_power.link(hwnd, IDC_EDT_DRC_POWER);
	edt_drc_level.link(hwnd, IDC_EDT_DRC_LEVEL);

	/////////////////////////////////////
	// Gains
	SendDlgItemMessage(hwnd, IDC_SLI_MASTER, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_MASTER, TBM_SETTIC, 0, 0);
	SendDlgItemMessage(hwnd, IDC_SLI_GAIN, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_GAIN, TBM_SETTIC, 0, 0);

	SendDlgItemMessage(hwnd, IDC_SLI_VOICE, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_VOICE, TBM_SETTIC, 0, 0);
	SendDlgItemMessage(hwnd, IDC_SLI_SUR, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_SUR, TBM_SETTIC, 0, 0);
	SendDlgItemMessage(hwnd, IDC_SLI_LFE, TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
	SendDlgItemMessage(hwnd, IDC_SLI_LFE, TBM_SETTIC, 0, 0);

	edt_master.link(hwnd, IDC_EDT_MASTER);
	edt_gain.link(hwnd, IDC_EDT_GAIN);
	edt_voice.link(hwnd, IDC_EDT_VOICE);
	edt_sur.link(hwnd, IDC_EDT_SUR);
	edt_lfe.link(hwnd, IDC_EDT_LFE);

	edt_gain.enable(false);

	/////////////////////////////////////
	// I/O Gains

	for (ch = 0; ch < NCHANNELS; ch++)
	{
		SendDlgItemMessage(hwnd, idc_slider_in[ch],  TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
		SendDlgItemMessage(hwnd, idc_slider_out[ch], TBM_SETRANGE, TRUE, MAKELONG(min_gain_level, max_gain_level) * ticks);
		SendDlgItemMessage(hwnd, idc_slider_in[ch],  TBM_SETTIC, 0, 0);
		SendDlgItemMessage(hwnd, idc_slider_out[ch], TBM_SETTIC, 0, 0);
		edt_in_gains[ch].link(hwnd, idc_edt_in[ch]);
		edt_out_gains[ch].link(hwnd, idc_edt_out[ch]);
	}

	/////////////////////////////////////
	// Delay

	for (ch = 0; ch < NCHANNELS; ch++)
		edt_delay[ch].link(hwnd, idc_edt_delay[ch]);

	SendDlgItemMessage(hwnd, IDC_CMB_UNITS, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(units_list); i++)
		SendDlgItemMessage(hwnd, IDC_CMB_UNITS, CB_ADDSTRING, 0, (LPARAM) units_list[i]);

	/////////////////////////////////////
	// Bass redirection

	edt_bass_freq.link(hwnd, IDC_EDT_BASS_FREQ);

	/////////////////////////////////////
	// Matrix

	for (int i = 0; i < 6; i++)
		for (int j = 0; j < 6; j++)
			edt_matrix[i][j].link(hwnd, matrix_controls[i][j]);

	/////////////////////////////////////
	// SPDIF
	SendDlgItemMessage(hwnd, IDC_CMB_SPDIF_BITRATE, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(bitrate_list); i++)
	{
		SendDlgItemMessage(hwnd, IDC_CMB_SPDIF_BITRATE, CB_SETITEMDATA,
		SendDlgItemMessage(hwnd, IDC_CMB_SPDIF_BITRATE, CB_ADDSTRING, 0,
			(LPARAM)_itot(bitrate_list[i], info, 10)), bitrate_list[i]);
	}
}

void ConfigDlg::reload_state()
{
	m_outMixer->get_Input(&in_spk);
	m_outMixer->get_Output(&out_spk);
#ifdef USE_SPDIF
	use_spdif = m_outMixer->get_UseSpdif();
#endif

	// Interface
	invert_levels = m_outMixer->get_InvertLevels();
#ifdef LEGACY_CODE
	refresh_time = m_outMixer->get_RefreshTime();
#endif

	// Input/output levels
	const vtime_t time = (m_outMixer->GetOutputTime() / 1000.0);
	if (time > 0)
	{
		m_proc->get_input_levels(time, input_levels);
		m_proc->get_output_levels(time, output_levels);
	}
	else
	{
		// if there's output time then we're very likely
		// to be in a stopped state so clear things here
		memset(input_levels, 0, sizeof(input_levels));
		memset(output_levels, 0, sizeof(output_levels));
	}

	// AGC options
	auto_gain = m_proc->get_auto_gain();
	normalize = m_proc->get_normalize();
	attack = m_proc->get_attack();
	release = m_proc->get_release();

	// DRC
	drc = m_proc->get_drc();
	drc_power = m_proc->get_drc_power();
	drc_level = m_proc->get_drc_level();

	// Master gain
	master = m_proc->get_master();
	gain = m_proc->get_gain();

	// Mix levels
	clev = m_proc->get_clev();
	slev = m_proc->get_slev();
	lfelev = m_proc->get_lfelev();

	// Input/output gains
	m_proc->get_input_gains(input_gains);
	m_proc->get_output_gains(output_gains);

	// Delay
	delay = m_proc->get_delay();
	m_proc->get_delays(delays);
	delay_units = m_proc->get_delay_units();

	// Matrix options
	auto_matrix = m_proc->get_auto_matrix();
	normalize_matrix = m_proc->get_normalize_matrix();
	voice_control = m_proc->get_voice_control();
	expand_stereo = m_proc->get_expand_stereo();

	// Bass redirection
	bass_redir = m_proc->get_bass_redir();
	bass_freq = m_proc->get_bass_freq();

	// Matrix
	m_proc->get_matrix(matrix);

#ifdef USE_SPDIF
	// SPDIF
	spdif_dts_mode = m_dvd_graph->get_dts_mode();
	spdif_dts_conv = m_dvd_graph->get_dts_conv();
	spdif_use_detector = m_dvd_graph->get_use_detector();
	spdif_encode = m_dvd_graph->get_spdif_encode();
	spdif_stereo_pt = m_dvd_graph->get_spdif_stereo_pt();
	spdif_bitrate = m_dvd_graph->get_spdif_bitrate();
	spdif_as_pcm = m_dvd_graph->get_spdif_as_pcm();
	spdif_check_sr = m_dvd_graph->get_spdif_check_sr();
	spdif_allow_48 = m_dvd_graph->get_spdif_allow_48();
	spdif_allow_44 = m_dvd_graph->get_spdif_allow_44();
	spdif_allow_32 = m_dvd_graph->get_spdif_allow_32();
	spdif_query_sink = m_dvd_graph->get_query_sink();
	spdif_close_at_end = m_outMixer->get_SpdifCloseAtEnd();
#endif
}

void ConfigDlg::update_static_controls()
{
	int ch;

	/////////////////////////////////////
	// Speakers
	SendDlgItemMessage(hwnd, IDC_CMB_SPK,    CB_SETCURSEL, mode_index(out_spk), 0);
	SendDlgItemMessage(hwnd, IDC_CMB_FORMAT, CB_SETCURSEL, format_index(out_spk), 0);
	SendDlgItemMessage(hwnd, IDC_CMB_RATE,	 CB_SETCURSEL, rate_index(m_outMixer->get_SampleRate()), 0);
#ifdef USE_SPDIF
	CheckDlgButton(hwnd, IDC_CHK_USE_SPDIF, use_spdif ? BST_CHECKED : BST_UNCHECKED);
#endif

	/////////////////////////////////////
	// Interface
	CheckDlgButton(hwnd, IDC_CHK_INVERT_LEVELS, invert_levels ? BST_CHECKED : BST_UNCHECKED);
#ifdef LEGACY_CODE
	edt_refresh_time.update_value(refresh_time);
#endif

	/////////////////////////////////////
	// Auto gain control
	SendDlgItemMessage(hwnd, IDC_SLI_MASTER, TBM_SETPOS, TRUE, long(-value2db(master) * ticks));
	edt_master.update_value(value2db(master));

	SendDlgItemMessage(hwnd, IDC_CHK_AUTO_GAIN, BM_SETCHECK, auto_gain ? BST_CHECKED : BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_CHK_NORMALIZE, BM_SETCHECK, normalize ? BST_CHECKED : BST_UNCHECKED, 1);
	EnableControl(hwnd, IDC_CHK_NORMALIZE, auto_gain);

	edt_attack.update_value(attack);
	edt_release.update_value(release);

	/////////////////////////////////////
	// DRC
	SendDlgItemMessage(hwnd, IDC_CHK_DRC, BM_SETCHECK, drc ? BST_CHECKED : BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_POWER, TBM_SETPOS, TRUE, long(-drc_power * ticks));
	edt_drc_power.update_value(drc_power);

	/////////////////////////////////////
	// Gain controls
	SendDlgItemMessage(hwnd, IDC_SLI_VOICE, TBM_SETPOS, TRUE, long(-value2db(clev)   * ticks));
	SendDlgItemMessage(hwnd, IDC_SLI_SUR,   TBM_SETPOS, TRUE, long(-value2db(slev)   * ticks));
	SendDlgItemMessage(hwnd, IDC_SLI_LFE,   TBM_SETPOS, TRUE, long(-value2db(lfelev) * ticks));

	edt_voice.update_value(value2db(clev));
	edt_sur  .update_value(value2db(slev));
	edt_lfe  .update_value(value2db(lfelev));

	/////////////////////////////////////
	// I/O Gains
	for (ch = 0; ch < NCHANNELS; ch++)
	{
		SendDlgItemMessage(hwnd, idc_slider_in[ch],  TBM_SETPOS, TRUE, long(-value2db(input_gains[ch])  * ticks));
		SendDlgItemMessage(hwnd, idc_slider_out[ch], TBM_SETPOS, TRUE, long(-value2db(output_gains[ch]) * ticks));
		edt_in_gains[ch].update_value(value2db(input_gains[ch]));
		edt_out_gains[ch].update_value(value2db(output_gains[ch]));
	}

	/////////////////////////////////////
	// Delay
	for (ch = 0; ch < NCHANNELS; ch++)
	{
		edt_delay[ch].update_value(delays[ch]);
		edt_delay[ch].enable(delay);
	}
	SendDlgItemMessage(hwnd, IDC_CHK_DELAYS, BM_SETCHECK, delay? BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_CMB_UNITS, CB_SETCURSEL, unit_index(delay_units), 0);
	EnableControl(hwnd, IDC_CMB_UNITS, delay);

	/////////////////////////////////////
	// Matrix Flags
	SendDlgItemMessage(hwnd, IDC_CHK_AUTO_MATRIX,   BM_SETCHECK, auto_matrix?      BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_CHK_EXPAND_STEREO, BM_SETCHECK, expand_stereo?    BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_CHK_VOICE_CONTROL, BM_SETCHECK, voice_control?    BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_CHK_NORM_MATRIX,   BM_SETCHECK, normalize_matrix? BST_CHECKED: BST_UNCHECKED, 1);
	EnableControl(hwnd, IDC_CHK_EXPAND_STEREO, auto_matrix);
	EnableControl(hwnd, IDC_CHK_VOICE_CONTROL, auto_matrix);
	EnableControl(hwnd, IDC_CHK_NORM_MATRIX, auto_matrix);

	/////////////////////////////////////
	// Bass redirection
	SendDlgItemMessage(hwnd, IDC_CHK_BASS_REDIR, BM_SETCHECK, bass_redir? BST_CHECKED: BST_UNCHECKED, 1);
	edt_bass_freq.update_value(bass_freq);

	/////////////////////////////////////
	// Matrix
	update_matrix_controls();

#ifdef USE_SPDIF
	/////////////////////////////////////
	// SPDIF/DTS output mode
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_MODE_AUTO,    BM_SETCHECK, spdif_dts_mode == DTS_MODE_AUTO? BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_MODE_WRAPPED, BM_SETCHECK, spdif_dts_mode == DTS_MODE_WRAPPED? BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_MODE_PADDED,  BM_SETCHECK, spdif_dts_mode == DTS_MODE_PADDED? BST_CHECKED: BST_UNCHECKED, 1);

	/////////////////////////////////////
	// SPDIF/DTS conversion
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_CONV_NONE,    BM_SETCHECK, spdif_dts_conv == DTS_CONV_NONE? BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_CONV_14BIT,   BM_SETCHECK, spdif_dts_conv == DTS_CONV_14BIT? BST_CHECKED: BST_UNCHECKED, 1);
	SendDlgItemMessage(hwnd, IDC_RBT_DTS_CONV_16BIT,   BM_SETCHECK, spdif_dts_conv == DTS_CONV_16BIT? BST_CHECKED: BST_UNCHECKED, 1);

	/////////////////////////////////////
	// SPDIF options
	CheckDlgButton(hwnd, IDC_CHK_USE_DETECTOR, spdif_use_detector? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_ENCODE, spdif_encode? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_STEREO_PT, spdif_stereo_pt? BST_CHECKED: BST_UNCHECKED);
	SendDlgItemMessage(hwnd, IDC_CMB_SPDIF_BITRATE, CB_SETCURSEL, bitrate_index(spdif_bitrate), 0);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_AS_PCM, spdif_as_pcm? BST_CHECKED: BST_UNCHECKED);

	CheckDlgButton(hwnd, IDC_CHK_SPDIF_CHECK_SR, spdif_check_sr? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_ALLOW_48, spdif_allow_48? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_ALLOW_44, spdif_allow_44? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_SPDIF_ALLOW_32, spdif_allow_32? BST_CHECKED: BST_UNCHECKED);

	EnableControl(hwnd, IDC_CHK_SPDIF_STEREO_PT, spdif_encode);
	EnableControl(hwnd, IDC_CHK_SPDIF_ALLOW_48, spdif_check_sr);
	EnableControl(hwnd, IDC_CHK_SPDIF_ALLOW_44, spdif_check_sr);
	EnableControl(hwnd, IDC_CHK_SPDIF_ALLOW_32, spdif_check_sr);

	CheckDlgButton(hwnd, IDC_CHK_QUERY_SINK, spdif_query_sink ? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_CHK_CLOSE_SPDIF, spdif_close_at_end ? BST_CHECKED: BST_UNCHECKED);
#endif
}

void ConfigDlg::update_dynamic_controls()
{
	if ((in_spk != old_in_spk) || m_refresh)
	{
		TCHAR buf[128] = { 0 };
		old_in_spk = in_spk;
		PrintfCch(buf, ARRAYSIZE(buf),
#ifdef _UNICODE
				  TEXT("Output format    (Current input format: %hs %hs @ %iHz)"),
#else
				  TEXT("Output format    (Current input format: %s %s @ %iHz)"),
#endif
				  in_spk.format_text(),
				  in_spk.mode_text(),
				  in_spk.sample_rate);
		SetDlgItemText(hwnd, IDC_GRP_OUTPUT, buf);
	}

	/////////////////////////////////////
	// Stream info
	char infoA[ARRAYSIZE(old_info)] = { 0 };
	wchar_t infoW[ARRAYSIZE(old_info)] = { 0 };
	m_dvd_graph->get_info(infoA, ARRAYSIZE(old_info));
#ifndef _UNICODDE
	cr2crlf(infoA, ARRAYSIZE(old_info));
#endif
	PrintfCch(infoW, ARRAYSIZE(infoW), L"%S", infoA);

	if (memcmp(infoW, old_info, ARRAYSIZE(old_info)) || m_refresh)
	{
		memcpy(old_info, infoW, ARRAYSIZE(old_info) * sizeof(TCHAR));
		SendDlgItemMessage(hwnd, IDC_EDT_INFO, WM_SETTEXT, 0, (LPARAM)infoW);
	}

#ifdef USE_SPDIF
	/////////////////////////////////////
	// Frames/errors
	PrintfCch(infoW, ARRAYSIZE(infoW), L"%i",
			  m_dvd_graph->dec.get_frames() +
			  m_dvd_graph->spdifer_pt.get_frames() +
			  m_dvd_graph->spdifer_enc.get_frames());
	SetDlgItemText(hwnd, IDC_EDT_FRAMES, infoW);
	PrintfCch(infoW, ARRAYSIZE(infoW), L"%i",
			  m_dvd_graph->dec.get_errors() +
			  m_dvd_graph->spdifer_pt.get_errors() +
			  m_dvd_graph->spdifer_enc.get_errors());
	SetDlgItemText(hwnd, IDC_EDT_ERRORS, infoW);
#endif

	/////////////////////////////////////
	// I/O Levels
	for (int ch = 0; ch < NCHANNELS; ch++)
	{
		if (invert_levels)
		{
			SendDlgItemMessage(hwnd, idc_level_in[ch],  PBM_SETPOS, input_levels[ch]  > 0 ? long(-value2db(input_levels[ch]) * ticks) : long(-min_level * ticks), 0);
			SendDlgItemMessage(hwnd, idc_level_out[ch], PBM_SETPOS, output_levels[ch]  > 0 ? long(-value2db(output_levels[ch]) * ticks) : long(-min_level * ticks), 0);
		}
		else
		{
			SendDlgItemMessage(hwnd, idc_level_in[ch],  PBM_SETPOS, input_levels[ch]  > 0 ? long((value2db(input_levels[ch]) - min_level) * ticks): 0, 0);
			SendDlgItemMessage(hwnd, idc_level_out[ch], PBM_SETPOS, output_levels[ch]  > 0 ? long((value2db(output_levels[ch]) - min_level) * ticks): 0, 0);
		}
		SendDlgItemMessage(hwnd, idc_level_out[ch], PBM_SETBARCOLOR, 0, (output_levels[ch] > 0.99)? RGB(255, 0, 0): RGB(0, 128, 0));
	}

	/////////////////////////////////////
	// Auto gain control
	SendDlgItemMessage(hwnd, IDC_SLI_GAIN, TBM_SETPOS, TRUE, long(-value2db(gain) * ticks));
	edt_gain.update_value(value2db(gain));

	/////////////////////////////////////
	// DRC
	SendDlgItemMessage(hwnd, IDC_SLI_DRC_LEVEL, TBM_SETPOS, TRUE, long(-value2db(drc_level) * ticks));
	edt_drc_level.update_value(value2db(drc_level));

	/////////////////////////////////////
	// Matrix controls
	if (auto_matrix)
		update_matrix_controls();

	m_refresh = false;
}

void ConfigDlg::update_matrix_controls()
{
	matrix_t _matrix;
	m_proc->get_matrix(_matrix);

	const bool _auto_matrix = m_proc->get_auto_matrix();

	if (memcmp(&old_matrix, &_matrix, sizeof(matrix_t)) || !_auto_matrix || m_refresh)
	{
		memcpy(&old_matrix, &_matrix, sizeof(matrix_t));
		for (int i = 0; i < 6; i++)
			for (int j = 0; j < 6; j++)
			{
				edt_matrix[i][j].update_value(_matrix[j][i]);
				SendDlgItemMessage(hwnd, matrix_controls[j][i], EM_SETREADONLY, _auto_matrix, 0);
			}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Commands
///////////////////////////////////////////////////////////////////////////////

void ConfigDlg::command(int control, int message)
{
	/////////////////////////////////////
	// Matrix controls
	if (message == CB_ENTER)
	{
		matrix_t matrix;
		m_proc->get_matrix(matrix);
		bool update_matrix = false;

		for (int i = 0; i < 6; i++)
			for (int j = 0; j < 6; j++)
				if (control == matrix_controls[i][j])
				{
					matrix[j][i] = edt_matrix[i][j].value;
					update_matrix = true;
				}

		if (update_matrix)
		{
			m_proc->set_matrix(matrix);
			update_matrix_controls();
			return;
		}
	}

	switch (control)
	{
		/////////////////////////////////////
		// Speaker selection
		case IDC_CMB_SPK:
		case IDC_CMB_FORMAT:
		case IDC_CMB_RATE:
#ifdef USE_SPDIF
		case IDC_CHK_USE_SPDIF:
#endif
		{
			if (control == IDC_CHK_USE_SPDIF || message == CBN_SELENDOK)
			{
				const int ispk = (const int)SendDlgItemMessage(hwnd, IDC_CMB_SPK, CB_GETCURSEL, 0, 0),
						  ifmt = (const int)SendDlgItemMessage(hwnd, IDC_CMB_FORMAT, CB_GETCURSEL, 0, 0),
						  rate = (const int)SendDlgItemMessage(hwnd, IDC_CMB_RATE, CB_GETCURSEL, 0, 0);
#ifdef USE_SPDIF
				if (GetDlgItem(hwnd, IDC_CHK_USE_SPDIF))
					use_spdif = IsDlgButtonChecked(hwnd, IDC_CHK_USE_SPDIF) == BST_CHECKED;
				m_outMixer->ChangeOutput(ispk, ifmt, rate, use_spdif);
#else
				m_outMixer->ChangeOutput(ispk, ifmt, rate);
#endif
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Interface
		case IDC_CHK_INVERT_LEVELS:
		{
			invert_levels = IsDlgButtonChecked(hwnd, IDC_CHK_INVERT_LEVELS) == BST_CHECKED;
			m_outMixer->set_InvertLevels(invert_levels);
			update();
			break;
		}
#ifdef LEGACY_CODE
		case IDC_EDT_REFRESH_TIME:
		{
			if (message == CB_ENTER)
			{
				refresh_time = int(edt_refresh_time.value);
				m_outMixer->set_RefreshTime(refresh_time);
				SetTimer(hwnd, 1, refresh_time, 0);
				update();
			}
			break;
		}
#endif

		/////////////////////////////////////
		// Output plugin selection
		case IDC_CMB_OUTPUT:
		{
			if (message == CBN_SELENDOK)
			{
				m_outMixer->set_OutputPlugin((LPCTSTR)SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_GETITEMDATA,
											SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_GETCURSEL, 0, 0), 0));
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Configure/About output plugin
		case IDC_BTN_CONFIGURE:
		case IDC_BTN_ABOUT:
		{
			if (message == BN_CLICKED)
			{
				Out_Module* out_plugin = get_out_plugin((LPCTSTR)SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_GETITEMDATA,
														SendDlgItemMessage(hwnd, IDC_CMB_OUTPUT, CB_GETCURSEL, 0, 0), 0));
				if (out_plugin)
				{
					if (control == IDC_BTN_CONFIGURE)
					{
						if (out_plugin->Config)
						{
							out_plugin->Config(hwnd);
						}
					}
					else
					{
						if (out_plugin->About)
						{
							out_plugin->About(hwnd);
						}
					}
				}
			}
			break;
		}

		/////////////////////////////////////
		// DRC
		case IDC_CHK_DRC:
		{
			drc = (SendDlgItemMessage(hwnd, IDC_CHK_DRC, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_drc(drc);
			update();
			break;
		}
		case IDC_SLI_DRC_POWER:
		{
			if (message == TB_THUMBPOSITION || message == TB_ENDTRACK)
			{
				drc_power = -double(SendDlgItemMessage(hwnd, IDC_SLI_DRC_POWER, TBM_GETPOS, 0, 0)) / ticks;
				m_proc->set_drc_power(drc_power);
				update();
			}
			break;
		}
		case IDC_EDT_DRC_POWER:
		{
			if (message == CB_ENTER)
			{
				drc_power = edt_drc_power.value;
				m_proc->set_drc_power(drc_power);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Auto gain control
		case IDC_SLI_MASTER:
		{
			if (message == TB_THUMBPOSITION || message == TB_ENDTRACK)
			{
				master = db2value(-double(SendDlgItemMessage(hwnd, IDC_SLI_MASTER,TBM_GETPOS, 0, 0))/ticks);
				m_proc->set_master(master);
				update();
			}
			break;
		}
		case IDC_EDT_MASTER:
		{
			if (message == CB_ENTER)
			{
				m_proc->set_master(db2value(edt_master.value));
				update();
			}
			break;
		}
		case IDC_EDT_ATTACK:
		{
			if (message == CB_ENTER)
			{
				attack = edt_attack.value;
				m_proc->set_attack(attack);
				update();
			}
			break;
		}
		case IDC_EDT_RELEASE:
		{
			if (message == CB_ENTER)
			{
				release = edt_release.value;
				m_proc->set_release(release);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Gain controls
		case IDC_SLI_VOICE:
		case IDC_SLI_SUR:
		case IDC_SLI_LFE:
		{
			if (message == TB_THUMBPOSITION || message == TB_ENDTRACK)
			{
				clev = db2value(-double(SendDlgItemMessage(hwnd, IDC_SLI_VOICE, TBM_GETPOS, 0, 0))/ticks);
				slev = db2value(-double(SendDlgItemMessage(hwnd, IDC_SLI_SUR,   TBM_GETPOS, 0, 0))/ticks);
				lfelev = db2value(-double(SendDlgItemMessage(hwnd, IDC_SLI_LFE,   TBM_GETPOS, 0, 0))/ticks);
				m_proc->set_clev(clev);
				m_proc->set_slev(slev);
				m_proc->set_lfelev(lfelev);
				update();
			}
			break;
		}
		case IDC_EDT_VOICE:
		case IDC_EDT_SUR:
		case IDC_EDT_LFE:
		{
			if (message == CB_ENTER)
			{  
				clev = db2value(edt_voice.value);
				slev = db2value(edt_sur.value);
				lfelev = db2value(edt_lfe.value);
				m_proc->set_clev(clev);
				m_proc->set_slev(slev);
				m_proc->set_lfelev(lfelev);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// I/O Gains
		case IDC_SLI_IN_L:
		case IDC_SLI_IN_C:
		case IDC_SLI_IN_R:
		case IDC_SLI_IN_SL:
		case IDC_SLI_IN_SR:
		case IDC_SLI_IN_LFE:
		{
			if (message == TB_THUMBPOSITION || message == TB_ENDTRACK)
			{
				for (int ch = 0; ch < NCHANNELS; ch++)
					input_gains[ch] = db2value(-double(SendDlgItemMessage(hwnd, idc_slider_in[ch], TBM_GETPOS, 0, 0))/ticks);

				m_proc->set_input_gains(input_gains);
				update();
			}
			break;
		}
		case IDC_EDT_IN_L:
		case IDC_EDT_IN_C:
		case IDC_EDT_IN_R:
		case IDC_EDT_IN_SL:
		case IDC_EDT_IN_SR:
		case IDC_EDT_IN_LFE:
		{
			if (message == CB_ENTER)
			{
				for (int ch = 0; ch < NCHANNELS; ch++)
					input_gains[ch] = db2value(edt_in_gains[ch].value);

				m_proc->set_input_gains(input_gains);
				update();
			}
			break;
		}
		case IDC_SLI_OUT_L:
		case IDC_SLI_OUT_C:
		case IDC_SLI_OUT_R:
		case IDC_SLI_OUT_SL:
		case IDC_SLI_OUT_SR:
		case IDC_SLI_OUT_LFE:
		{
			if (message == TB_THUMBPOSITION || message == TB_ENDTRACK)
			{
				for (int ch = 0; ch < NCHANNELS; ch++)
					output_gains[ch] = db2value(-double(SendDlgItemMessage(hwnd, idc_slider_out[ch], TBM_GETPOS, 0, 0))/ticks);

				m_proc->set_output_gains(output_gains);
				update();
			}
			break;
		}
		case IDC_EDT_OUT_L:
		case IDC_EDT_OUT_C:
		case IDC_EDT_OUT_R:
		case IDC_EDT_OUT_SL:
		case IDC_EDT_OUT_SR:
		case IDC_EDT_OUT_LFE:
		{
			if (message == CB_ENTER)
			{
				for (int ch = 0; ch < NCHANNELS; ch++)
					output_gains[ch] = db2value(edt_out_gains[ch].value);

				m_proc->set_output_gains(output_gains);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Delay
		case IDC_CHK_DELAYS:
		{
			delay = (SendDlgItemMessage(hwnd, IDC_CHK_DELAYS, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_delay(delay);
			update();
			break;
		}
		case IDC_EDT_DL:
		case IDC_EDT_DC:
		case IDC_EDT_DR:
		case IDC_EDT_DSL:
		case IDC_EDT_DSR:
		case IDC_EDT_DLFE:
		{
			if (message == CB_ENTER)
			{
				for (int ch = 0; ch < NCHANNELS; ch++)
					delays[ch] = (float)edt_delay[ch].value;

				m_proc->set_delays(delays);
				update();
			}
			break;
		}
		case IDC_CMB_UNITS:
		{
			if (message == CBN_SELENDOK)
			{
				delay_units = unit_from_index((int)SendDlgItemMessage(hwnd, IDC_CMB_UNITS, CB_GETCURSEL, 0, 0));
				m_proc->set_delay_units(delay_units);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Bass redirection
		case IDC_CHK_BASS_REDIR:
		{
			bass_redir = (SendDlgItemMessage(hwnd, IDC_CHK_BASS_REDIR, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_bass_redir(bass_redir);
			update();
			break;
		}
		case IDC_EDT_BASS_FREQ:
		{
			if (message == CB_ENTER)
			{
				bass_freq = (int)edt_bass_freq.value;
				m_proc->set_bass_freq(bass_freq);
				update();
			}
			break;
		}

		/////////////////////////////////////
		// Flags
		case IDC_CHK_AUTO_GAIN:
		{
			auto_gain = (SendDlgItemMessage(hwnd, IDC_CHK_AUTO_GAIN, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_auto_gain(auto_gain);
			update();
			break;
		}
		case IDC_CHK_NORMALIZE:
		{
			normalize = (SendDlgItemMessage(hwnd, IDC_CHK_NORMALIZE, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_normalize(normalize);
			update();
			break;
		}
		case IDC_CHK_AUTO_MATRIX:
		{
			auto_matrix = (SendDlgItemMessage(hwnd, IDC_CHK_AUTO_MATRIX, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_auto_matrix(auto_matrix);
			m_refresh = true;
			update();
			break;
		}
		case IDC_CHK_NORM_MATRIX:
		{
			normalize_matrix = (SendDlgItemMessage(hwnd, IDC_CHK_NORM_MATRIX, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_normalize_matrix(normalize_matrix);
			update();
			break;
		}
		case IDC_CHK_VOICE_CONTROL:
		{
			voice_control = (SendDlgItemMessage(hwnd, IDC_CHK_VOICE_CONTROL, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_voice_control(voice_control);
			update();
			break;
		}
		case IDC_CHK_EXPAND_STEREO:
		{
			expand_stereo = (SendDlgItemMessage(hwnd, IDC_CHK_EXPAND_STEREO, BM_GETCHECK, 0, 0) == BST_CHECKED);
			m_proc->set_expand_stereo(expand_stereo);
			update();
			break;
		}

#ifdef USE_SPDIF
		/////////////////////////////////////
		// SPDIF/DTS output mode
		case IDC_RBT_DTS_MODE_AUTO:
		{
			spdif_dts_mode = DTS_MODE_AUTO;
			m_dvd_graph->set_dts_mode(spdif_dts_mode);
			update();
			break;
		}
		case IDC_RBT_DTS_MODE_WRAPPED:
		{
			spdif_dts_mode = DTS_MODE_WRAPPED;
			m_dvd_graph->set_dts_mode(spdif_dts_mode);
			update();
			break;
		}
		case IDC_RBT_DTS_MODE_PADDED:
		{
			spdif_dts_mode = DTS_MODE_PADDED;
			m_dvd_graph->set_dts_mode(spdif_dts_mode);
			update();
			break;
		}

		/////////////////////////////////////
		// SPDIF/DTS conversion
		case IDC_RBT_DTS_CONV_NONE:
		{
			spdif_dts_conv = DTS_CONV_NONE;
			m_dvd_graph->set_dts_conv(spdif_dts_conv);
			update();
			break;
		}
		case IDC_RBT_DTS_CONV_14BIT:
		{
			spdif_dts_conv = DTS_CONV_14BIT;
			m_dvd_graph->set_dts_conv(spdif_dts_conv);
			update();
			break;
		}
		case IDC_RBT_DTS_CONV_16BIT:
		{
			spdif_dts_conv = DTS_CONV_16BIT;
			m_dvd_graph->set_dts_conv(spdif_dts_conv);
			update();
			break;
		}

		/////////////////////////////////////
		// SPDIF options
		case IDC_CHK_USE_DETECTOR:
		{
			spdif_use_detector = IsDlgButtonChecked(hwnd, IDC_CHK_USE_DETECTOR) == BST_CHECKED;
			m_dvd_graph->set_use_detector(spdif_use_detector);
			break;
		}
		case IDC_CHK_SPDIF_AS_PCM:
		{
			spdif_as_pcm = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_AS_PCM) == BST_CHECKED;
			if (spdif_as_pcm)
				spdif_as_pcm = MessageBox(hwnd, TEXT("This option is DANGEROUS! Filter may make very loud ")
										  TEXT("noise with this option enabled. Press 'No' to enable this ")
										  TEXT("option."), TEXT("Dangerous option!"), MB_YESNO | MB_ICONWARNING) == IDNO;
			m_dvd_graph->set_spdif_as_pcm(spdif_as_pcm);
			update();
			break;
		}
		case IDC_CHK_SPDIF_ENCODE:
		{
			spdif_encode = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_ENCODE) == BST_CHECKED;
			m_dvd_graph->set_spdif_encode(spdif_encode);
			update();
			break;
		}
		case IDC_CHK_SPDIF_STEREO_PT:
		{
			spdif_stereo_pt = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_STEREO_PT) == BST_CHECKED;
			m_dvd_graph->set_spdif_stereo_pt(spdif_stereo_pt);
			update();
			break;
		}
		case IDC_CMB_SPDIF_BITRATE:
		{
			if (message == CBN_SELENDOK)
			{
				const int ibitrate = (const int )SendDlgItemMessage(hwnd, IDC_CMB_SPDIF_BITRATE, CB_GETCURSEL, 0, 0);
				if (ibitrate != CB_ERR)
				{
					spdif_bitrate = bitrate_from_index(ibitrate);
					m_dvd_graph->set_spdif_bitrate(spdif_bitrate);
				}
				update();
				break;
			}
		}
		case IDC_CHK_SPDIF_CHECK_SR:
		{
			spdif_check_sr = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_CHECK_SR) == BST_CHECKED;
			m_dvd_graph->set_spdif_check_sr(spdif_check_sr);
			update();
			break;
		}
		case IDC_CHK_SPDIF_ALLOW_48:
		{
			spdif_allow_48 = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_ALLOW_48) == BST_CHECKED;
			m_dvd_graph->set_spdif_allow_48(spdif_allow_48);
			update();
			break;
		}
		case IDC_CHK_SPDIF_ALLOW_44:
		{
			spdif_allow_44 = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_ALLOW_44) == BST_CHECKED;
			m_dvd_graph->set_spdif_allow_44(spdif_allow_44);
			update();
			break;
		}
		case IDC_CHK_SPDIF_ALLOW_32:
		{
			spdif_allow_32 = IsDlgButtonChecked(hwnd, IDC_CHK_SPDIF_ALLOW_32) == BST_CHECKED;
			m_dvd_graph->set_spdif_allow_32(spdif_allow_32);
			update();
			break;
		}
		case IDC_CHK_QUERY_SINK:
		{
			spdif_query_sink = IsDlgButtonChecked(hwnd, IDC_CHK_QUERY_SINK) == BST_CHECKED;
			m_dvd_graph->set_query_sink(spdif_query_sink);
			update();
			break;
		}
		case IDC_CHK_CLOSE_SPDIF:
		{
			spdif_close_at_end = IsDlgButtonChecked(hwnd, IDC_CHK_CLOSE_SPDIF) == BST_CHECKED;
			m_outMixer->set_SpdifCloseAtEnd(spdif_close_at_end);
			update();
			break;
		}
#endif
	}
}