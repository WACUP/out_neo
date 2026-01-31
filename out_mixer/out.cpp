// This plugin is based on "Outbridge Winamp Plugin" for plugin structure and DevilConfig class (Sebastian Pipping <webmaster@hartwork.org> http://www.hartwork.org)
// This plugin use ac3filter for all audio processing (http://ac3filter.net/)

#include "out.h"
#include "outMixer.h"
#include "config\DevilConfig.h"
#include <loader/loader/paths.h>
#include <loader/loader/utils.h>
#include <nu/servicebuilder.h>
#include <loader/hook/get_api_service.h>
#include <loader/hook/squash.h>
#include "resource.h"
#include "config/tab.h"

// {092E2E9A-A2C0-47b8-9712-D026AE485BEA}
static const GUID OutNotSoNeoLangGUID =
{ 0x92e2e9a, 0xa2c0, 0x47b8, { 0x97, 0x12, 0xd0, 0x26, 0xae, 0x48, 0x5b, 0xea } };

// wasabi based services for localisation support
api_service* WASABI_API_SVC = 0;
api_application* WASABI_API_APP = 0;

SETUP_API_LNG_VARS;

prefsDlgRecW* output_prefs = NULL;

extern "C" __declspec(dllexport) void __cdecl winampGetOutModeChange(const int mode);

HINSTANCE g_hSlaveInstance = NULL;
Out_Module *g_pModSlave = NULL;

outMixer *g_pMixer = NULL;
int g_loaded = 0;

void Config( HWND p );
void About( HWND p );
void Init( void );
void Quit( void );
int Open( const int sr, const int nch, const int bps, const int len_ms, const int pre_len_ms );
void Close( void );
int	Write(const char * data, const int size );
int CanWrite( void );
int IsPlaying( void );
int Pause(const int new_state );
void SetVolume(const int v );
void SetPan(const int p );
void Flush( const int pos );
int GetWrittenTime( void );
int GetOutputTime( void );

////////////////////////////////////////////////////////////////////////////////
//  g_OutModMaster
////////////////////////////////////////////////////////////////////////////////
Out_Module g_OutModMaster = {
	OUT_VER_U,			// int version
	(char *)PLUGIN_NAME,// char * description
	PLUGIN_ID,			// int id
	0,					// HWND hMainWindow
	0,					// HINSTANCE hDllInstance
	Config,				// void ( * Config )( HWND hwndParent )
	About,				// void ( * About )( HWND hwndParent )
	Init,				// void ( * Init )()
	Quit,				// void ( * Quit )()
	Open,				// int ( *Open )( int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms )
	Close,				// void ( * Close )()
	Write,				// int ( * Write )( char * buf, int len )
	CanWrite,			// int ( * CanWrite )()
	IsPlaying,			// int ( * IsPlaying )()
	Pause,				// int ( * Pause )( int pause )
	SetVolume,			// void ( * SetVolume )( int volume )
	SetPan,				// void ( * SetPan )( int pan )
	Flush,				// void ( * Flush )( int t )
	GetOutputTime,		// int ( * GetOutputTime )()
	GetWrittenTime,		// int ( * GetWrittenTime )()
};

////////////////////////////////////////////////////////////////////////////////
//  Init
////////////////////////////////////////////////////////////////////////////////
void Init( void )
{
	WASABI_API_SVC = GetServiceAPIPtr();
	if (WASABI_API_SVC != NULL)
	{
		StartPluginLangWithDesc(g_OutModMaster.hDllInstance, OutNotSoNeoLangGUID,
				   IDS_PLUGIN_NAME, PLUGIN_VERSION, &g_OutModMaster.description);
	}
}

////////////////////////////////////////////////////////////////////////////////
//  Quit
////////////////////////////////////////////////////////////////////////////////
void Quit( void )
{
	if (g_pMixer)
	{
		delete g_pMixer;
		g_pMixer = NULL;
	}

	if (g_pModSlave)
	{
		void(__cdecl *changed)(const int) = (void(__cdecl *)(const int))GetProcAddress(g_hSlaveInstance, "winampGetOutModeChange");
		if (changed)
		{
			changed(OUT_UNSET);
		}

		/*if (g_pModSlave->Quit)
		{
			g_pModSlave->Quit();
		}*/

		g_hSlaveInstance = NULL;
		g_pModSlave = NULL;
	}

	// Unload DLL
	// we're not going to do this & instead let it leak
	// as there's still the possibility of the plug-in
	// doing clean-up & this then mimics winamp / wacup
	// to be safer on closing if playback was running &
	// if we need to do proper dynamic unloading we can
	// come back & change / improve this as is needed :)
	//FreeLibrary( g_hSlaveInstance );
}

////////////////////////////////////////////////////////////////////////////////
//  plugin functions
////////////////////////////////////////////////////////////////////////////////
void Config( HWND p )
{
	// incase the user only goes to the
	// config, this ensure we've setup
	// correctly otherwise all crashes
	winampGetOutModeChange(OUT_SET);

	if (output_prefs != NULL)
	{
		OpenPrefsPage((WPARAM)output_prefs);
	}
}

void About( HWND p )
{
	const unsigned char* output = DecompressResourceText(WASABI_API_LNG_HINST, WASABI_API_ORIG_HINST,
																	  IDR_ABOUT_TEXT_GZ, NULL, true);

	wchar_t message[1024]/* = { 0 }*/, title[128]/* = { 0 }*/;
	PrintfCch(message, ARRAYSIZE(message), (LPCWSTR)output, (LPWSTR)
			  g_OutModMaster.description, WACUP_Author(),
			  WACUP_Copyright(), TEXT(__DATE__));
	AboutMessageBox(p, message, LngStringCopy(IDS_ABOUT_TITLE,
									title, ARRAYSIZE(title)));

	DecompressResourceFree(output);
}

int Open( const int sr, const int nch, const int bps, const int len_ms, const int pre_len_ms )
{
	return (g_pMixer ? g_pMixer->Open( sr, nch, bps, len_ms, pre_len_ms ) : -1);
}

void Close( void )
{
	if (g_pMixer)
	{
		g_pMixer->Close();
	}
}

int Write( const char * data, const int size )
{
	return (g_pMixer ? g_pMixer->Write( data, size ) : 0);
}

int CanWrite( void )
{
	return (g_pMixer ? g_pMixer->CanWrite() : 0);
}

int IsPlaying( void )
{
	return (g_pMixer ? g_pMixer->IsPlaying() : 0);
}

int Pause( const int new_state )
{
	return (g_pMixer ? g_pMixer->Pause( new_state ) : 0);
}

void SetVolume( const int v )
{
	if (g_pMixer)
	{
		g_pMixer->SetVolume(v);
	}
}

void SetPan( const int p )
{
	if (g_pMixer)
	{
		g_pMixer->SetPan(p);
	}
}

void Flush( const int pos )
{
	if (g_pMixer)
	{
		g_pMixer->Flush(pos);
	}
}

int GetWrittenTime( void )
{
	return (g_pMixer ? g_pMixer->GetWrittenTime() : 0);
}

int GetOutputTime( void )
{
	return (g_pMixer ? g_pMixer->GetOutputTime() : 0);
}

////////////////////////////////////////////////////////////////////////////////
//  switchOutPutPlugin
////////////////////////////////////////////////////////////////////////////////
bool switchOutPutPlugin(const TCHAR* path)
{
	if (!g_pMixer)
		g_pMixer = new outMixer();

	/*if (g_pModSlave && g_pModSlave->Quit)
		g_pModSlave->Quit();*/

	// Unload DLL
	if ( g_hSlaveInstance )
	{
		void(__cdecl *changed)(const int) = (void(__cdecl *)(const int))GetProcAddress(g_hSlaveInstance, "winampGetOutModeChange");
		if (changed)
		{
			changed(OUT_UNSET);
		}

		//FreeLibrary( g_hSlaveInstance );
		g_hSlaveInstance = NULL;
	}
	g_pModSlave = NULL;

	// Load slave dll but check if it's already
	// been loaded to avoid some duplicate bits
	g_hSlaveInstance = GetOrLoadDll(path, NULL, FALSE, NULL, NULL);
	if( !g_hSlaveInstance )
	{
#if 0 // TODO
		//_strlwr( walk );
		TCHAR szBuffer[1000]/* = { 0 }*/;
		_tprintf(
			szBuffer,
			TEXT("Slave plugin could not be loaded: %s\n"),
			path
		);
		MessageBox( NULL, szBuffer, TEXT("Slave plugin error"), MB_ICONINFORMATION );
#endif
		return false;
	}

	HookPluginHelper(g_hSlaveInstance);

	// Find export
	WINAMPGETOUTMODULE winampGetOutModule =
		( WINAMPGETOUTMODULE )GetProcAddress( g_hSlaveInstance, "winampGetOutModule" );
	if( !winampGetOutModule )
	{
		//FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Get module
	g_pModSlave = winampGetOutModule();
	if( !g_pModSlave )
	{
		//FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Version mismatch?
	if( ( g_pModSlave->version != OUT_VER ) && 
		( g_pModSlave->version != OUT_VER_U ))
	{
		//FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Modify slave
	g_pModSlave->hDllInstance = g_hSlaveInstance;
	g_pModSlave->hMainWindow = g_OutModMaster.hMainWindow;
	/*g_pModSlave->Init();

	void(__cdecl *changed)(const int) = (void(__cdecl *)(const int))GetProcAddress(g_hSlaveInstance, "winampGetOutModeChange");
	if (changed)
	{
		changed(OUT_SET);
	}*/
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//  winampGetOutModule
////////////////////////////////////////////////////////////////////////////////
extern "C" __declspec(dllexport) Out_Module * winampGetOutModule(void)
{
	return &g_OutModMaster;
}

INT_PTR CALLBACK MixerConfigProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TabDlg* dlg = NULL;
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// incase the user only goes to the
			// config, this ensure we've setup
			// correctly otherwise all crashes
			winampGetOutModeChange(OUT_SET);

			if (g_pMixer)
			{
				g_pMixer->Config(hwnd);
			}
			return 0;
		}
	}

	if (!dlg) dlg = (TabDlg*)GetWindowLongPtr(hwnd, DWLP_USER);
	return (dlg ? dlg->message(hwnd, uMsg, wParam, lParam) : FALSE);
}

extern "C" __declspec(dllexport) BOOL __cdecl winampGetOutPrefs(prefsDlgRecW* prefs, const int mode)
{
	if (!mode)
	{
		// this is called when the preferences window is being created
		// and is used for the delayed registering of a native prefs
		// page to be placed as a child of the 'Output' node (why not)
		if (prefs)
		{
			Init();

			// need to have this initialised before we try
			// to do anything with localisation features
			// TODO
			StartPluginLangOnly(g_OutModMaster.hDllInstance, OutNotSoNeoLangGUID);

			// TODO localise
			prefs->hInst = GetModuleHandle(GetPaths()->wacup_core_dll)/*WASABI_API_LNG_HINST*/;
			prefs->dlgID = IDD_TABBED_PREFS_DIALOG;// IDD_CONFIG;
			prefs->name = LngStringDup(IDS_PREFS_NAME);
			prefs->proc = MixerConfigProc;
			prefs->where = 9;
			prefs->_id = 54;
			output_prefs = prefs;
		}
	}
	else
	{
		if (output_prefs != NULL)
		{
			output_prefs = (prefsDlgRecW*)RemovePrefsPage((WPARAM)output_prefs, TRUE);
		}
	}
	return !!output_prefs;
}

extern "C" __declspec(dllexport) void __cdecl winampGetOutModeChange(const int mode)
{
	// just look at the set / unset state
	switch (mode & ~0xFF0)
	{
		case OUT_UNSET:
		{
			// we've been unloaded so we can 
			// reset everything just in-case
			// avoids the prefs showing in a
			// weird state when not actively
			// used after changing plug-in
			if (g_pMixer)
			{
				g_pMixer->UnSet();
			}
			break;
		}
		case OUT_SET:
		{
			// if we're not being used then there's no
			// need to be loading anything until then
			/*if ((WASABI_API_SVC == NULL) && (WASABI_API_LNG == NULL))
			{
				WASABI_API_SVC = GetServiceAPIPtr();
				if (WASABI_API_SVC != NULL)
				{
					if (WASABI_API_LNG == NULL)
					{
						ServiceBuild(WASABI_API_SVC, WASABI_API_LNG, languageApiGUID);
						StartPluginLangOnly(plugin.hDllInstance, OutNotSoNeoLangGUID);
					}
				}
			}*/

			if (!g_loaded)
			{
				g_loaded = 1;

				if (!g_pMixer)
					g_pMixer = new outMixer();

				TCHAR szPluginName[256]/* = { 0 }*/;
				LPTSTR pszfilename = szPluginName;
				DevilConfig *pConfig = new DevilConfig( g_OutModMaster.hDllInstance );
				szPluginName[0] = 0;
				pConfig->Read(TEXT("sOutputPlugin"), szPluginName, TEXT("out_notsodirect.dll"));
				delete pConfig;

				Out_Module* out_plugin = (!szPluginName[0] ? GetUsableOutputPlugin(TRUE) : nullptr);
				if (out_plugin && (g_OutModMaster.hDllInstance != out_plugin->hDllInstance))
				{
					GetModuleFileName(out_plugin->hDllInstance, szPluginName, ARRAYSIZE(szPluginName));
					pszfilename = (LPWSTR)FindPathFileName(szPluginName);
				}

				// Load slave dll but check if it's already
				// been loaded to avoid some duplicate bits
				BOOL loaded = FALSE;
				TCHAR szFullpath[MAX_PATH]/* = { 0 }*/;
				g_hSlaveInstance = GetOrLoadDll(CombinePath(szFullpath, GetPaths()->winamp_plugin_dir,
															pszfilename), &loaded, FALSE, NULL, NULL);
				if( !g_hSlaveInstance )
				{
					/*_tcslwr(szPluginName);
					TCHAR szBuffer[1000]/* = { 0 }*//*;
					PrintfCch(
						szBuffer, ARRAYSIZE(szBuffer),
						TEXT("Slave plugin could not be loaded: %s\n"),
						szPluginName
					);
					TimedMessageBox( NULL, szBuffer, TEXT("Slave plugin error"), MB_ICONINFORMATION, 5000 );*/
					return;
				}

				HookPluginHelper(g_hSlaveInstance);

				// Find export
				WINAMPGETOUTMODULE winampGetOutModule =
					( WINAMPGETOUTMODULE )GetProcAddress( g_hSlaveInstance, "winampGetOutModule" );
				if( !winampGetOutModule )
				{
					//FreeLibrary( g_hSlaveInstance );
					return;
				}

				// Get module
				g_pModSlave = winampGetOutModule();
				if( !g_pModSlave )
				{
					//FreeLibrary( g_hSlaveInstance );
					return;
				}

				// Version mismatch?
				if( g_pModSlave->version < OUT_VER )
				{
					//FreeLibrary( g_hSlaveInstance );
					return;
				}

				// Modify slave
				g_pModSlave->hDllInstance = g_hSlaveInstance;
				g_pModSlave->hMainWindow = g_OutModMaster.hMainWindow;

				if (loaded && g_pModSlave->Init)
				{
					g_pModSlave->Init();
				}

				void(__cdecl *changed)(const int) = (void(__cdecl *)(const int))GetProcAddress(g_pModSlave->hDllInstance, "winampGetOutModeChange");
				if (changed)
				{
					changed(mode);
				}
			}
			break;
		}
	}
}

extern "C" __declspec(dllexport) const Out_Module* ProxiedOutputPlugin(void)
{
	return ((g_pModSlave != NULL) ? g_pModSlave : NULL);
}