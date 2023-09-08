#include <math.h>
#include "outDsound.h"

outDsound::outDsound(void)
{
	m_dsound.open_dsound(0, true);
	m_volume = 0;
}

outDsound::~outDsound(void)
{
	m_dsound.close_dsound();
}

void outDsound::Config(HWND hwndParent)
{
}

void outDsound::About(HWND hwndParent)
{
}

int outDsound::Open(Speakers spk, const int bufferlenms, const int prebufferms)
{
	if (m_dsound.get_buffer_size() == 0)
		m_dsound.open(Speakers(FORMAT_SPDIF, MODE_STEREO, 48000));
	return 1;
}

void outDsound::Close(void)
{
	m_dsound.stop();
}

void outDsound::Reset(void)
{
	m_dsound.close();
}

int outDsound::Write(const char *buf, const int len)
{
	m_chunk.set_rawdata(m_dsound.get_input(), (uint8_t*)buf, len);
	if (!m_chunk.is_dummy() && !m_chunk.is_empty())
		m_dsound.process(&m_chunk);
	return 0;
}

int outDsound::CanWrite(void)
{
	return (int)(m_dsound.get_buffer_size() - m_dsound.get_data_size());
}

int outDsound::IsPlaying(void)
{
	return 0;
}

int outDsound::Pause(const int pause)
{
	bool was_paused = m_dsound.is_paused();
	if (pause)
		m_dsound.pause();
	else
		m_dsound.unpause();
	return was_paused;
}

void outDsound::SetVolume(const int volume)
{
	//m_volume = volume == -666 ? m_volume : volume;
	//if (m_volume == 0)
	//	m_dsound.set_vol(-100);
	//else
	//	m_dsound.set_vol(20*log10(m_volume/255.0));
}

void outDsound::SetPan(const int pan)
{
	//if (pan == -128)
	//	m_dsound.set_pan(-100);
	//else if (pan == 128)
	//	m_dsound.set_pan(100);
	//else
	//	m_dsound.set_pan( (pan<0?1:-1) * 20*log10(1-abs(pan)/128.0));
}

void outDsound::Flush(const int t)
{
	m_dsound.stop();
}

int outDsound::GetOutputTime(void)
{
	return (int)(1000 * m_dsound.get_playback_time());
}

int outDsound::GetWrittenTime(void)
{
	return (int)(1000 * m_dsound.get_written_time());
}