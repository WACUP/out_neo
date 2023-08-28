

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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#include <stdlib.h>


class DevilConfig
{
private:
	TCHAR * szIniPath;
	TCHAR * szSection;
	void Init( const TCHAR * szCopySection, HMODULE hMod, const TCHAR *szAppDataSection = NULL );

public:
	DevilConfig( HMODULE hMod = NULL, const TCHAR *szAppDataSection = NULL );
	DevilConfig( const TCHAR * szCopySection, HMODULE hMod = NULL, const TCHAR *szAppDataSection = NULL );
	DevilConfig( const TCHAR * szCopySection, const TCHAR * szFilename );
	~DevilConfig();

	bool Write( const TCHAR * szKey, const double fValue );
	bool Write( const TCHAR * szKey, const int iValue );
	bool Write( const TCHAR * szKey, const TCHAR * szText );

	bool Read( const TCHAR * szKey, double * fOut, const double fDefault );
	bool Read( const TCHAR * szKey, int * iOut, const int iDefault );
	bool Read( const TCHAR * szKey, TCHAR * szOut, const TCHAR * szDefault, UINT uSize = 256 );

	void CopyIniPath( TCHAR * szOut, UINT uSize = _MAX_PATH );
	TCHAR * GetIniPath() { return szIniPath; }
};



#endif // DEVIL_CONFIG_H
