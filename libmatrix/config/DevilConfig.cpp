

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


#include "DevilConfig.h"
#include <stdio.h>
#include <shlobj.h>


// Don't warn on int->bool
#pragma warning( disable : 4800 )


// *****************************************************************************
void DevilConfig::Init( const TCHAR * szCopySection, HMODULE hMod, const TCHAR *szAppDataSection )
{
	// Save section name
	szSection = new TCHAR[ _tcslen( szCopySection ) + 1 ];
	_tcscpy( szSection, szCopySection );


	/*

	(1) Get the filename belonging to this process (e.g. "teston~1.scr")
	(2) We ensure we got the long filename to avoid trouble ("test one.scr")
	(3) We replace ".scr" by ".ini" ("test one.ini")

	*/


	szIniPath = new TCHAR[ _MAX_PATH ];


	TCHAR szFull[ _MAX_PATH ] = TEXT( "" );
	TCHAR szDrive[ _MAX_DRIVE	] = TEXT( "" );
	TCHAR szDir[ _MAX_DIR	] = TEXT( "" );


	// Get our filename
	GetModuleFileName( hMod, szFull, _MAX_PATH );


	// Split it up
	_tsplitpath( szFull, szDrive, szDir, NULL, NULL );


	// Convert short to long path
	GetLongPathName( szDir, szDir, _MAX_DIR );


	// Convert short to long file
	WIN32_FIND_DATA fd;
	HANDLE h = FindFirstFile( szFull, &fd );


	// Search last dot
	TCHAR * szSearch = fd.cFileName + _tcslen( fd.cFileName ) - 1;
	while( ( *szSearch != TEXT( '.' ) ) && ( szSearch > fd.cFileName ) )
	{
		szSearch--;
	}


	// Replace extension
	// assert( szSearch > fd.cFileName );
	_tcscpy( szSearch, TEXT( ".ini" ) );


	// Copy full filename
	if (szAppDataSection == NULL)
		_sntprintf( szIniPath, _MAX_PATH, TEXT( "%s%s%s" ), szDrive, szDir, fd.cFileName );
	else
	{
		SHGetSpecialFolderPath(0, szFull, CSIDL_APPDATA, FALSE);
		_sntprintf( szIniPath, _MAX_PATH, TEXT( "%s\\%s\\%s" ), szFull, szAppDataSection, fd.cFileName );
	}
}



// *****************************************************************************
DevilConfig::DevilConfig( HMODULE hMod, const TCHAR *szAppDataSection )
{
	Init( TEXT( "Config" ), hMod, szAppDataSection );
}



// *****************************************************************************
DevilConfig::DevilConfig( const TCHAR * szCopySection, HMODULE hMod, const TCHAR *szAppDataSection )
{
	Init( szCopySection, hMod, szAppDataSection );
}



// *****************************************************************************
DevilConfig::DevilConfig( const TCHAR * szCopySection, const TCHAR * szFilename )
{
	const UINT uFilenameLen = _tcslen( szFilename );
	szIniPath = new TCHAR[ uFilenameLen + 1 ];
	memcpy( szIniPath, szFilename, uFilenameLen * sizeof( TCHAR ) );
	szIniPath[ uFilenameLen ] = TEXT( '\0' );
	
	const UINT uSectionLen = _tcslen( szCopySection );
	szSection = new TCHAR[ uSectionLen + 1 ];
	memcpy( szSection, szCopySection, uSectionLen * sizeof( TCHAR ) );
	szSection[ uSectionLen ] = TEXT( '\0' );
}



// *****************************************************************************
DevilConfig::~DevilConfig()
{
	if( NULL != szIniPath )
	{
		delete [] szIniPath;
	}
	if( NULL != szSection )
	{
		delete [] szSection;
	}
}



// *****************************************************************************
bool DevilConfig::Write( const TCHAR * szKey, const double fValue )
{
	TCHAR szText[ 50 ] = TEXT( "" );
	_stprintf( szText, TEXT( "%.16f" ), fValue );
	return ( bool )WritePrivateProfileString( szSection, szKey, szText, szIniPath );
}



// *****************************************************************************
bool DevilConfig::Write( const TCHAR * szKey, const int iValue )
{
	TCHAR szNumber[ 12 ] = TEXT( "" );
	_stprintf( szNumber, TEXT( "%i" ), iValue );
	return ( bool )WritePrivateProfileString( szSection, szKey, szNumber, szIniPath );
}



// *****************************************************************************
bool DevilConfig::Write( const TCHAR * szKey, const TCHAR * szText )
{
	return ( bool )WritePrivateProfileString( szSection, szKey, szText, szIniPath );
}



// *****************************************************************************
bool DevilConfig::Read( const TCHAR * szKey, double * fOut, const double fDefault )
{
	TCHAR szDefault[ 50 ] = TEXT( "" );
	_stprintf( szDefault, TEXT( "%.16f" ), fDefault );

	TCHAR szOut[ 50 ] = TEXT( "" );
	bool res = ( bool )GetPrivateProfileString( szSection, szKey, szDefault, szOut, 20, szIniPath );
	if( res )
	{
		*fOut = _tcstod( szOut, NULL );
	}
	return res;
}



// *****************************************************************************
bool DevilConfig::Read( const TCHAR * szKey, int * iOut, const int iDefault )
{
	*iOut = GetPrivateProfileInt( szSection, szKey, iDefault, szIniPath );
	return true;
}



// *****************************************************************************
bool DevilConfig::Read( const TCHAR * szKey, TCHAR * szOut, const TCHAR * szDefault, UINT uSize )
{
	return ( bool )GetPrivateProfileString( szSection, szKey, szDefault, szOut, uSize, szIniPath );
}



// *****************************************************************************
void DevilConfig::CopyIniPath( TCHAR * szOut, UINT uSize )
{
	_tcsncpy( szOut, szIniPath, uSize );
}
