// Copyright © 2014 CCP ehf.
#include "StdAfx.h"
#include "ExeFile.h"
#include "Crashpad.h"
#include "BlueInterface.h"
#include <CcpLog.h>

#include <errno.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <wchar.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "CommandArguments.h"

const char* g_moduleName = "ExeFile";

#ifdef _WIN32
#include <signal.h>
#include <timeapi.h>
#include <windows.h>
#endif

enum ExitCodes : int
{
	SUCCESS = 0,
	STACKLESS_INIT_ERROR = 4,
	FAILED_LOADING_BLUE_RELEASE_FLAVOUR = 5,
	SEARCH_PATH_ARGUMENT_ERROR = 6,
	PYTHON_INIT_ERROR = 7,
};

//Use this to redirect standard error to a file.
//Note, that we don't like this much.  The user should use
//ExeFile.com in conjunction with shell redirection to do this.
void RedirectOutput( FILE* which, const wchar_t* pattern )
{
	std::wstring tmp;
	const wchar_t *f = wcsstr( pattern, L"%p" );
	if( f )
	{
		tmp = std::wstring( pattern, ( f-pattern ) );
		tmp += std::to_wstring( uint64_t( CcpGetCurrentProcessId() ) );
		tmp += std::wstring( f + 2 );
		pattern = tmp.c_str();
	}
	FILE *os;
#ifdef _WIN32
	//don"t use _wfreopen_s, because it uses no-sharing for the file
	//(one can't browse it until the process dies)
#pragma warning( suppress : 4996 )
	os = _wfreopen(pattern, L"a", which);
#else
	os = freopen( CW2A( pattern ), "a", which );
#endif
	if( os )
	{
		setvbuf(os, 0, _IONBF, 2);
	}
}

#ifdef _WIN32

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T(
				"Eve Node Service: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

		raise(SIGINT); // Signal the main-thread by sending an interrupt to the process.
					   // I HOPE that the stackless system catches this SIGINT and properly shuts down the process

		break;

	default:
		break;
	}
}

bool ServiceSetup()
{
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T(
				"Eve Node Service: ServiceMain: SetServiceStatus returned error"));
		}
		return FALSE;
	}
	ResetEvent(g_ServiceStopEvent);

	g_StatusHandle = RegisterServiceCtrlHandler("", ServiceCtrlHandler);
	if (g_StatusHandle == NULL)
	{
		return FALSE;
	}

	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T(
			"Eve Node Service: ServiceMain: SetServiceStatus returned error"));
	}

	return TRUE;
}

void ServiceStart()
{
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T(
			"Eve Node Service: ServiceMain: SetServiceStatus returned error"));
	}
}

void ServiceStop()
{
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T(
			"Eve Node Service: ServiceMain: SetServiceStatus returned error"));
	}
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	ServiceSetup();
	ServiceStart();
	WaitForSingleObject(g_ServiceStopEvent, INFINITE);
	ServiceStop();
	CloseHandle(g_ServiceStopEvent);
}

DWORD WINAPI ServiceEntrypoint(LPVOID lpParam)
{

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPSTR)"", (LPSERVICE_MAIN_FUNCTION)ServiceMain }
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		return GetLastError();
	}

	return 0;
}

#endif

bool BuildConcatenatedPathFromPathlist( const std::vector<std::wstring>& pathlist, std::wstring& path, bool interpreterMode )
{
#ifdef _WIN32
	const wchar_t separator = L';';
#else
	const wchar_t separator = L':';
#endif
	path.clear();

	// We need to obey the environment settings when running in interpreter mode.
	// NB: this is only necessary because our Stackless fork overrides the way sys.path is bootstrapped,
	// and as such we kind of need to re-implement a part of `calculate_path()` in stackless' getpathp.c.
	if( interpreterMode )
	{
		// in case you're wondering about the maximum length of an environment variable on Windows:
		// https://devblogs.microsoft.com/oldnewthing/20100203-00/?p=15083
		// And for posix, see "Limits on size of arguments and environment here:
		// https://man7.org/linux/man-pages/man2/execve.2.html
		wchar_t pythonpath[4096] = { '\0' };
#if _MSC_VER
		// MSVC complains that `std::getenv` isn't safe. However, here the usage is safe because this
		// codepath is single threaded and we copy the pointed at value via the `mbstowcs_s` call below.
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
		auto pythonpath_env = std::getenv( "PYTHONPATH" );
#if _MSC_VER
#pragma warning(pop)
#endif
		if( pythonpath_env )
		{
			if ( mbstowcs( pythonpath, pythonpath_env, std::extent_v<decltype(pythonpath)> - 1 ) == -1 )
			{
				return false;
			}
			path = pythonpath;
			path += separator;
		}
	}

	for( size_t i = 0; i < pathlist.size(); i++ )
	{
		std::wstring elem = pathlist[i];
		while( elem[elem.size() - 1] == L'/' || elem[elem.size() - 1] == L'\\' )
			elem.erase( elem.size() - 1 );
		if( !elem.size() )
			continue;
		if( i )
			path += separator;
		path += elem;
	}

	return true;
}

// Builds the interpreter's command line: argv[0] as this process received it, followed by every argument after /py,
// or the whole command line when /py is absent. Each argument is narrowed with CW2A, as runPyMain() did before.
std::vector<std::string> GetPythonArgv( const CommandLine& commandLine )
{
	auto marker = std::find( commandLine.begin(), commandLine.end(), L"/py" );
	auto first = marker == commandLine.end() ? commandLine.begin() + 1 : marker + 1;

	std::vector<std::string> pythonArgv{ std::string( CW2A( commandLine.front().c_str() ) ) };
	for( auto it = first; it != commandLine.end(); ++it )
	{
		pythonArgv.push_back( std::string( CW2A( it->c_str() ) ) );
	}
	return pythonArgv;
}

// Configures and initializes Python. pythonArgv is the interpreter's command line when ExeFile is about to run as
// the vanilla interpreter, and empty otherwise. Python must only be initialized once: initializing it again would
// discard this configuration, and reset sys.prefix after site.py has pointed it at a venv.
bool ConfigurePython( BlueInterface& blue, bool interpreterMode, const std::vector<std::string>& pythonArgv )
{
	// TODO this doesn't seem to work well when mixing Python C extensions
	// that are built in debug vs. non-debug...
	CCP_LOG( "Installing Python memory allocators" );
	blue.InstallPythonMemoryHooks();

	PyPreConfig preConfig;
	PyConfig config;
	PyStatus status;

	std::vector<char*> argvPointers;
	for( const auto& arg : pythonArgv )
	{
		argvPointers.push_back( const_cast<char*>( arg.c_str() ) ); // CPython copies, never writes through these
	}
	const auto argc = static_cast<Py_ssize_t>( argvPointers.size() );

	CCP_LOG( "Pre-initializing Python" );
	if( !interpreterMode )
	{
		PyPreConfig_InitIsolatedConfig( &preConfig );
	}
	else
	{
		PyPreConfig_InitPythonConfig( &preConfig );
	}

	if( pythonArgv.empty() )
	{
		status = Py_PreInitialize( &preConfig );
	}
	else
	{
		// -E, -I, -X utf8 and -X dev are settled at pre-initialization.
		status = Py_PreInitializeFromBytesArgs( &preConfig, argc, argvPointers.data() );
	}
	CCP_LOG( "Pre-init reported exit code %d and message %s", status.exitcode, status.err_msg );

	CCP_LOG( "Configuring Python" );
	if( !interpreterMode )
	{
		PyConfig_InitIsolatedConfig( &config );
		config.write_bytecode = 0;
		config.optimization_level = 2;
		config.site_import = 0;
	}
	else
	{
		PyConfig_InitPythonConfig( &config );
		// We always disable the user site directory - even in interpreter mode.
		// The reason for this is that any C extension in the user's site directory
		// won't be compatible with us in any case. Additionally, we seem to have an
		// issue treating the corresponding configuration flag correctly, so we cannot
		// just add `-s` or set `PYTHONNOUSERSITE=1` in the pythonInterpreter scipts.
		config.user_site_directory = 0;
	}
	CCP_LOG( "Init reported exit code %d and message %s", status.exitcode, status.err_msg );

	CCP_LOG( "Adding warn option " );
	status = PyWideStringList_Append( &( config.warnoptions ), L"d" );
	CCP_LOG( "Warn option reported exit code %d and message %s", status.exitcode, status.err_msg );
	CCP_LOG( "Setting argv" );
	if( pythonArgv.empty() )
	{
		status = PyConfig_SetArgv( &config, 0, nullptr );
	}
	else
	{
		// CPython parses the interpreter's command line when it reads the configuration, so -c, -m, -W, -O and the
		// rest take effect in this initialization, and Py_RunMain() runs what they asked for.
		status = PyConfig_SetBytesArgv( &config, argc, argvPointers.data() );
	}
	CCP_LOG( "SetArgv reported exit code %d and message %s", status.exitcode, status.err_msg );

	std::vector<std::wstring> pathlist;
	static std::wstring path;

	if( !blue.ConstructPathListFromManifest( pathlist, !interpreterMode ) )
	{
		CCP_LOGERR("InitSysIncludePaths() failed");
		return false;
	}
	if ( ! BuildConcatenatedPathFromPathlist( pathlist, path, interpreterMode ) )
	{
		CCP_LOGERR( "Failed constructing path list" );
		return false;
	}

	std::wstringstream pathHelper( path );

#if _WIN32
	for( std::wstring tmp; std::getline( pathHelper, tmp, L';' ); )
#elif __APPLE__
	for( std::wstring tmp; std::getline( pathHelper, tmp, L':' ); )
#else
#error Unknown env var separator on this platform
#endif
	{
		status = PyWideStringList_Append( &( config.module_search_paths ), tmp.c_str() );
		CCP_LOG( "Appending %S to sys.path candidate list (error %d: %s)", tmp.c_str(), status.exitcode, status.err_msg );
	}
	config.module_search_paths_set = 1;

	// We want to avoid cluttering the Perforce workspace with `__pycache__` folders. Therefore, write any compiled
	// bytecode to our usual cache location instead.
	auto cachePath = blue.ResolvePathForWritingW( L"cache:/__pycache__" );
	if (!cachePath.empty()) {
		config.pycache_prefix = cachePath.data();
		CCP_LOG("Configured __pycache__ location to be %ls", config.pycache_prefix);
	}
	else {
		CCP_LOG("Not configuring __pycache__ because `cache:` prefix not registered");
	}

	// Initialize built-in Python modules
	std::vector<_inittab> extendedInitTab;
	blue.GetInitTab(extendedInitTab);

	auto initTabPtr = extendedInitTab.data();

	if( PyImport_ExtendInittab( initTabPtr ) == -1 )
	{
		CCP_LOGERR( "Failed extending inittab with CCP builtins" );
		return false;
	}

	CCP_LOG( "Initializing Python" );
	status = Py_InitializeFromConfig( &config );

	if( PyStatus_IsExit( status ) )
	{
		// CPython has handled -h, --version or an invalid option on the interpreter's command line. Exit with its
		// exit code, as the vanilla interpreter does.
		Py_ExitStatusException( status );
	}

	if( !Py_IsInitialized() )
	{
		//PyFlushError("Failed initializing Python");
		CCP_LOGERR( "Py_Initialize() failed with error %d: %s", status.exitcode, status.err_msg );
		return false;
	}

	return true;
}

/*
	This is so that we can have exefile behave as a standard python interpreter.
	ConfigurePython() has already initialized Python from the interpreter's command line.
*/
int runPyMain()
{
	if (auto crashReporter = GetCrashReporter(); crashReporter) {
		// Signal to the crash reporting system that we're running in interpreter mode
		crashReporter->SetCrashKeyValue("interpreterMode", "true");
		// And this allows us to filter for these kinds of crashes in sentry.io
		// using snake_csae to be consistent with the other search able keys on sentry.
		crashReporter->SetCrashKeyValue("sentry", R"({"tags": {"interpreter_mode": "true"}})");
	}

	return Py_RunMain();
}

int Main(const CommandLine& commandLine, bool isSupportedOS)
{
    DumpCommandLineToDebugger( commandLine );
    CommandArguments commandArguments = GetCommandArguments( commandLine );

	BlueInterface blue;
	bool defaultedBlueFlavor = false;
	std::wstring flavor = commandArguments.buildFlavor;
	if( flavor == L"release" )
	{
		flavor = L"";
	}

	const std::vector<std::wstring> validBuildFlavors
	{
		L"", // Release
		L"internal",
		L"trinitydev",
		L"debug",
	};

	if( std::find( std::begin( validBuildFlavors ), std::end( validBuildFlavors ), flavor ) == std::end( validBuildFlavors ) )
	{
		flavor = L"";
		defaultedBlueFlavor = true;
	}

	if( !blue.LoadBlue( flavor ) )
	{
		fprintf( stderr, "Failed to load Blue flavor: '%S', trying default\n", flavor.c_str() );

		if( flavor.empty() || !blue.LoadBlue( L"" ) )
		{
			fprintf( stderr, "Failed to load release Blue flavor\n" );
			fflush( stderr );
			return FAILED_LOADING_BLUE_RELEASE_FLAVOUR;
		}

		defaultedBlueFlavor = true;
	}

	//Check for valid os and show localized platform specific error message dialog
	if ( !isSupportedOS )
	{
		blue.ShowInvalidOSVersionError();
		return 0;
	}

	auto crashReporter = GetCrashReporter();
	if( crashReporter && commandArguments.uploadMinidump )
	{
		crashReporter->InitializeCrashpad();
	}
	// Tell Blue about our crash interface so that it can change settings
	blue.SetCrashReporter( crashReporter );

	blue.ModuleStartup();
	blue.InitializeSocketLogger();
	ON_BLOCK_EXIT( [&]{ blue.ShutdownSocketLogger(); } );

	if( defaultedBlueFlavor )
	{
		blue.LogFuncChannel( CCP::GetModuleChannel(), CCP::LOGTYPE_ERR, 0, "Could not load Blue flavor '%S', defaulted to release flavor.", commandArguments.buildFlavor.c_str() );
	}
	else
	{
		blue.LogFuncChannel( CCP::GetModuleChannel(), CCP::LOGTYPE_NOTICE, 0, "Loaded Blue flavor '%S'", flavor.c_str() );
	}

#ifdef _WIN32
	HANDLE sdcHandle = 0;
	if (commandArguments.asService == true)
	{
		sdcHandle = CreateThread(NULL, 0, ServiceEntrypoint, NULL, 0, NULL);
		if (sdcHandle == NULL)
		{
			blue.LogFuncChannel( CCP::GetModuleChannel(), CCP::LOGTYPE_ERR, 0, "Failed to set up windows service ctrl." );
			return SUCCESS;
		}
	}
    
    // Set desired timer resolution to 1 millisecond. By default,
    // the granularity of timers is something like 15 milliseconds, which
    // makes sleeps on Windows take a lot more time than one would expect.
    // Microsoft documentation recommends doing this once at the application level
    // since changing this frequently can mess with the system clock, scheduler and power usage.
    // https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep
    TIMECAPS tc;
    UINT TARGET_RESOLUTION_MS{1};
    UINT resolutionMS{0};

    MMRESULT timeCapsError = timeGetDevCaps( &tc, sizeof( TIMECAPS ) );
    if( timeCapsError != TIMERR_NOERROR )
    {
        CCP_LOGERR( "Failed to set desired timer resolution (error: %u). Simulation will tick at a lower frequency", timeCapsError );
    }
    else
    {
        resolutionMS = std::min( std::max( tc.wPeriodMin, TARGET_RESOLUTION_MS ), tc.wPeriodMax );
        timeBeginPeriod( resolutionMS );
    }
    ON_BLOCK_EXIT( [resolutionMS] { if ( resolutionMS > 0 ) { timeEndPeriod( resolutionMS ); } } );
#endif

	//Initialize console and redirect stdoutput
	ShowConsoleWindow( commandArguments.consoleMode );
	if( commandArguments.redirectStdErr.size() )
	{
		RedirectOutput( stderr, commandArguments.redirectStdErr.c_str() );
	}
	if( commandArguments.redirectStdOut.size() )
	{
		RedirectOutput( stdout, commandArguments.redirectStdOut.c_str() );
	}

	PreStartupTest();

	blue.SetStartupArgs( commandLine );

	bool interpreterMode = blue.HasStartupArg( L"py" );

	std::wstring defaultPath = CcpGetCurrentWorkingDirectory();
	if( interpreterMode )
	{
		// Python interpreter mode must not assume that the current working directory contains
		// the expected relative paths passed from the varios *.args files.
		// Instead, we're constructing a path relative to /the/path/to/exefile.exe, e.g.:
		// c:/p4/eve/server/bin/x64/exefile.exe
		// -> c:/p4/eve/server/bin/x64/exefile.exe/../../../
		// -> c:/p4/eve/server
		defaultPath = CcpGetAbsolutePath(CcpExecutablePath() + L"/../../..");
	}
	blue.InitializePaths( defaultPath );
	if( !blue.SetSearchPaths( commandArguments.searchPaths ) )
	{
		blue.LogFuncChannel( CCP::GetModuleChannel(), CCP::LOGTYPE_ERR, 0, "Error setting search paths. You might have a circular reference." );
		blue.ShowError();
		return SEARCH_PATH_ARGUMENT_ERROR; // Search path argument error
	}

	if( commandArguments.affinity != -1 )
	{
		SetProcessAffinity( commandArguments.affinity );
	}

	// When we are going to run as the vanilla interpreter, Python is initialized with the interpreter's command line
	// up front, and runPyMain() only runs what it asked for.
	const bool runInterpreter = !blue.IsPackaged() && interpreterMode;
	std::vector<std::string> pythonArgv;
	if( runInterpreter )
	{
		pythonArgv = GetPythonArgv( commandLine );
	}

	if( !ConfigurePython( blue, interpreterMode, pythonArgv ) )
	{
		blue.ShowError();
		return PYTHON_INIT_ERROR; // Python initialization error code
	}

	// Now enter python interpreter mode, if we are not packaged, and interpreter mode flag is set
	if( runInterpreter )
	{
		int ret = runPyMain();
		// exit with the interpreter's failure exit code
		blue.Terminate(ret);
	}

	// Now, enter stackless and continue running from there.  This allows stackless to initialize
	// the main tasklet.
	if( !blue.RunStackless() )
	{
		blue.ShowError();
		return STACKLESS_INIT_ERROR;
	}

#ifdef _WIN32
	blue.LogFuncChannel( CCP::GetModuleChannel(), CCP::LOGTYPE_NOTICE, 0, "Windows Service Stop Event Triggered" );
	SetEvent(g_ServiceStopEvent);
	WaitForSingleObject(sdcHandle, 5000);
	CloseHandle(sdcHandle);
#endif

	// Normally the RunStackless function will terminate the app before even
	// reaching here.  But just in case that doesn't happen we call Terminate,
	// it's the only way to be sure :)
	blue.Terminate();

	// Stop the compiler from complaining - we won't get here
	return 0;
}
