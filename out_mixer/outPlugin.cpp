#include "outPlugin.h"

extern Out_Module *g_pModSlave;

outPlugin::outPlugin(void)
{
}

outPlugin::~outPlugin(void)
{
}

void outPlugin::Config(HWND hwndParent)
{
	g_pModSlave->Config(hwndParent);
}

void outPlugin::About(HWND hwndParent)
{
	g_pModSlave->About(hwndParent);
}

int outPlugin::Open(Speakers spk, int bufferlenms, int prebufferms)
{
	return g_pModSlave->Open(spk.sample_rate, spk2nch(spk), spk2bps(spk), bufferlenms, prebufferms);
}

void outPlugin::Close()
{
	g_pModSlave->Close();
}

int outPlugin::Write(char *buf, int len)
{
	return g_pModSlave->Write(buf, len);
}

int outPlugin::CanWrite()
{
	return g_pModSlave->CanWrite();
}

int outPlugin::IsPlaying()
{
	return g_pModSlave->IsPlaying();
}

int outPlugin::Pause(int pause)
{
	return g_pModSlave->Pause(pause);
}

void outPlugin::SetVolume(int volume)
{
	g_pModSlave->SetVolume(volume);
}

void outPlugin::SetPan(int pan)
{
	g_pModSlave->SetPan(pan);
}

void outPlugin::Flush(int t)
{
	g_pModSlave->Flush(t);
}

int outPlugin::GetOutputTime()
{
	return g_pModSlave->GetOutputTime();
}

int outPlugin::GetWrittenTime()
{
	return g_pModSlave->GetWrittenTime();
}
