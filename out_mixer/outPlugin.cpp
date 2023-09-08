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
	if (g_pModSlave && g_pModSlave->Config)
	{
	g_pModSlave->Config(hwndParent);
}
}

void outPlugin::About(HWND hwndParent)
{
	if (g_pModSlave && g_pModSlave->About)
	{
	g_pModSlave->About(hwndParent);
}
}

int outPlugin::Open(Speakers spk, const int bufferlenms, const int prebufferms)
{
	return (g_pModSlave && g_pModSlave->Open ? g_pModSlave->Open(spk.sample_rate,
						 spk2nch(spk), spk2bps(spk), bufferlenms, prebufferms) : -1);
}

void outPlugin::Close(void)
{
	if (g_pModSlave && g_pModSlave->Close)
{
	g_pModSlave->Close();
}
}

int outPlugin::Write(const char *buf, const int len)
{
	return (g_pModSlave ? g_pModSlave->Write(buf, len) : 0);
}

int outPlugin::CanWrite(void)
{
	return (g_pModSlave ? g_pModSlave->CanWrite() : 0);
}

int outPlugin::IsPlaying(void)
{
	return (g_pModSlave ? g_pModSlave->IsPlaying() : 0);
}

int outPlugin::Pause(const int pause)
{
	return (g_pModSlave ? g_pModSlave->Pause(pause) : 0);
}

void outPlugin::SetVolume(const int volume)
{
	if (g_pModSlave)
{
	g_pModSlave->SetVolume(volume);
}
}

void outPlugin::SetPan(const int pan)
{
	if (g_pModSlave)
{
	g_pModSlave->SetPan(pan);
}
}

void outPlugin::Flush(const int t)
{
	if (g_pModSlave && g_pModSlave->Flush)
{
	g_pModSlave->Flush(t);
}
}

int outPlugin::GetOutputTime(void)
{
	return (g_pModSlave ? g_pModSlave->GetOutputTime() : 0);
}

int outPlugin::GetWrittenTime(void)
{
	return (g_pModSlave ? g_pModSlave->GetWrittenTime() : 0);
}