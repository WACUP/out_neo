// This plugin is based on "Outbridge Winamp Plugin" for plugin structure and DevilConfig class (Sebastian Pipping <webmaster@hartwork.org> http://www.hartwork.org)
// This plugin use ac3filter for all audio processing (http://ac3filter.net/)

#include "out.h"
#include "outMixer.h"
#include "config\DevilConfig.h"

typedef Out_Module * ( * WINAMPGETOUTMODULE )();

HINSTANCE g_hMasterInstance = NULL; // extern
HINSTANCE g_hSlaveInstance = NULL;
Out_Module *g_pModSlave = NULL;

outMixer *g_pMixer = NULL;

UINT_PTR hMainHandleTimer = 0;

void Config( HWND p );
void About( HWND p );
void Init();
void Quit();
int Open( int sr, int nch, int bps, int bufferlenms, int prebufferms );
void Close();
int	Write( char * data, int size );
int CanWrite();
int IsPlaying();
int Pause( int new_state );
void SetVolume( int v );
void SetPan( int p );
void Flush( int pos );
int GetWrittenTime();
int GetOutputTime();


////////////////////////////////////////////////////////////////////////////////
//  g_OutModMaster
////////////////////////////////////////////////////////////////////////////////
Out_Module g_OutModMaster = {
	OUT_VER,			// int version
	PLUGIN_NAME,		// char * description
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
void Init()
{
	g_pMixer = new outMixer();
	g_pModSlave->Init();
}

////////////////////////////////////////////////////////////////////////////////
//  Quit
////////////////////////////////////////////////////////////////////////////////
void Quit()
{
	delete g_pMixer;

	// In case Quit() is called right after Init()
	// which can happen when Plainamp scans the plugin
	// the timer is still running which means we are
	// unloading the plugin although is still running.
	// Bad idea so we have to stop the timer.
	if( hMainHandleTimer )
	{
		KillTimer( NULL, hMainHandleTimer );
		hMainHandleTimer = 0;
	}

	g_pModSlave->Quit();

	// Unload DLL
	FreeLibrary( g_hSlaveInstance );
}

////////////////////////////////////////////////////////////////////////////////
//  plugin functions
////////////////////////////////////////////////////////////////////////////////
void Config( HWND p )
{
	g_pMixer->Config( p );
}

void About( HWND p )
{
	g_pMixer->About( p );
}

int Open( int sr, int nch, int bps, int len_ms, int pre_len_ms )
{
	return g_pMixer->Open( sr, nch, bps, len_ms, pre_len_ms );
}

void Close()
{
	g_pMixer->Close();
}

int Write( char * data, int size )
{
	return g_pMixer->Write( data, size );
}

int CanWrite()
{
	return g_pMixer->CanWrite();
}

int IsPlaying()
{
	return g_pMixer->IsPlaying();
}

int Pause(int new_state)
{
	return g_pMixer->Pause( new_state );
}

void SetVolume( int v )
{
	g_pMixer->SetVolume( v );
}

void SetPan( int p )
{
	g_pMixer->SetPan( p );
}

void Flush( int pos )
{
	g_pMixer->Flush( pos );
}

int GetWrittenTime()
{
	return g_pMixer->GetWrittenTime();
}

int GetOutputTime()
{
	return g_pMixer->GetOutputTime();
}


////////////////////////////////////////////////////////////////////////////////
//  GetCurrentModule
////////////////////////////////////////////////////////////////////////////////
HMODULE GetCurrentModule()
{
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery( &dummy, &mbi, sizeof( MEMORY_BASIC_INFORMATION ) );
	return ( HMODULE )mbi.AllocationBase;
}

//////////////////////////////////////////////////////////////////////////////// 
//  MainHandleTimerProc
//////////////////////////////////////////////////////////////////////////////// 
VOID CALLBACK MainHandleTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime )
{
	static bool bStayOut = false;

	if( bStayOut )
	{
		return;
	}
	
	if( g_OutModMaster.hMainWindow )
	{
		bStayOut = true;

		KillTimer( NULL, hMainHandleTimer );
		hMainHandleTimer = 0;

		g_pModSlave->hMainWindow = g_OutModMaster.hMainWindow;

		bStayOut = false;
	}
}

////////////////////////////////////////////////////////////////////////////////
//  switchOutPutPlugin
////////////////////////////////////////////////////////////////////////////////
bool switchOutPutPlugin(const char* path)
{
	if( hMainHandleTimer )
	{
		KillTimer( NULL, hMainHandleTimer );
		hMainHandleTimer = 0;
	}

	if ( g_pModSlave )
		g_pModSlave->Quit();

	// Unload DLL
	if ( g_hSlaveInstance )
		FreeLibrary( g_hSlaveInstance );
	g_hSlaveInstance = NULL;
	g_pModSlave = NULL;

	// Load slave dll
	g_hSlaveInstance = LoadLibrary( path );
	if( !g_hSlaveInstance )
	{
		//_strlwr( walk );
		char szBuffer[ 1000 ];
		wsprintf(
			szBuffer,
			"Slave plugin could not be loaded: %s\n",
			path
		);
		MessageBox( NULL, szBuffer, "Slave plugin error", MB_ICONINFORMATION );
		return false;
	}

	// Find export
	WINAMPGETOUTMODULE winampGetOutModule =
		( WINAMPGETOUTMODULE )GetProcAddress( g_hSlaveInstance, "winampGetOutModule" );
	if( !winampGetOutModule )
	{
		FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Get module
	g_pModSlave = winampGetOutModule();
	if( !g_pModSlave )
	{
		FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Version mismatch?
	if( g_pModSlave->version < OUT_VER )
	{
		FreeLibrary( g_hSlaveInstance );
		return false;
	}

	// Modify slave
	g_pModSlave->hDllInstance   = g_hSlaveInstance;

	// Start main window detection
	hMainHandleTimer = SetTimer( NULL, 0, 333, MainHandleTimerProc );

	g_pModSlave->Init();
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//  winampGetOutModule
////////////////////////////////////////////////////////////////////////////////
extern "C" __declspec( dllexport ) Out_Module * winampGetOutModule()
{
	g_hMasterInstance = ( HINSTANCE )GetCurrentModule();
	g_OutModMaster.hDllInstance = g_hMasterInstance;

	// Get master path
	char szFullpath[ MAX_PATH ] = "";
	GetModuleFileName( g_hMasterInstance, szFullpath, MAX_PATH - 6 - 1 );
	const int iFullLathLen = strlen( szFullpath );

	// Get slave path
	char * walk = szFullpath + iFullLathLen - 1; // Last char
	while( ( walk > szFullpath ) && ( *walk != '\\' ) ) walk--;
	walk++;

	DevilConfig *pConfig = new DevilConfig( g_hMasterInstance, "Winamp" );
	char sPlugin[ MAX_PATH ] = "";
	int a = MAX_PATH - (walk - szFullpath) - 1;
	pConfig->Read("sOutputPlugin", walk, "out_ds.dll", a);
	delete pConfig;

	// Load slave dll
	g_hSlaveInstance = LoadLibrary( szFullpath );
	if( !g_hSlaveInstance )
	{
		_strlwr( walk );
		char szBuffer[ 1000 ];
		wsprintf(
			szBuffer,
			"Slave plugin could not be loaded: %s\n",
			walk
		);
		MessageBox( NULL, szBuffer, "Slave plugin error", MB_ICONINFORMATION );
		return NULL;
	}

	// Find export
	WINAMPGETOUTMODULE winampGetOutModule =
		( WINAMPGETOUTMODULE )GetProcAddress( g_hSlaveInstance, "winampGetOutModule" );
	if( !winampGetOutModule )
	{
		FreeLibrary( g_hSlaveInstance );
		return NULL;
	}

	// Get module
	g_pModSlave = winampGetOutModule();
	if( !g_pModSlave )
	{
		FreeLibrary( g_hSlaveInstance );
		return NULL;
	}

	// Version mismatch?
	if( g_pModSlave->version < OUT_VER )
	{
		FreeLibrary( g_hSlaveInstance );
		return NULL;
	}

	// Modify slave
	g_pModSlave->hDllInstance   = g_hSlaveInstance;

	// Start main window detection
	hMainHandleTimer = SetTimer( NULL, 0, 333, MainHandleTimerProc );

	return &g_OutModMaster;
}
