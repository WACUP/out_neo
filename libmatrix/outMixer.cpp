#include "outMixer.h"
#include "config/config.h"

const int vlc_order[NCHANNELS] = 
{ CH_L, CH_R, CH_SL, CH_SR, CH_C, CH_LFE };

TabDlg		*outMixer::p_config_dlg = NULL;

HMODULE GetCurrentModule()
{
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery( &dummy, &mbi, sizeof( MEMORY_BASIC_INFORMATION ) );
	return ( HMODULE )mbi.AllocationBase;
}

outMixer::outMixer(vlc_fourcc_t i_format, int i_physical_channels, unsigned int i_rate)
{
	int mask = MODE_2_0;
	switch (i_physical_channels)
	{
		case AOUT_CHAN_CENTER: mask = MODE_1_0; break;
		case AOUT_CHANS_2_0:   mask = MODE_2_0; break;
		case AOUT_CHANS_4_0:   mask = MODE_QUADRO; break;
		case AOUT_CHANS_5_0:   mask = MODE_3_2; break;
		case AOUT_CHANS_5_1:   mask = MODE_5_1; break;
	}

	// Réglages input/output
	m_in_spk = Speakers(FORMAT_PCMFLOAT, mask, i_rate);
	m_out_spk = Speakers(FORMAT_PCM16, mask, i_rate);

	m_pConfig = new DevilConfig( GetCurrentModule(), "Fritivi.NET" );
	ReadConfig();
	m_ratio = (float)(m_out_spk.sample_rate * getNbChannels()) / (2*m_in_spk.sample_rate * getInputNbChannels());

	// Paramétrage de valib
	m_dvd_graph.reset();
	m_dvd_graph.proc.set_input_order(vlc_order);
	m_dvd_graph.proc.set_output_order(win_order);
	m_dvd_graph.set_user(m_out_spk);
	m_dvd_graph.set_input(m_in_spk);

	p_empty_buffer = NULL;
	i_empty_size = 0;

	p_config_dlg = NULL;
}

outMixer::~outMixer(void)
{
	if (p_config_dlg)
		p_config_dlg->close();
	if (p_empty_buffer) free(p_empty_buffer);
	delete m_pConfig;
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
	m_pConfig->Read("iOutputMode",		&m_out_spk.mask, 0);
	m_asis = m_out_spk.mask == 0;
	m_out_spk.set(m_out_spk.format,
					m_asis ? m_in_spk.mask : m_out_spk.mask,
					m_out_spk.sample_rate, -1, m_out_spk.relation); // to set level correctly

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
	m_pConfig->Write("iOutputMode",		m_asis ? 0 : m_out_spk.mask);

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
}

////////////////////////////////////////////////////////////////////////////////
//  Dialogs (Config + about)
////////////////////////////////////////////////////////////////////////////////
void outMixer::Config(HWND hwndParent)
{
	if (p_config_dlg)
		return;

	HMODULE hInstance = GetCurrentModule();
	TabDlg dlg(hInstance, MAKEINTRESOURCE(IDD_TABDLG), hwndParent);
	dlg.add_page(0, ConfigDlg::create_main(hInstance, this), "Main");
	dlg.add_page(1, ConfigDlg::create_mixer(hInstance, this), "Mixer");
	dlg.add_page(2, ConfigDlg::create_gains(hInstance, this), "Gains");
	//dlg.add_page(3, ConfigDlg::create_spdif(hInstance, this), "SPDIF");
	dlg.add_page(4, ConfigDlg::create_about(hInstance, this), "About");
	p_config_dlg = &dlg;
	dlg.exec("Matrix Mixer configuration");
	WriteConfig();
	p_config_dlg = NULL;
}
////////////////////////////////////////////////////////////////////////////////
//  Write
////////////////////////////////////////////////////////////////////////////////
void outMixer::Process(uint8_t *p_buffer, size_t i_size)
{
	m_cpu.start();
	if (p_buffer)
	{
		m_chunk.set_rawdata(m_in_spk, p_buffer, i_size);
		i_empty_size = i_size;
	}
	else if (i_empty_size>0)
	{
		if (!p_empty_buffer) //allocate the empty buffer from the size of the first non empty chunk, to be sure to get the right size
		{
			p_empty_buffer = (uint8_t *)malloc(i_empty_size);
			memset(p_empty_buffer, 0, i_empty_size);
		}
		m_chunk.set_rawdata(m_in_spk, p_empty_buffer, i_empty_size);
	}

	if (!m_chunk.is_dummy() && !m_chunk.is_empty())
		m_dvd_graph.process(&m_chunk);
	m_cpu.stop();
}

Chunk *outMixer::getChunk(void)
{
	m_cpu.start();
	Chunk *chunk = NULL;
	if (!m_dvd_graph.is_empty())
	{
		m_dvd_graph.get_chunk(&m_chunk);
		if (!m_chunk.is_dummy() && !m_chunk.is_empty())
			chunk = &m_chunk;
	}	
	m_cpu.stop();
	return chunk;
}

double outMixer::GetOutputTime(void)
{
	return mdate();
}
