#include "StdAfx.h"

#if __linux__

#include "FileSystem.h"

#include <cstdlib>

std::string GetAppdataFolder()
{
	const char* xdg_data_home = std::getenv( "XDG_DATA_HOME" );

	if( xdg_data_home )
	{
		return xdg_data_home;
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