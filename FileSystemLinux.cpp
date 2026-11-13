#include "StdAfx.h"

#if __linux__

#include "FileSystem.h"

#include <cstdlib>

std::string GetAppdataFolder()
{
	const char* xdgDataHome = std::getenv( "XDG_DATA_HOME" );

	if( xdgDataHome && (xdgDataHome[0] != '\0') )
	{
		return xdgDataHome;
	}
	// Fallback to ~/.local/share if XDG not found
	const char* home = std::getenv( "HOME" );
	if( !home )
	{
		return "";
	}
	return std::string( home ) + "/.local/share";
}

#endif