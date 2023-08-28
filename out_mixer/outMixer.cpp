#include "outMixer.h"
#include "config/config.h"

extern HINSTANCE g_hMasterInstance;
extern Out_Module *g_pModSlave;
extern bool switchOutPutPlugin(const char* path);

outMixer::outMixer(void)
{
	*m_out_plugin = '\0';
	m_pOutPlugin = new outPlugin();
	m_pOutDsound = new outDsound();

	m_pConfig = new DevilConfig( g_hMasterInstance, "Winamp" );
	ReadConfig();

	m_pOut = m_use_spdif
			? (IOut*)m_pOutDsound
			: (IOut*)m_pOutPlugin;
}

outMixer::~outMixer(void)
{
	WriteConfig();
	delete m_pOutPlugin;
	delete m_pOutDsound;
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
	float    delays[NCHANNELS];
	matrix_t matrix;

	// Output
	m_pConfig->Read("iOutputMode",		&m_out_spk.mask, MODE_2_0);
	m_pConfig->Read("iOutputRelation",	&m_out_spk.relation, NO_RELATION);
	m_pConfig->Read("iOutputFormat",	&m_out_spk.format, FORMAT_PCM16);
	m_pConfig->Read("iOutputRate",		&m_out_spk.sample_rate, 44100);
	m_pConfig->Read("bUseSpdif",		&iRes, 0);		m_use_spdif = (iRes==1);
	m_user_sample_rate = m_out_spk.sample_rate;
	m_out_spk.set(m_out_spk.format, m_out_spk.mask, m_out_spk.sample_rate, -1, m_out_spk.relation); // to set level correctly

	// Interface
	m_pConfig->Read("bInvertLevels",	&iRes, 0);		m_invert_levels = (iRes==1);
	m_pConfig->Read("iRefreshTime",		&iRes, 100);	m_refresh_time = iRes;

	// AGC options
	m_pConfig->Read("bAutoGain",		&iRes, 1);		m_dvd_graph.proc.set_auto_gain(iRes==1);
	m_pConfig->Read("bNormalize",		&iRes, 0);		m_dvd_graph.proc.set_normalize(iRes==1);
	m_pConfig->Read("dAttack",			&dRes, 100);	m_dvd_graph.proc.set_attack(dRes);
	m_pConfig->Read("dRelease",			&dRes, 50);		m_dvd_graph.proc.set_release(dRes);

	// DRC
	m_pConfig->Read("bDRC",				&iRes, 0);	m_dvd_graph.proc.set_drc(iRes==1);
	m_pConfig->Read("dDRCPower",		&dRes, 0);	m_dvd_graph.proc.set_drc_power(dRes);

	// Gain
	m_pConfig->Read("dGainMaster",		&dRes, 1);	m_dvd_graph.proc.set_master(dRes);
	m_pConfig->Read("dGainVoice",		&dRes, 1);	m_dvd_graph.proc.set_clev(dRes);
	m_pConfig->Read("dGainSurround",	&dRes, 1);	m_dvd_graph.proc.set_slev(dRes);
	m_pConfig->Read("dGainLFE",			&dRes, 1);	m_dvd_graph.proc.set_lfelev(dRes);

	// Input/output gains
	m_pConfig->Read("dGainInputL",		&dRes, 1);	input_gains[CH_L] = dRes;
	m_pConfig->Read("dGainInputC",		&dRes, 1);	input_gains[CH_C] = dRes;
	m_pConfig->Read("dGainInputR",		&dRes, 1);	input_gains[CH_R] = dRes;
	m_pConfig->Read("dGainInputSL",		&dRes, 1);	input_gains[CH_SL] = dRes;
	m_pConfig->Read("dGainInputSR",		&dRes, 1);	input_gains[CH_SR] = dRes;
	m_pConfig->Read("dGainInputLFE",	&dRes, 1);	input_gains[CH_LFE] = dRes;
	m_pConfig->Read("dGainOutputL",		&dRes, 1);	output_gains[CH_L] = dRes;
	m_pConfig->Read("dGainOutputC",		&dRes, 1);	output_gains[CH_C] = dRes;
	m_pConfig->Read("dGainOutputR",		&dRes, 1);	output_gains[CH_R] = dRes;
	m_pConfig->Read("dGainOutputSL",	&dRes, 1);	output_gains[CH_SL] = dRes;
	m_pConfig->Read("dGainOutputSR",	&dRes, 1);	output_gains[CH_SR] = dRes;
	m_pConfig->Read("dGainOutputLFE",	&dRes, 1);	output_gains[CH_LFE] = dRes;

	// Delay
	m_pConfig->Read("bDelay",			&iRes, 0);	m_dvd_graph.proc.set_delay(iRes==1);
	m_pConfig->Read("dDelayL",			&dRes, 0);	delays[CH_L] = (float)dRes;
	m_pConfig->Read("dDelayC",			&dRes, 0);	delays[CH_C] = (float)dRes;
	m_pConfig->Read("dDelayR",			&dRes, 0);	delays[CH_R] = (float)dRes;
	m_pConfig->Read("dDelaySL",			&dRes, 0);	delays[CH_SL] = (float)dRes;
	m_pConfig->Read("dDelaySR",			&dRes, 0);	delays[CH_SR] = (float)dRes;
	m_pConfig->Read("dDelayLFE",		&dRes, 0);	delays[CH_LFE] = (float)dRes;
	m_pConfig->Read("iDelayUnits",		&iRes, 0);	m_dvd_graph.proc.set_delay_units(iRes);

	// Matrix options
	m_pConfig->Read("bAutoMatrix",		&iRes, 1);	m_dvd_graph.proc.set_auto_matrix(iRes==1);
	m_pConfig->Read("bNormalizeMatrix",	&iRes, 1);	m_dvd_graph.proc.set_normalize_matrix(iRes==1);
	m_pConfig->Read("bVoiceControl",	&iRes, 1);	m_dvd_graph.proc.set_voice_control(iRes==1);
	m_pConfig->Read("bExpandStereo",	&iRes, 1);	m_dvd_graph.proc.set_expand_stereo(iRes==1);

	// Bass redirection
	m_pConfig->Read("bBassRedirection",	&iRes, 0);	m_dvd_graph.proc.set_bass_redir(iRes==1);
	m_pConfig->Read("iBassFrequency",	&iRes, 0);	m_dvd_graph.proc.set_bass_freq(iRes);

	// Matrix
	m_pConfig->Read("dMatrix_L_L",		&dRes, 0);	matrix[0][0] = dRes;
	m_pConfig->Read("dMatrix_L_C",		&dRes, 0);	matrix[0][1] = dRes;
	m_pConfig->Read("dMatrix_L_R",		&dRes, 0);	matrix[0][2] = dRes;
	m_pConfig->Read("dMatrix_L_SL",		&dRes, 0);	matrix[0][3] = dRes;
	m_pConfig->Read("dMatrix_L_SR",		&dRes, 0);	matrix[0][4] = dRes;
	m_pConfig->Read("dMatrix_L_LFE",	&dRes, 0);	matrix[0][5] = dRes;

	m_pConfig->Read("dMatrix_C_L",		&dRes, 0);	matrix[1][0] = dRes;
	m_pConfig->Read("dMatrix_C_C",		&dRes, 0);	matrix[1][1] = dRes;
	m_pConfig->Read("dMatrix_C_R",		&dRes, 0);	matrix[1][2] = dRes;
	m_pConfig->Read("dMatrix_C_SL",		&dRes, 0);	matrix[1][3] = dRes;
	m_pConfig->Read("dMatrix_C_SR",		&dRes, 0);	matrix[1][4] = dRes;
	m_pConfig->Read("dMatrix_C_LFE",	&dRes, 0);	matrix[1][5] = dRes;

	m_pConfig->Read("dMatrix_R_L",		&dRes, 0);	matrix[2][0] = dRes;
	m_pConfig->Read("dMatrix_R_C",		&dRes, 0);	matrix[2][1] = dRes;
	m_pConfig->Read("dMatrix_R_R",		&dRes, 0);	matrix[2][2] = dRes;
	m_pConfig->Read("dMatrix_R_SL",		&dRes, 0);	matrix[2][3] = dRes;
	m_pConfig->Read("dMatrix_R_SR",		&dRes, 0);	matrix[2][4] = dRes;
	m_pConfig->Read("dMatrix_R_LFE",	&dRes, 0);	matrix[2][5] = dRes;

	m_pConfig->Read("dMatrix_SL_L",		&dRes, 0);	matrix[3][0] = dRes;
	m_pConfig->Read("dMatrix_SL_C",		&dRes, 0);	matrix[3][1] = dRes;
	m_pConfig->Read("dMatrix_SL_R",		&dRes, 0);	matrix[3][2] = dRes;
	m_pConfig->Read("dMatrix_SL_SL",	&dRes, 0);	matrix[3][3] = dRes;
	m_pConfig->Read("dMatrix_SL_SR",	&dRes, 0);	matrix[3][4] = dRes;
	m_pConfig->Read("dMatrix_SL_LFE",	&dRes, 0);	matrix[3][5] = dRes;

	m_pConfig->Read("dMatrixSR_L",		&dRes, 0);	matrix[4][0] = dRes;
	m_pConfig->Read("dMatrixSR_C",		&dRes, 0);	matrix[4][1] = dRes;
	m_pConfig->Read("dMatrixSR_R",		&dRes, 0);	matrix[4][2] = dRes;
	m_pConfig->Read("dMatrixSR_SL",		&dRes, 0);	matrix[4][3] = dRes;
	m_pConfig->Read("dMatrixSR_SR",		&dRes, 0);	matrix[4][4] = dRes;
	m_pConfig->Read("dMatrixSR_LFE",	&dRes, 0);	matrix[4][5] = dRes;

	m_pConfig->Read("dMatrixLFE_L",		&dRes, 0);	matrix[5][0] = dRes;
	m_pConfig->Read("dMatrixLFE_C",		&dRes, 0);	matrix[5][1] = dRes;
	m_pConfig->Read("dMatrixLFE_R",		&dRes, 0);	matrix[5][2] = dRes;
	m_pConfig->Read("dMatrixLFE_SL",	&dRes, 0);	matrix[5][3] = dRes;
	m_pConfig->Read("dMatrixLFE_SR",	&dRes, 0);	matrix[5][4] = dRes;
	m_pConfig->Read("dMatrixLFE_LFE",	&dRes, 0);	matrix[5][5] = dRes;

	m_dvd_graph.proc.set_input_gains(input_gains);
	m_dvd_graph.proc.set_output_gains(output_gains);
	m_dvd_graph.proc.set_delays(delays);
	m_dvd_graph.proc.set_matrix(matrix);

	// SPDIF
	m_pConfig->Read("iSpdifDtsMode",		&iRes, 0);	m_dvd_graph.set_dts_mode(iRes);
	m_pConfig->Read("iSpdifDtsConv",		&iRes, 0);	m_dvd_graph.set_dts_conv(iRes);
	m_pConfig->Read("bSpdifUseDetector",	&iRes, 0);	m_dvd_graph.set_use_detector(iRes==1);
	m_pConfig->Read("bSpdifEncode",			&iRes, 1);	m_dvd_graph.set_spdif_encode(iRes==1);
	m_pConfig->Read("bSpdifStereoPT",		&iRes, 1);	m_dvd_graph.set_spdif_stereo_pt(iRes==1);
	m_pConfig->Read("iSpdifBitrate",		&iRes, 640000);	m_dvd_graph.set_spdif_bitrate(iRes);
	m_pConfig->Read("bSpdifAsPcm",			&iRes, 0);	m_dvd_graph.set_spdif_as_pcm(iRes==1);
	m_pConfig->Read("bSpdifCheckSR",		&iRes, 1);	m_dvd_graph.set_spdif_check_sr(iRes==1);
	m_pConfig->Read("bSpdifAllow48",		&iRes, 1);	m_dvd_graph.set_spdif_allow_48(iRes==1);
	m_pConfig->Read("bSpdifAllow44",		&iRes, 0);	m_dvd_graph.set_spdif_allow_44(iRes==1);
	m_pConfig->Read("bSpdifAllow32",		&iRes, 0);	m_dvd_graph.set_spdif_allow_32(iRes==1);
	m_pConfig->Read("bSpdifQuerySink",		&iRes, 1);	m_dvd_graph.set_query_sink(iRes==1);
	m_pConfig->Read("bSpdifCloseAtEnd",		&iRes, 1);	m_spdif_close_at_end = (iRes==1);
}


////////////////////////////////////////////////////////////////////////////////
//  WriteConfig
////////////////////////////////////////////////////////////////////////////////
void outMixer::WriteConfig()
{
	if( !m_pConfig ) return;

	sample_t input_gains[NCHANNELS];
	sample_t output_gains[NCHANNELS];
	float    delays[NCHANNELS];
	matrix_t matrix;

	m_dvd_graph.proc.get_input_gains(input_gains);
	m_dvd_graph.proc.get_output_gains(output_gains);
	m_dvd_graph.proc.get_delays(delays);
	m_dvd_graph.proc.get_matrix(matrix);

	// Output
	m_pConfig->Write("iOutputMode",		m_out_spk.mask);
	m_pConfig->Write("iOutputRelation",	m_out_spk.relation);
	m_pConfig->Write("iOutputFormat",	m_out_spk.format);
	m_pConfig->Write("iOutputRate",		m_user_sample_rate);
	m_pConfig->Write("bUseSpdif",		m_use_spdif);

	// Interface
	m_pConfig->Write("bInvertLevels",	(int)m_invert_levels);
	m_pConfig->Write("iRefreshTime",	m_refresh_time);

	// AGC options
	m_pConfig->Write("bAutoGain",		(int)m_dvd_graph.proc.get_auto_gain());
	m_pConfig->Write("bNormalize",		m_dvd_graph.proc.get_normalize());
	m_pConfig->Write("dAttack",			m_dvd_graph.proc.get_attack());
	m_pConfig->Write("dRelease",		m_dvd_graph.proc.get_release());

	// DRC
	m_pConfig->Write("bDRC",			(int)m_dvd_graph.proc.get_drc());
	m_pConfig->Write("dDRCPower",		m_dvd_graph.proc.get_drc_power());

	// Gain
	m_pConfig->Write("dGainMaster",		m_dvd_graph.proc.get_master());
	m_pConfig->Write("dGainVoice",		m_dvd_graph.proc.get_clev());
	m_pConfig->Write("dGainSurround",	m_dvd_graph.proc.get_slev());
	m_pConfig->Write("dGainLFE",		m_dvd_graph.proc.get_lfelev());

	// Input/output gains
	m_pConfig->Write("dGainInputL",		input_gains[CH_L]);
	m_pConfig->Write("dGainInputC",		input_gains[CH_C]);
	m_pConfig->Write("dGainInputR",		input_gains[CH_R]);
	m_pConfig->Write("dGainInputSL",	input_gains[CH_SL]);
	m_pConfig->Write("dGainInputSR",	input_gains[CH_SR]);
	m_pConfig->Write("dGainInputLFE",	input_gains[CH_LFE]);
	m_pConfig->Write("dGainOutputL",	output_gains[CH_L]);
	m_pConfig->Write("dGainOutputC",	output_gains[CH_C]);
	m_pConfig->Write("dGainOutputR",	output_gains[CH_R]);
	m_pConfig->Write("dGainOutputSL",	output_gains[CH_SL]);
	m_pConfig->Write("dGainOutputSR",	output_gains[CH_SR]);
	m_pConfig->Write("dGainOutputLFE",	output_gains[CH_LFE]);

	// Delay
	m_pConfig->Write("bDelay",			(int)m_dvd_graph.proc.get_delay());
    m_pConfig->Write("dDelayL",			delays[CH_L]);
    m_pConfig->Write("dDelayC",			delays[CH_C]);
    m_pConfig->Write("dDelayR",			delays[CH_R]);
    m_pConfig->Write("dDelaySL",		delays[CH_SL]);
    m_pConfig->Write("dDelaySR",		delays[CH_SR]);
    m_pConfig->Write("dDelayLFE",		delays[CH_LFE]);
	m_pConfig->Write("iDelayUnits",		m_dvd_graph.proc.get_delay_units());

	// Matrix options
	m_pConfig->Write("bAutoMatrix",		(int)m_dvd_graph.proc.get_auto_matrix());
	m_pConfig->Write("bNormalizeMatrix",(int)m_dvd_graph.proc.get_normalize_matrix());
	m_pConfig->Write("bVoiceControl",	(int)m_dvd_graph.proc.get_voice_control());
	m_pConfig->Write("bExpandStereo",	(int)m_dvd_graph.proc.get_expand_stereo());

	// Bass redirection
	m_pConfig->Write("bBassRedirection",(int)m_dvd_graph.proc.get_bass_redir());
	m_pConfig->Write("iBassFrequency",	m_dvd_graph.proc.get_bass_freq());

	// Matrix
	m_pConfig->Write("dMatrix_L_L",		matrix[0][0]);
	m_pConfig->Write("dMatrix_L_C",		matrix[0][1]);
	m_pConfig->Write("dMatrix_L_R",		matrix[0][2]);
	m_pConfig->Write("dMatrix_L_SL",	matrix[0][3]);
	m_pConfig->Write("dMatrix_L_SR",	matrix[0][4]);
	m_pConfig->Write("dMatrix_L_LFE",	matrix[0][5]);

	m_pConfig->Write("dMatrix_C_L",		matrix[1][0]);
	m_pConfig->Write("dMatrix_C_C",		matrix[1][1]);
	m_pConfig->Write("dMatrix_C_R",		matrix[1][2]);
	m_pConfig->Write("dMatrix_C_SL",	matrix[1][3]);
	m_pConfig->Write("dMatrix_C_SR",	matrix[1][4]);
	m_pConfig->Write("dMatrix_C_LFE",	matrix[1][5]);

	m_pConfig->Write("dMatrix_R_L",		matrix[2][0]);
	m_pConfig->Write("dMatrix_R_C",		matrix[2][1]);
	m_pConfig->Write("dMatrix_R_R",		matrix[2][2]);
	m_pConfig->Write("dMatrix_R_SL",	matrix[2][3]);
	m_pConfig->Write("dMatrix_R_SR",	matrix[2][4]);
	m_pConfig->Write("dMatrix_R_LFE",	matrix[2][5]);

	m_pConfig->Write("dMatrix_SL_L",	matrix[3][0]);
	m_pConfig->Write("dMatrix_SL_C",	matrix[3][1]);
	m_pConfig->Write("dMatrix_SL_R",	matrix[3][2]);
	m_pConfig->Write("dMatrix_SL_SL",	matrix[3][3]);
	m_pConfig->Write("dMatrix_SL_SR",	matrix[3][4]);
	m_pConfig->Write("dMatrix_SL_LFE",	matrix[3][5]);

	m_pConfig->Write("dMatrixSR_L",		matrix[4][0]);
	m_pConfig->Write("dMatrixSR_C",		matrix[4][1]);
	m_pConfig->Write("dMatrixSR_R",		matrix[4][2]);
	m_pConfig->Write("dMatrixSR_SL",	matrix[4][3]);
	m_pConfig->Write("dMatrixSR_SR",	matrix[4][4]);
	m_pConfig->Write("dMatrixSR_LFE",	matrix[4][5]);

	m_pConfig->Write("dMatrixLFE_L",	matrix[5][0]);
	m_pConfig->Write("dMatrixLFE_C",	matrix[5][1]);
	m_pConfig->Write("dMatrixLFE_R",	matrix[5][2]);
	m_pConfig->Write("dMatrixLFE_SL",	matrix[5][3]);
	m_pConfig->Write("dMatrixLFE_SR",	matrix[5][4]);
	m_pConfig->Write("dMatrixLFE_LFE",	matrix[5][5]);

	// SPDIF
	m_pConfig->Write("iSpdifDtsMode",		m_dvd_graph.get_dts_mode());
	m_pConfig->Write("iSpdifDtsConv",		m_dvd_graph.get_dts_conv());
	m_pConfig->Write("bSpdifUseDetector",	(int)m_dvd_graph.get_use_detector());
	m_pConfig->Write("bSpdifEncode",		(int)m_dvd_graph.get_spdif_encode());
	m_pConfig->Write("bSpdifStereoPT",		(int)m_dvd_graph.get_spdif_stereo_pt());
	m_pConfig->Write("iSpdifBitrate",		m_dvd_graph.get_spdif_bitrate());
	m_pConfig->Write("bSpdifAsPcm",			(int)m_dvd_graph.get_spdif_as_pcm());
	m_pConfig->Write("bSpdifCheckSR",		(int)m_dvd_graph.get_spdif_check_sr());
	m_pConfig->Write("bSpdifAllow48",		(int)m_dvd_graph.get_spdif_allow_48());
	m_pConfig->Write("bSpdifAllow44",		(int)m_dvd_graph.get_spdif_allow_44());
	m_pConfig->Write("bSpdifAllow32",		(int)m_dvd_graph.get_spdif_allow_32());
	m_pConfig->Write("bSpdifQuerySink",		(int)m_dvd_graph.get_query_sink());
	m_pConfig->Write("bSpdifCloseAtEnd",	(int)m_spdif_close_at_end);
}

////////////////////////////////////////////////////////////////////////////////
//  Dialogs (Config + about)
////////////////////////////////////////////////////////////////////////////////
void outMixer::Config(HWND hwndParent)
{
	TabDlg dlg(g_hMasterInstance, MAKEINTRESOURCE(IDD_TABDLG), hwndParent);
	dlg.add_page(0, ConfigDlg::create_main(g_hMasterInstance, this), "Main");
	dlg.add_page(1, ConfigDlg::create_mixer(g_hMasterInstance, this), "Mixer");
	dlg.add_page(2, ConfigDlg::create_gains(g_hMasterInstance, this), "Gains");
	dlg.add_page(3, ConfigDlg::create_spdif(g_hMasterInstance, this), "SPDIF");
	dlg.add_page(4, ConfigDlg::create_about(g_hMasterInstance, this), "About");
	dlg.exec("Matrix Mixer configuration");
	//m_pOut->Config(hwndParent);
}

void outMixer::About(HWND hwndParent)
{
	TabDlg dlg(g_hMasterInstance, MAKEINTRESOURCE(IDD_TABDLG), hwndParent);
	dlg.add_page(0, ConfigDlg::create_about(g_hMasterInstance, this), "About");
	dlg.exec("Matrix Mixer configuration");
	//m_pOut->About(hwndParent);
}

////////////////////////////////////////////////////////////////////////////////
//  Open
////////////////////////////////////////////////////////////////////////////////
int outMixer::Open(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms)
{
	// Récupération du chemin complet du plugin actuel
	char szFullpath[ MAX_PATH ] = "";
	GetModuleFileName( g_pModSlave->hDllInstance, szFullpath, MAX_PATH - 6 - 1 );
	const int iFullLathLen = strlen( szFullpath );

	// Isolation du nom de la dll seule, sans path
	char * walk = szFullpath + iFullLathLen - 1; // Last char
	while( ( walk > szFullpath ) && ( *walk != '\\' ) ) walk--;
	walk++;

	// Changement de plugin si ce n'est pas le bon
	if (*m_out_plugin == '\0')
		memcpy(m_out_plugin, walk, szFullpath + iFullLathLen - walk + 1);
	else
	{
		if (strcmp(m_out_plugin, walk))
			if (!switchOutPutPlugin(m_out_plugin))
				switchOutPutPlugin(walk);
	}

	// init
	m_outputchanged = false;
	int out_format = m_dvd_graph.get_output().format;
	if (m_use_spdif && (out_format == FORMAT_SPDIF || out_format == FORMAT_UNKNOWN))
	{
		m_output_mode = OUTPUT_MODE_SPDIF;
		m_pOut = m_pOutDsound;
	}
	else
	{
		m_output_mode = OUTPUT_MODE_NORMAL;
		m_pOut = m_pOutPlugin;
	}

	// Réglages input/output
	m_in_spk = winamp2spk(samplerate, numchannels, bitspersamp);
	if (m_user_sample_rate == 0)
		m_out_spk.sample_rate = samplerate;

	// Paramétrage de valib
	m_dvd_graph.reset();
	m_dvd_graph.proc.set_input_order(win_order);
	m_dvd_graph.proc.set_output_order(win_order);
	m_dvd_graph.set_user(m_out_spk);
	m_dvd_graph.set_input(m_in_spk);
	m_dvd_graph.set_use_spdif(m_use_spdif);

	return m_pOut->Open(m_out_spk, bufferlenms, prebufferms);
}

////////////////////////////////////////////////////////////////////////////////
//  Close
////////////////////////////////////////////////////////////////////////////////
void outMixer::Close()
{
	m_pOut->Close();
	if (m_use_spdif && m_spdif_close_at_end)
		m_pOutDsound->Reset();
}

////////////////////////////////////////////////////////////////////////////////
//  Write
////////////////////////////////////////////////////////////////////////////////
int outMixer::Write(char *buf, int len)
{
	int res = 0;
	m_cpu.start();

	// Chgt de mode de lecture s'il a changé
	if (m_outputchanged)
	{
		this->Flush(0);
		this->Close();
		this->Open(m_in_spk.sample_rate, spk2nch(m_in_spk), spk2bps(m_in_spk), 0, 0);
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
			res = m_pOut->Write((char*)m_chunk.rawdata, m_chunk.size);
	}

	// On force la réouverture si le mode ne correspond pas
	int out_format = m_dvd_graph.get_output().format;
	if (out_format > 0 &&
		(m_output_mode == OUTPUT_MODE_NORMAL && out_format == FORMAT_SPDIF ||
		m_output_mode == OUTPUT_MODE_SPDIF && out_format != FORMAT_SPDIF))
			m_outputchanged = true;

	m_cpu.stop();
	return res;
}

////////////////////////////////////////////////////////////////////////////////
//  
////////////////////////////////////////////////////////////////////////////////

int outMixer::CanWrite()
{
	return m_pOut->CanWrite();
}

int outMixer::IsPlaying()
{
	return m_pOut->IsPlaying();
}

int outMixer::Pause(int pause)
{
	return m_pOut->Pause(pause);
}

void outMixer::SetVolume(int volume)
{
	m_pOut->SetVolume(volume);
}

void outMixer::SetPan(int pan)
{
	m_pOut->SetPan(pan);
}

void outMixer::Flush(int t)
{
	m_pOut->Flush(t);
}

int outMixer::GetOutputTime()
{
	return m_pOut->GetOutputTime();
}

int outMixer::GetWrittenTime()
{
	return m_pOut->GetWrittenTime();
}

////////////////////////////////////////////////////////////////////////////////
//  ChangeOutput
////////////////////////////////////////////////////////////////////////////////
void outMixer::ChangeOutput(int ispk, int ifmt, int rate, bool use_spdif)
{
	int format = FORMAT_PCM16;
	int mask = MODE_STEREO;
	int relation = NO_RELATION;

	switch (ispk)
	{
		case  0: mask = m_in_spk.mask; break;
		case  1: mask = MODE_1_0; break;
		case  2: mask = MODE_2_0; break;
		case  3: mask = MODE_3_0; break;
		case  4: mask = MODE_2_2; break;
		case  5: mask = MODE_3_1 | CH_MASK_LFE; break;
		case  6: mask = MODE_3_2 | CH_MASK_LFE; break;

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

	m_out_spk = Speakers(format, ispk ? mask : 0, m_user_sample_rate ? m_user_sample_rate : m_in_spk.sample_rate, -1, relation);
	m_outputchanged = true;
	m_use_spdif = use_spdif;
}
////////////////////////////////////////////////////////////////////////////////
//  set_OutputPlugin
////////////////////////////////////////////////////////////////////////////////
void outMixer::set_OutputPlugin(const char *path)
{	
	memcpy_s(m_out_plugin, MAX_PATH, path, strlen(path)+1);
	m_pConfig->Write("sOutputPlugin", m_out_plugin);
}
