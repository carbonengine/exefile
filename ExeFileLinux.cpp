#include "StdAfx.h"
#ifdef __linux__
#include "ExeFile.h"

bool IsSupportedOSVersion()
{
	return true;
}
int main( int argc, char* argv[] )
{
	CommandLine commandLine = ParseCommandLine( argc, argv );
	return Main( commandLine, IsSupportedOSVersion() );
}
#endif