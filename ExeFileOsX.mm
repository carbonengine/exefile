#include "StdAfx.h"

#ifdef __APPLE__

#include "ExeFile.h"

bool IsSupportedOSVersion()
{
    if( @available( macOS 10.15, * ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

int main( int argc, char* argv[] )
{
	CommandLine commandLine = ParseCommandLine( argc, argv );
	return Main( commandLine, IsSupportedOSVersion() );
}

#endif
