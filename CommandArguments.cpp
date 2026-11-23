// Copyright © 2015 CCP ehf.
#include "StdAfx.h"
#include "CommandArguments.h"
#include <fstream>
#ifdef _WIN32
#include <shellapi.h>
#endif

namespace
{

void Usage()
{
	fprintf(stderr, "Usage: exefile [flag ...]\n");
	fprintf(stderr, "flags: /?, /help: this text\n");
	fprintf(stderr, "       /root=<root path>\n");
	fprintf(stderr, "       /bin=<bin path>\n");
	fprintf(stderr, "       /lib=<bin path>\n");
	fprintf(stderr, "       /verbose , /noverbose\n");
	fprintf(stderr, "       /console , /noconsole, /inherit\n");
	fprintf(stderr, "       /aflock=<aff>\n");
	fprintf(stderr, "       /jessica\n");
	fprintf(stderr, "       /pyoptimize=<opt>\n");
	fprintf(stderr, "       /stderr=<path with %%p as pid>\n");
	fprintf(stderr, "       /stdout=<path with %%p as pid>\n");
	fprintf(stderr, "       /noCrashReportUpload\n");
	fprintf(stderr, "       /service\n");
	fprintf(stderr, "       /buildflavor=<flavor>\n");
	fprintf(stderr, "       paths can start with $(cwd)\n");
	exit( 0 );
}

#ifdef _WIN32

CommandLine SplitCommandLine(const wchar_t *line)
{
	int nArgs;
	LPWSTR *argv = CommandLineToArgvW(line, &nArgs);
	std::vector<std::wstring> res;
	for(int i = 0; i<nArgs; i++)
	{
		res.push_back(argv[i]);
	}
	LocalFree(argv);
	return res;
}


bool GetEnvironmentVar(std::wstring *val, const wchar_t *name)
{
	wchar_t f;
	DWORD size = GetEnvironmentVariableW(name, &f, 0);
	if (!size)
	{
		return false; // not set
	}
	if (!val)
	{
		return true; //just test for existance
	}
	std::vector<wchar_t> buf(size);
	DWORD size2 = GetEnvironmentVariableW(name, &buf[0], size);
	if(size2 > 0 && size2+1 <= size)
	{
		*val = &buf[0];
		return true;
	}
	return false;
}

#else

CommandLine SplitCommandLine(const wchar_t *line)
{
	CommandLine result;
	std::wstring arg;
	bool inQuotes = false;
	for( auto i = line; *i; ++i )
	{
		if( !inQuotes && ( *i == L' ' || *i == L'\t' ) )
		{
			if( !arg.empty() )
			{
				result.push_back( arg );
				arg = L"";
				continue;
			}
		}
		else if( *i == L'"' )
		{
			if( !arg.empty() && arg.back() == L'\\' )
			{
				arg.back() = *i;
			}
			else
			{
				inQuotes = !inQuotes;
			}
			continue;
		}
		else if( *i == L'\\' )
		{
			if( !arg.empty() && arg.back() == L'\\' )
			{
				continue;
			}
		}
		arg += *i;
	}
	if( !arg.empty() )
	{
		result.push_back( arg );
	}
	return result;
}

bool GetEnvironmentVar(std::wstring *val, const wchar_t *name)
{
	auto env = getenv( CW2A( name ) );
	if( !env )
	{
		return false;
	}
	if( val )
	{
		*val = CA2W( env );
	}
	return true;
}

#endif

std::wstring GetEnvironmentArgs()
{
	std::wstring result;
	GetEnvironmentVar( &result, L"EXEFILE_ARGS" );
	return result;
}

std::vector<std::wstring> ExpandCommandLineWithEnvironment( const CommandLine& arguments )
{
	//construct vector of arguments
	auto tmpArgv = arguments;
	if( tmpArgv.size() < 2 )
	{
		//use env var if nothing on the cmd line
		std::wstring envargs = GetEnvironmentArgs();
		if( envargs.size() )
		{
			// don't call SplitCommandLine with an empty envargs since
			// it will cause the executable path to be used instead
			std::vector<std::wstring> tmp = SplitCommandLine( envargs.c_str() );
			tmpArgv.insert( tmpArgv.end(), tmp.begin(), tmp.end() );
		}
	}
	return tmpArgv;
}

std::wstring TrimLeft( const std::wstring& str )
{
	int start = 0;
	for( std::wstring::const_iterator i = str.begin(); i != str.end(); ++i)
	{
		if( *i != L' ' && *i != L'\t' )
			break;
		++start;
	}

	return str.substr( start );
}

void ExpandFileContents( const std::wstring& filename, std::vector<std::wstring>& argv )
{
	std::ifstream is;
#ifdef _WIN32
	is.open( filename.c_str() );
#else
	is.open( CW2A( filename.c_str() ) );
#endif
	if( !is.good() )
	{
		CCP_LOGERR( "File name '%S' provided as startup argument to executable cannot be read", filename.c_str() );
		return;
	}

	while( is.good() )
	{
		std::string a;
		is >> a;

		std::wstring wa = TrimLeft( std::wstring( CA2W( a.c_str() ) ) );

		if( wa.length() > 0 )
		{
			if( wa[0] == L'@' )
			{
				std::wstring filename = wa.substr(1);
				ExpandFileContents( filename, argv );
			}
			else
			{
				argv.push_back( wa );
			}
		}
	}
}

CommandLine ExpandCommandLineWithFiles( const std::vector<std::wstring>& source )
{
	CommandLine destination;

	for( auto it = std::begin( source ); it != std::end( source ); ++it )
	{
		if( ( *it )[0] == L'@' )
		{
			std::wstring filename = it->substr(1);
			ExpandFileContents( filename, destination );
		}
		else
		{
			destination.push_back( *it );
		}
	}

	return destination;
}

std::wstring ToLower( const std::wstring &s )
{
	std::wstring r = s;
	for( size_t i = 0; i< s.size(); i++ )
	{
		wchar_t c = r[i];
		if( iswupper( c ) )
		{
			wchar_t lc = towlower( c );
			if( iswlower( lc ) )
			{
				c = lc;
			}
		}
		r[i] = c;
	}
	return r;
}

}

#if _WIN32
CommandLine ParseCommandLine()
{
	auto rawCommandLine = SplitCommandLine( GetCommandLineW() );
	return ExpandCommandLineWithFiles( ExpandCommandLineWithEnvironment( rawCommandLine ) );
}
#elif __APPLE__ || __linux__
CommandLine ParseCommandLine( int argc, char* argv[] )
{
	CommandLine rawCommandLine;
	for( int i = 0; i < argc; ++i )
	{
		rawCommandLine.push_back( std::wstring( CA2W( argv[i] ) ) );
	}
	return ExpandCommandLineWithFiles( ExpandCommandLineWithEnvironment( rawCommandLine ) );
}
#endif

#if !_WIN32
int _wtoi( const wchar_t* str )
{
	return atoi( CW2A( str ) );
}
#endif

void DumpCommandLineToDebugger( const CommandLine& commandLine )
{
#if __APPLE__
	if ( !CcpIsDebuggerPresent() ) {
		return;
	}
#endif

    for( auto it = std::begin( commandLine ); it != std::end( commandLine ); ++it )
    {
#if _WIN32
        OutputDebugStringW( it->c_str() );
        OutputDebugStringW( L"\n" );
#elif __APPLE__ || __linux__
        wprintf(L"%ls\n", it->c_str());
#else
#error "Unsupported platform!"
#endif
    }
}

CommandArguments GetCommandArguments( const CommandLine& commandLine )
{
	bool verbose = false;
	CommandArguments commandArguments;

	//Environment var way of turning on our spiffy new _inherit mode.
	//This is used by ExeFile.com
	if(GetEnvironmentVar( 0, L"EXEFILE_INHERIT" ) )
	{
		commandArguments.consoleMode = console_mode_inherit;
	}

	// Special case: If started witout arguments, it behaves as though eve.exe had
	// started it.  This is to support "pinning to taskbar" on windows 7.
	// Later we want better taskbar support.
	if( commandLine.size() < 2 )
	{
		commandArguments.consoleMode = console_mode_off;
	}

	std::wstring redirect_stderr;
	std::wstring redirect_stdout;

	// Old-style path for rootpath
	std::wstring rootPath;
	// Old-style path for binpath
	std::wstring binPath;
	// Old-style path for libpath
	std::wstring libPath;

	for( size_t i = 1; i < commandLine.size(); i++ )
	{
		const std::wstring &arg = commandLine[i];
		const std::wstring larg = ToLower(arg);
		if (larg.find(L"/?")==0 || larg.find(L"help")==0) {
			Usage();
		} else if (larg.find(L"/root=")==0) {
			rootPath = arg.substr(6);
			std::wstring path = L"root=";
			path += rootPath;
			commandArguments.searchPaths.push_back( path );
		} else if (larg.find(L"/bin=")==0) {
			binPath = arg.substr(5);
			std::wstring path = L"bin=";
			path += binPath;
			commandArguments.searchPaths.push_back( path );
		} else if (larg.find(L"/lib=")==0) {
			libPath = arg.substr(5);
			std::wstring path = L"lib=";
			path += libPath;
			commandArguments.searchPaths.push_back( path );
		} else if (larg.find(L"/path:")==0 ) {
			// Gather up search paths - push them to BeOS once we've loaded Blue
			std::wstring path = arg.substr(6);
			commandArguments.searchPaths.push_back( path );
		} else if (larg == L"/verbose") {
			verbose = true;
		} else if (larg == L"/noverbose") {
			verbose = false;
		} else if (larg == L"/console") {
			commandArguments.consoleMode = console_mode_create;
		} else if (larg == L"/noconsole") {
			commandArguments.consoleMode = console_mode_off;
		} else if (larg == L"/inherit") {
			commandArguments.consoleMode = console_mode_inherit;
		} else if (larg == L"/nocrashreportupload") {
			commandArguments.uploadMinidump = false;
		} else if (larg.find(L"/aflock=")==0) {
			commandArguments.affinity = _wtoi(arg.substr(8).c_str());
		} else if (larg.find(L"/pyoptimize=")==0) {
			commandArguments.pyOptimize = _wtoi(arg.substr(12).c_str());
		} else if (larg.find(L"/stderr=") == 0) {
			commandArguments.redirectStdErr = arg.substr(8);
		} else if (larg.find(L"/stdout=") == 0) {
			commandArguments.redirectStdOut = arg.substr(8);
		} else if (larg == L"/service") {
			commandArguments.asService = true;
		} else if (larg.find(L"/buildflavor=") == 0) {
			commandArguments.buildFlavor = arg.substr(13);
		}
	}

	if( verbose )
	{
		fprintf(stdout, "%S starting up\n", commandLine.size()>0?commandLine[0].c_str() : L"ExeFile.exe");
		fprintf(stdout, "/root=%S\n", rootPath.c_str());
		fprintf(stdout, "/bin=%S\n", binPath.c_str());
		fprintf(stdout, "/lib=%S\n", libPath.c_str());
		fprintf(stdout, "/%sverbose\n", verbose?"":"no");
		fprintf(stdout, "/console_mode %d\n", commandArguments.consoleMode);
		fprintf(stdout, "/aflock=%d\n", commandArguments.affinity);
		fprintf(stdout, "/pyoptimize=%d\n", commandArguments.pyOptimize);
		fprintf(stdout, "/service=%s\n", commandArguments.asService ? "yes" : "no");
		fprintf(stdout, "/buildflavor=%S\n", commandArguments.buildFlavor.c_str());
	}

	return commandArguments;
}
