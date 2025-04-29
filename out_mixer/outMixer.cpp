#include "outMixer.h"
#include "config/config.h"
#include <tchar.h>
#include <loader/loader/utils.h>

extern Out_Module *g_pModSlave;
extern bool switchOutPutPlugin(const TCHAR* path);

outMixer::outMixer(void)
{
	*m_out_plugin = '\0';
	m_pOutPlugin = new outPlugin();
#ifdef USE_SPDIF
	m_pOutDsound = new outDsound();
#endif

	m_pConfig = new DevilConfig( g_OutModMaster.hDllInstance );
	ReadConfig();

#ifdef USE_SPDIF
	m_pOut = m_use_spdif
			? (IOut*)m_pOutDsound
			: (IOut*)m_pOutPlugin;
#else
	m_pOut = (IOut*)m_pOutPlugin;
#endif
}

outMixer::~outMixer(void)
{
	WriteConfig();

	if (m_pOutPlugin)
	{
		delete m_pOutPlugin;
		m_pOutPlugin = NULL;
	}

#ifdef USE_SPDIF
	if (m_pOutDsound)
	{
		delete m_pOutDsound;
		m_pOutDsound = NULL;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
//  ReadConfig
////////////////////////////////////////////////////////////////////////////////
void outMixer::ReadConfig()
{
	if( !m_pConfig ) return;

	int iRes;
	double dRes;
	sample_t input_gains[NCHANNELS];
	sample_t output_gains[NCHANNELS];
	float delays[NCHANNELS];
	matrix_t matrix;

	// Output
	m_pConfig->Read(TEXT("iOutputMode"), &m_out_spk.mask, MODE_2_0);
	m_pConfig->Read(TEXT("iOutputRelation"), &m_out_spk.relation, NO_RELATION);
	m_pConfig->Read(TEXT("iOutputFormat"), &m_out_spk.format, FORMAT_PCM16);
	m_pConfig->Read(TEXT("iOutputRate"), &m_out_spk.sample_rate, 44100);
#ifdef USE_SPDIF
	m_pConfig->Read(TEXT("bUseSpdif"), &iRes, 0);
	m_use_spdif = (iRes==1);
#endif
	m_user_sample_rate = m_out_spk.sample_rate;
	m_out_spk.set(m_out_spk.format, m_out_spk.mask, m_out_spk.sample_rate, -1, m_out_spk.relation); // to set level correctly

	// Interface
	m_pConfig->Read(TEXT("bInvertLevels"), &iRes, 0);
	m_invert_levels = (iRes==1);
	m_pConfig->Read(TEXT("iRefreshTime"), &iRes, 125);
	m_refresh_time = iRes;

	// AGC options
	m_pConfig->Read(TEXT("bAutoGain"), &iRes, 1);
	m_dvd_graph.proc.set_auto_gain(iRes==1);
	m_pConfig->Read(TEXT("bNormalize"), &iRes, 0);
	m_dvd_graph.proc.set_normalize(iRes==1);
	m_pConfig->Read(TEXT("dAttack"), &dRes, 100);
	m_dvd_graph.proc.set_attack(dRes);
	m_pConfig->Read(TEXT("dRelease"), &dRes, 50);
	m_dvd_graph.proc.set_release(dRes);

	// DRC
	m_pConfig->Read(TEXT("bDRC"), &iRes, 0);
	m_dvd_graph.proc.set_drc(iRes==1);
	m_pConfig->Read(TEXT("dDRCPower"), &dRes, 0);
	m_dvd_graph.proc.set_drc_power(dRes);

	// Gain
	m_pConfig->Read(TEXT("dGainMaster"), &dRes, 1);
	m_dvd_graph.proc.set_master(dRes);
	m_pConfig->Read(TEXT("dGainVoice"), &dRes, 1);
	m_dvd_graph.proc.set_clev(dRes);
	m_pConfig->Read(TEXT("dGainSurround"), &dRes, 1);
	m_dvd_graph.proc.set_slev(dRes);
	m_pConfig->Read(TEXT("dGainLFE"), &dRes, 1);
	m_dvd_graph.proc.set_lfelev(dRes);

	// Input/output gains
	m_pConfig->Read(TEXT("dGainInputL"), &dRes, 1);
	input_gains[CH_L] = dRes;
	m_pConfig->Read(TEXT("dGainInputC"), &dRes, 1);
	input_gains[CH_C] = dRes;
	m_pConfig->Read(TEXT("dGainInputR"), &dRes, 1);
	input_gains[CH_R] = dRes;
	m_pConfig->Read(TEXT("dGainInputSL"), &dRes, 1);
	input_gains[CH_SL] = dRes;
	m_pConfig->Read(TEXT("dGainInputSR"), &dRes, 1);
	input_gains[CH_SR] = dRes;
	m_pConfig->Read(TEXT("dGainInputLFE"), &dRes, 1);
	input_gains[CH_LFE] = dRes;
	m_pConfig->Read(TEXT("dGainOutputL"), &dRes, 1);
	output_gains[CH_L] = dRes;
	m_pConfig->Read(TEXT("dGainOutputC"), &dRes, 1);
	output_gains[CH_C] = dRes;
	m_pConfig->Read(TEXT("dGainOutputR"), &dRes, 1);
	output_gains[CH_R] = dRes;
	m_pConfig->Read(TEXT("dGainOutputSL"), &dRes, 1);
	output_gains[CH_SL] = dRes;
	m_pConfig->Read(TEXT("dGainOutputSR"), &dRes, 1);
	output_gains[CH_SR] = dRes;
	m_pConfig->Read(TEXT("dGainOutputLFE"), &dRes, 1);
	output_gains[CH_LFE] = dRes;

	// Delay
	m_pConfig->Read(TEXT("bDelay"), &iRes, 0);
	m_dvd_graph.proc.set_delay(iRes==1);
	m_pConfig->Read(TEXT("dDelayL"), &dRes, 0);
	delays[CH_L] = (float)dRes;
	m_pConfig->Read(TEXT("dDelayC"), &dRes, 0);
	delays[CH_C] = (float)dRes;
	m_pConfig->Read(TEXT("dDelayR"), &dRes, 0);
	delays[CH_R] = (float)dRes;
	m_pConfig->Read(TEXT("dDelaySL"), &dRes, 0);
	delays[CH_SL] = (float)dRes;
	m_pConfig->Read(TEXT("dDelaySR"), &dRes, 0);
	delays[CH_SR] = (float)dRes;
	m_pConfig->Read(TEXT("dDelayLFE"), &dRes, 0);
	delays[CH_LFE] = (float)dRes;
	m_pConfig->Read(TEXT("iDelayUnits"), &iRes, 0);
	m_dvd_graph.proc.set_delay_units(iRes);

	// Matrix options
	m_pConfig->Read(TEXT("bAutoMatrix"), &iRes, 1);
	m_dvd_graph.proc.set_auto_matrix(iRes==1);
	m_pConfig->Read(TEXT("bNormalizeMatrix"), &iRes, 1);
	m_dvd_graph.proc.set_normalize_matrix(iRes==1);
	m_pConfig->Read(TEXT("bVoiceControl"), &iRes, 1);
	m_dvd_graph.proc.set_voice_control(iRes==1);
	m_pConfig->Read(TEXT("bExpandStereo"), &iRes, 1);
	m_dvd_graph.proc.set_expand_stereo(iRes==1);

	// Bass redirection
	m_pConfig->Read(TEXT("bBassRedirection"), &iRes, 0);
	m_dvd_graph.proc.set_bass_redir(iRes==1);
	m_pConfig->Read(TEXT("iBassFrequency"), &iRes, 0);
	m_dvd_graph.proc.set_bass_freq(iRes);

	// Matrix
	m_pConfig->Read(TEXT("dMatrix_L_L"), &dRes, 0);
	matrix[0][0] = dRes;
	m_pConfig->Read(TEXT("dMatrix_L_C"), &dRes, 0);
	matrix[0][1] = dRes;
	m_pConfig->Read(TEXT("dMatrix_L_R"), &dRes, 0);
	matrix[0][2] = dRes;
	m_pConfig->Read(TEXT("dMatrix_L_SL"), &dRes, 0);
	matrix[0][3] = dRes;
	m_pConfig->Read(TEXT("dMatrix_L_SR"), &dRes, 0);
	matrix[0][4] = dRes;
	m_pConfig->Read(TEXT("dMatrix_L_LFE"), &dRes, 0);
	matrix[0][5] = dRes;

	m_pConfig->Read(TEXT("dMatrix_C_L"), &dRes, 0);
	matrix[1][0] = dRes;
	m_pConfig->Read(TEXT("dMatrix_C_C"), &dRes, 0);
	matrix[1][1] = dRes;
	m_pConfig->Read(TEXT("dMatrix_C_R"), &dRes, 0);
	matrix[1][2] = dRes;
	m_pConfig->Read(TEXT("dMatrix_C_SL"), &dRes, 0);
	matrix[1][3] = dRes;
	m_pConfig->Read(TEXT("dMatrix_C_SR"), &dRes, 0);
	matrix[1][4] = dRes;
	m_pConfig->Read(TEXT("dMatrix_C_LFE"), &dRes, 0);
	matrix[1][5] = dRes;

	m_pConfig->Read(TEXT("dMatrix_R_L"), &dRes, 0);
	matrix[2][0] = dRes;
	m_pConfig->Read(TEXT("dMatrix_R_C"), &dRes, 0);
	matrix[2][1] = dRes;
	m_pConfig->Read(TEXT("dMatrix_R_R"), &dRes, 0);
	matrix[2][2] = dRes;
	m_pConfig->Read(TEXT("dMatrix_R_SL"), &dRes, 0);
	matrix[2][3] = dRes;
	m_pConfig->Read(TEXT("dMatrix_R_SR"), &dRes, 0);
	matrix[2][4] = dRes;
	m_pConfig->Read(TEXT("dMatrix_R_LFE"), &dRes, 0);
	matrix[2][5] = dRes;

	m_pConfig->Read(TEXT("dMatrix_SL_L"), &dRes, 0);
	matrix[3][0] = dRes;
	m_pConfig->Read(TEXT("dMatrix_SL_C"), &dRes, 0);
	matrix[3][1] = dRes;
	m_pConfig->Read(TEXT("dMatrix_SL_R"), &dRes, 0);
	matrix[3][2] = dRes;
	m_pConfig->Read(TEXT("dMatrix_SL_SL"), &dRes, 0);
	matrix[3][3] = dRes;
	m_pConfig->Read(TEXT("dMatrix_SL_SR"), &dRes, 0);
	matrix[3][4] = dRes;
	m_pConfig->Read(TEXT("dMatrix_SL_LFE"), &dRes, 0);
	matrix[3][5] = dRes;

	m_pConfig->Read(TEXT("dMatrixSR_L"), &dRes, 0);
	matrix[4][0] = dRes;
	m_pConfig->Read(TEXT("dMatrixSR_C"), &dRes, 0);
	matrix[4][1] = dRes;
	m_pConfig->Read(TEXT("dMatrixSR_R"), &dRes, 0);
	matrix[4][2] = dRes;
	m_pConfig->Read(TEXT("dMatrixSR_SL"), &dRes, 0);
	matrix[4][3] = dRes;
	m_pConfig->Read(TEXT("dMatrixSR_SR"), &dRes, 0);
	matrix[4][4] = dRes;
	m_pConfig->Read(TEXT("dMatrixSR_LFE"), &dRes, 0);
	matrix[4][5] = dRes;

	m_pConfig->Read(TEXT("dMatrixLFE_L"), &dRes, 0);
	matrix[5][0] = dRes;
	m_pConfig->Read(TEXT("dMatrixLFE_C"), &dRes, 0);
	matrix[5][1] = dRes;
	m_pConfig->Read(TEXT("dMatrixLFE_R"), &dRes, 0);
	matrix[5][2] = dRes;
	m_pConfig->Read(TEXT("dMatrixLFE_SL"), &dRes, 0);
	matrix[5][3] = dRes;
	m_pConfig->Read(TEXT("dMatrixLFE_SR"), &dRes, 0);
	matrix[5][4] = dRes;
	m_pConfig->Read(TEXT("dMatrixLFE_LFE"), &dRes, 0);
	matrix[5][5] = dRes;

	m_dvd_graph.proc.set_input_gains(input_gains);
	m_dvd_graph.proc.set_output_gains(output_gains);
	m_dvd_graph.proc.set_delays(delays);
	m_dvd_graph.proc.set_matrix(matrix);

#ifdef USE_SPDIF
	// SPDIF
	m_pConfig->Read(TEXT("iSpdifDtsMode"), &iRes, 0);
	m_dvd_graph.set_dts_mode(iRes);
	m_pConfig->Read(TEXT("iSpdifDtsConv"), &iRes, 0);
	m_dvd_graph.set_dts_conv(iRes);
	m_pConfig->Read(TEXT("bSpdifUseDetector"), &iRes, 0);
	m_dvd_graph.set_use_detector(iRes==1);
	m_pConfig->Read(TEXT("bSpdifEncode"), &iRes, 1);
	m_dvd_graph.set_spdif_encode(iRes==1);
	m_pConfig->Read(TEXT("bSpdifStereoPT"), &iRes, 1);
	m_dvd_graph.set_spdif_stereo_pt(iRes==1);
	m_pConfig->Read(TEXT("iSpdifBitrate"), &iRes, 640000);
	m_dvd_graph.set_spdif_bitrate(iRes);
	m_pConfig->Read(TEXT("bSpdifAsPcm"), &iRes, 0);
	m_dvd_graph.set_spdif_as_pcm(iRes==1);
	m_pConfig->Read(TEXT("bSpdifCheckSR"), &iRes, 1);
	m_dvd_graph.set_spdif_check_sr(iRes==1);
	m_pConfig->Read(TEXT("bSpdifAllow48"), &iRes, 1);
	m_dvd_graph.set_spdif_allow_48(iRes==1);
	m_pConfig->Read(TEXT("bSpdifAllow44"), &iRes, 0);
	m_dvd_graph.set_spdif_allow_44(iRes==1);
	m_pConfig->Read(TEXT("bSpdifAllow32"), &iRes, 0);
	m_dvd_graph.set_spdif_allow_32(iRes==1);
	m_pConfig->Read(TEXT("bSpdifQuerySink"), &iRes, 1);
	m_dvd_graph.set_query_sink(iRes==1);
	m_pConfig->Read(TEXT("bSpdifCloseAtEnd"), &iRes, 1);
	m_spdif_close_at_end = (iRes==1);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//  WriteConfig
////////////////////////////////////////////////////////////////////////////////
void outMixer::WriteConfig()
{
	if( !m_pConfig ) return;

	sample_t input_gains[NCHANNELS];
	sample_t output_gains[NCHANNELS];
	float delays[NCHANNELS];
	matrix_t matrix;

	m_dvd_graph.proc.get_input_gains(input_gains);
	m_dvd_graph.proc.get_output_gains(output_gains);
	m_dvd_graph.proc.get_delays(delays);
	m_dvd_graph.proc.get_matrix(matrix);

	// Output
	m_pConfig->Write(TEXT("iOutputMode"), m_out_spk.mask);
	m_pConfig->Write(TEXT("iOutputRelation"), m_out_spk.relation);
	m_pConfig->Write(TEXT("iOutputFormat"), m_out_spk.format);
	m_pConfig->Write(TEXT("iOutputRate"), m_user_sample_rate);
#ifdef USE_SPDIF
	m_pConfig->Write(TEXT("bUseSpdif"), m_use_spdif);
#endif

	// Interface
	m_pConfig->Write(TEXT("bInvertLevels"), (int)m_invert_levels);
	m_pConfig->Write(TEXT("iRefreshTime"), m_refresh_time);

	// AGC options
	m_pConfig->Write(TEXT("bAutoGain"), (int)m_dvd_graph.proc.get_auto_gain());
	m_pConfig->Write(TEXT("bNormalize"), m_dvd_graph.proc.get_normalize());
	m_pConfig->Write(TEXT("dAttack"), m_dvd_graph.proc.get_attack());
	m_pConfig->Write(TEXT("dRelease"), m_dvd_graph.proc.get_release());

	// DRC
	m_pConfig->Write(TEXT("bDRC"), (int)m_dvd_graph.proc.get_drc());
	m_pConfig->Write(TEXT("dDRCPower"), m_dvd_graph.proc.get_drc_power());

	// Gain
	m_pConfig->Write(TEXT("dGainMaster"), m_dvd_graph.proc.get_master());
	m_pConfig->Write(TEXT("dGainVoice"), m_dvd_graph.proc.get_clev());
	m_pConfig->Write(TEXT("dGainSurround"), m_dvd_graph.proc.get_slev());
	m_pConfig->Write(TEXT("dGainLFE"), m_dvd_graph.proc.get_lfelev());

	// Input/output gains
	m_pConfig->Write(TEXT("dGainInputL"), input_gains[CH_L]);
	m_pConfig->Write(TEXT("dGainInputC"), input_gains[CH_C]);
	m_pConfig->Write(TEXT("dGainInputR"), input_gains[CH_R]);
	m_pConfig->Write(TEXT("dGainInputSL"), input_gains[CH_SL]);
	m_pConfig->Write(TEXT("dGainInputSR"), input_gains[CH_SR]);
	m_pConfig->Write(TEXT("dGainInputLFE"), input_gains[CH_LFE]);
	m_pConfig->Write(TEXT("dGainOutputL"), output_gains[CH_L]);
	m_pConfig->Write(TEXT("dGainOutputC"), output_gains[CH_C]);
	m_pConfig->Write(TEXT("dGainOutputR"), output_gains[CH_R]);
	m_pConfig->Write(TEXT("dGainOutputSL"), output_gains[CH_SL]);
	m_pConfig->Write(TEXT("dGainOutputSR"), output_gains[CH_SR]);
	m_pConfig->Write(TEXT("dGainOutputLFE"), output_gains[CH_LFE]);

	// Delay
	m_pConfig->Write(TEXT("bDelay"), (int)m_dvd_graph.proc.get_delay());
	m_pConfig->Write(TEXT("dDelayL"), delays[CH_L]);
	m_pConfig->Write(TEXT("dDelayC"), delays[CH_C]);
	m_pConfig->Write(TEXT("dDelayR"), delays[CH_R]);
	m_pConfig->Write(TEXT("dDelaySL"), delays[CH_SL]);
	m_pConfig->Write(TEXT("dDelaySR"), delays[CH_SR]);
	m_pConfig->Write(TEXT("dDelayLFE"), delays[CH_LFE]);
	m_pConfig->Write(TEXT("iDelayUnits"), m_dvd_graph.proc.get_delay_units());

	// Matrix options
	m_pConfig->Write(TEXT("bAutoMatrix"), (int)m_dvd_graph.proc.get_auto_matrix());
	m_pConfig->Write(TEXT("bNormalizeMatrix"), (int)m_dvd_graph.proc.get_normalize_matrix());
	m_pConfig->Write(TEXT("bVoiceControl"), (int)m_dvd_graph.proc.get_voice_control());
	m_pConfig->Write(TEXT("bExpandStereo"), (int)m_dvd_graph.proc.get_expand_stereo());

	// Bass redirection
	m_pConfig->Write(TEXT("bBassRedirection"), (int)m_dvd_graph.proc.get_bass_redir());
	m_pConfig->Write(TEXT("iBassFrequency"), m_dvd_graph.proc.get_bass_freq());

	// Matrix
	m_pConfig->Write(TEXT("dMatrix_L_L"), matrix[0][0]);
	m_pConfig->Write(TEXT("dMatrix_L_C"), matrix[0][1]);
	m_pConfig->Write(TEXT("dMatrix_L_R"), matrix[0][2]);
	m_pConfig->Write(TEXT("dMatrix_L_SL"), matrix[0][3]);
	m_pConfig->Write(TEXT("dMatrix_L_SR"), matrix[0][4]);
	m_pConfig->Write(TEXT("dMatrix_L_LFE"), matrix[0][5]);

	m_pConfig->Write(TEXT("dMatrix_C_L"), matrix[1][0]);
	m_pConfig->Write(TEXT("dMatrix_C_C"), matrix[1][1]);
	m_pConfig->Write(TEXT("dMatrix_C_R"), matrix[1][2]);
	m_pConfig->Write(TEXT("dMatrix_C_SL"), matrix[1][3]);
	m_pConfig->Write(TEXT("dMatrix_C_SR"), matrix[1][4]);
	m_pConfig->Write(TEXT("dMatrix_C_LFE"), matrix[1][5]);

	m_pConfig->Write(TEXT("dMatrix_R_L"), matrix[2][0]);
	m_pConfig->Write(TEXT("dMatrix_R_C"), matrix[2][1]);
	m_pConfig->Write(TEXT("dMatrix_R_R"), matrix[2][2]);
	m_pConfig->Write(TEXT("dMatrix_R_SL"), matrix[2][3]);
	m_pConfig->Write(TEXT("dMatrix_R_SR"), matrix[2][4]);
	m_pConfig->Write(TEXT("dMatrix_R_LFE"), matrix[2][5]);

	m_pConfig->Write(TEXT("dMatrix_SL_L"), matrix[3][0]);
	m_pConfig->Write(TEXT("dMatrix_SL_C"), matrix[3][1]);
	m_pConfig->Write(TEXT("dMatrix_SL_R"), matrix[3][2]);
	m_pConfig->Write(TEXT("dMatrix_SL_SL"), matrix[3][3]);
	m_pConfig->Write(TEXT("dMatrix_SL_SR"), matrix[3][4]);
	m_pConfig->Write(TEXT("dMatrix_SL_LFE"), matrix[3][5]);

	m_pConfig->Write(TEXT("dMatrixSR_L"), matrix[4][0]);
	m_pConfig->Write(TEXT("dMatrixSR_C"), matrix[4][1]);
	m_pConfig->Write(TEXT("dMatrixSR_R"), matrix[4][2]);
	m_pConfig->Write(TEXT("dMatrixSR_SL"), matrix[4][3]);
	m_pConfig->Write(TEXT("dMatrixSR_SR"), matrix[4][4]);
	m_pConfig->Write(TEXT("dMatrixSR_LFE"), matrix[4][5]);

	m_pConfig->Write(TEXT("dMatrixLFE_L"), matrix[5][0]);
	m_pConfig->Write(TEXT("dMatrixLFE_C"), matrix[5][1]);
	m_pConfig->Write(TEXT("dMatrixLFE_R"), matrix[5][2]);
	m_pConfig->Write(TEXT("dMatrixLFE_SL"), matrix[5][3]);
	m_pConfig->Write(TEXT("dMatrixLFE_SR"), matrix[5][4]);
	m_pConfig->Write(TEXT("dMatrixLFE_LFE"), matrix[5][5]);

#ifdef USE_SPDIF
	// SPDIF
	m_pConfig->Write(TEXT("iSpdifDtsMode"), m_dvd_graph.get_dts_mode());
	m_pConfig->Write(TEXT("iSpdifDtsConv"), m_dvd_graph.get_dts_conv());
	m_pConfig->Write(TEXT("bSpdifUseDetector"), (int)m_dvd_graph.get_use_detector());
	m_pConfig->Write(TEXT("bSpdifEncode"), (int)m_dvd_graph.get_spdif_encode());
	m_pConfig->Write(TEXT("bSpdifStereoPT"), (int)m_dvd_graph.get_spdif_stereo_pt());
	m_pConfig->Write(TEXT("iSpdifBitrate"), m_dvd_graph.get_spdif_bitrate());
	m_pConfig->Write(TEXT("bSpdifAsPcm"), (int)m_dvd_graph.get_spdif_as_pcm());
	m_pConfig->Write(TEXT("bSpdifCheckSR"), (int)m_dvd_graph.get_spdif_check_sr());
	m_pConfig->Write(TEXT("bSpdifAllow48"), (int)m_dvd_graph.get_spdif_allow_48());
	m_pConfig->Write(TEXT("bSpdifAllow44"), (int)m_dvd_graph.get_spdif_allow_44());
	m_pConfig->Write(TEXT("bSpdifAllow32"), (int)m_dvd_graph.get_spdif_allow_32());
	m_pConfig->Write(TEXT("bSpdifQuerySink"), (int)m_dvd_graph.get_query_sink());
	m_pConfig->Write(TEXT("bSpdifCloseAtEnd"), (int)m_spdif_close_at_end);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//  Dialogs (Config + about)
////////////////////////////////////////////////////////////////////////////////
void outMixer::Config(HWND hwndParent)
{
	TabDlg *dlg = new TabDlg(hwndParent);
	if (dlg)
	{
		dlg->add_page(0, ConfigDlg::create_main(this), TEXT("Options"));
		dlg->add_page(1, ConfigDlg::create_mixer(this), TEXT("Mixer"));
		dlg->add_page(2, ConfigDlg::create_gains(this), TEXT("Gains"));
#ifdef USE_SPDIF
		dlg->add_page(3, ConfigDlg::create_spdif(this), TEXT("SPDIF"));
#endif
		//dlg.add_page(4, ConfigDlg::create_about(this), TEXT("About"));
		// TODO reload with the previous selection
		int page = 0;
		m_pConfig->Read(TEXT("iLastPrefs"), &page, 0);
		dlg->switch_to(page);
		dlg->exec();
		//m_pOut->Config(hwndParent);
	}
}

////////////////////////////////////////////////////////////////////////////////
//  Open
////////////////////////////////////////////////////////////////////////////////
int outMixer::Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms)
{
	// Récupération du chemin complet du plugin actuel
	TCHAR szFullpath[MAX_PATH] = { 0 }, *pszFullpath = 0;
	if (g_pModSlave && g_pModSlave->hDllInstance)
	{
		GetModuleFileName(g_pModSlave->hDllInstance, szFullpath, MAX_PATH - 6 - 1);
		pszFullpath = (LPWSTR)FindPathFileName(szFullpath);
	}

	// Changement de plugin si ce n'est pas le bon
	if (*m_out_plugin == '\0' && pszFullpath)
	{
		CopyCchStr(m_out_plugin, ARRAYSIZE(m_out_plugin), pszFullpath);
	}
	else
	{
		if (pszFullpath && _tcscmp(m_out_plugin, pszFullpath))
			if (!switchOutPutPlugin(m_out_plugin))
				switchOutPutPlugin(pszFullpath);
	}

	// init
	m_outputchanged = false;
#ifdef USE_SPDIF
	int out_format = m_dvd_graph.get_output().format;
	if (m_use_spdif && (out_format == FORMAT_SPDIF || out_format == FORMAT_UNKNOWN))
	{
		m_output_mode = OUTPUT_MODE_SPDIF;
		m_pOut = m_pOutDsound;
	}
	else
#endif
	{
		m_output_mode = OUTPUT_MODE_NORMAL;
		m_pOut = m_pOutPlugin;
	}

	// Réglages input/output
	m_in_spk = winamp2spk(samplerate, numchannels, bitspersamp);
	if (m_user_sample_rate == 0)
		m_out_spk.sample_rate = samplerate;

	if (m_out_spk.original_mask == 0)
		m_out_spk.original_mask = m_in_spk.original_mask;

	// Paramétrage de valib
	m_dvd_graph.reset();
	m_dvd_graph.proc.set_input_order(win_order);
	m_dvd_graph.proc.set_output_order(win_order);
	m_dvd_graph.set_user(m_out_spk);
	m_dvd_graph.set_input(m_in_spk);
#ifdef USE_SPDIF
	m_dvd_graph.set_use_spdif(m_use_spdif);
#endif

	return (m_pOut ? m_pOut->Open(m_out_spk, bufferlenms, prebufferms) : -1);
}

////////////////////////////////////////////////////////////////////////////////
//  Close
////////////////////////////////////////////////////////////////////////////////
void outMixer::Close(void)
{
	if (m_pOut)
	{
		m_pOut->Close();
	}

#ifdef USE_SPDIF
	if (m_use_spdif && m_spdif_close_at_end)
		m_pOutDsound->Reset();
#endif
}

////////////////////////////////////////////////////////////////////////////////
//  Write
////////////////////////////////////////////////////////////////////////////////
int outMixer::Write(const char *buf, const int len)
{
	int res = 0;

	// Chgt de mode de lecture s'il a changé
	if (m_outputchanged)
	{
		const int written = this->GetOutputTime();
		this->Flush(0);
		this->Close();
		this->Open(m_in_spk.sample_rate, spk2nch(m_in_spk), spk2bps(m_in_spk), 0, 0);
		this->Flush(written);
		m_outputchanged = false;
	}

	// traitement du signal par valib
	m_chunk.set_rawdata(m_in_spk, (uint8_t*)buf, len);
	if (!m_chunk.is_dummy() && !m_chunk.is_empty())
		m_dvd_graph.process(&m_chunk);

	// lecture du signal modifié
	while (!m_dvd_graph.is_empty())
	{
		m_dvd_graph.get_chunk(&m_chunk);
		if (!m_chunk.is_dummy() && !m_chunk.is_empty())
			res = m_pOut->Write((char*)m_chunk.rawdata, (int)m_chunk.size);
	}

	// On force la réouverture si le mode ne correspond pas
	int out_format = m_dvd_graph.get_output().format;
	if (out_format > 0 &&
		(m_output_mode == OUTPUT_MODE_NORMAL && out_format == FORMAT_SPDIF ||
		m_output_mode == OUTPUT_MODE_SPDIF && out_format != FORMAT_SPDIF))
			m_outputchanged = true;

	return res;
}

////////////////////////////////////////////////////////////////////////////////
//  
////////////////////////////////////////////////////////////////////////////////

int outMixer::CanWrite(void)
{
	return (m_pOut ? m_pOut->CanWrite() : 0);
}

int outMixer::IsPlaying(void)
{
	return (m_pOut ? m_pOut->IsPlaying() : 0);
}

int outMixer::Pause(const int pause)
{
	return (m_pOut ? m_pOut->Pause(pause) : 0);
}

void outMixer::SetVolume(const int volume)
{
	if (m_pOut)
	{
		m_pOut->SetVolume(volume);
	}
}

void outMixer::SetPan(const int pan)
{
	if (m_pOut)
	{
		m_pOut->SetPan(pan);
	}
}

void outMixer::Flush(const int t)
{
	if (m_pOut)
	{
		m_pOut->Flush(t);
	}
}

int outMixer::GetOutputTime(void)
{
	return (m_pOut ? m_pOut->GetOutputTime() : 0);
}

int outMixer::GetWrittenTime(void)
{
	return (m_pOut ? m_pOut->GetWrittenTime() : 0);
}

////////////////////////////////////////////////////////////////////////////////
//  ChangeOutput
////////////////////////////////////////////////////////////////////////////////
#ifdef USE_SPDIF
void outMixer::ChangeOutput(const int ispk, const int ifmt, const int rate, const bool use_spdif)
#else
void outMixer::ChangeOutput(const int ispk, const int ifmt, const int rate)
#endif
{
	int format = FORMAT_PCM16;
	int mask = MODE_STEREO;
	int relation = NO_RELATION;

	switch (ispk)
	{
		case 0: mask = m_in_spk.mask; break;
		case 1: mask = MODE_1_0; break;
		case 2: mask = MODE_2_0; break;
		case 3: mask = MODE_3_0; break;
		case 4: mask = MODE_2_2; break;
		case 5: mask = MODE_3_1 | CH_MASK_LFE; break;
		case 6: mask = MODE_3_2 | CH_MASK_LFE; break;
		case 7: mask = MODE_STEREO; relation = RELATION_DOLBY; break;
		case 8: mask = MODE_STEREO; relation = RELATION_DOLBY2; break;
	}

	switch (ifmt)
	{
		case 0: format = FORMAT_PCM16; break;
		case 1: format = FORMAT_PCM24; break;
		case 2: format = FORMAT_PCM32; break;
	}

	switch (rate)
	{
		case 0: m_user_sample_rate = 0; break;
		case 1: m_user_sample_rate = 8000; break;
		case 2: m_user_sample_rate = 11025; break;
		case 3: m_user_sample_rate = 22050; break;
		case 4: m_user_sample_rate = 24000; break;
		case 5: m_user_sample_rate = 32000; break;
		case 6: m_user_sample_rate = 44100; break;
		case 7: m_user_sample_rate = 48000; break;
		case 8: m_user_sample_rate = 96000; break;
		case 9: m_user_sample_rate = 192000; break;
	}

	m_out_spk = Speakers(format, ispk ? mask : 0, m_user_sample_rate ?
						 m_user_sample_rate : m_in_spk.sample_rate, -1, relation);
	m_outputchanged = true;
#ifdef USE_SPDIF
	m_use_spdif = use_spdif;
#endif
}

////////////////////////////////////////////////////////////////////////////////
//  set_OutputPlugin
////////////////////////////////////////////////////////////////////////////////
void outMixer::set_OutputPlugin(const TCHAR *path)
{	
	CopyCchStr(m_out_plugin, ARRAYSIZE(m_out_plugin), path);
	m_pConfig->Write(TEXT("sOutputPlugin"), m_out_plugin);
}

const TCHAR* outMixer::get_OutputPlugin(void)
{
	if (!m_out_plugin[0])
	{
		m_pConfig->Read(TEXT("sOutputPlugin"), m_out_plugin, TEXT("out_notsodirect.dll"));
	}
	return m_out_plugin;
}