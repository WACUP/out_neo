

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
    *                                                                 *
    *   DEVIL Screensaver System 1.03                                 *
    *   Copyright © 2005 Sebastian Pipping <webmaster@hartwork.org>   *
    *                                                                 *
    *   --> http://www.hartwork.org                                   *
    *                                                                 *
    *                                                                 *
    *   This source code is released under LGPL.                      *
    *   See LGPL.txt for details.                        2005-08-31   *
    *                                                                 *
    \* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef DEVIL_CONFIG_H
#define DEVIL_CONFIG_H

// For GetLongPathName
#if _WIN32_WINDOWS < 0x0410
# undef _WIN32_WINDOWS
# define _WIN32_WINDOWS 0x0410
#endif

#include <windows.h>
#include <tchar.h>

#include <stdlib.h>

class DevilConfig
{
private:
	TCHAR * szIniPath;
	TCHAR * szSection;
	void Init( const TCHAR * szCopySection, HMODULE hMod );

public:
	explicit DevilConfig( HMODULE hMod = NULL );
	explicit DevilConfig( const TCHAR * szCopySection, HMODULE hMod = NULL );
	DevilConfig( const TCHAR * szCopySection, const TCHAR * szFilename );
	~DevilConfig();

	bool Write( const TCHAR * szKey, const double fValue );
	bool Write( const TCHAR * szKey, const int iValue );
	bool Write( const TCHAR * szKey, const TCHAR * szText );

	bool Read( const TCHAR * szKey, double * fOut, const double fDefault );
	bool Read( const TCHAR * szKey, int * iOut, const int iDefault );
	bool Read( const TCHAR * szKey, TCHAR * szOut, const TCHAR * szDefault, UINT uSize = 256 );

#if 0
	void CopyIniPath( TCHAR * szOut, UINT uSize = MAX_PATH );
	TCHAR * GetIniPath() { return szIniPath; }
#endif
};

#endif // DEVIL_CONFIG_H