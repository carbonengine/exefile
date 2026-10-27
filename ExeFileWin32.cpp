// Copyright © 2015 CCP ehf.
#include "StdAfx.h"
#ifdef _WIN32
#include "ExeFile.h"
#include <CcpCrash.h>
#include <fcntl.h>
#include <Shlobj.h>
#include <VersionHelpers.h>

namespace
{

DWORD g_mainThreadId;
bool g_quit = false;

//Note, we might want to change this to produce always english
//messages...
std::string FmtError(DWORD err = 0)
{
	DWORD olderr = GetLastError();
	if (!err)
		err = olderr;

	LPVOID lpMsgBuf;
	DWORD ok = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL );

	if (!ok && GetLastError() == ERROR_RESOURCE_LANG_NOT_FOUND)
	{
		//try again using default language
		ok = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		0,
		(LPTSTR) &lpMsgBuf,
		0,
		NULL );
	}

	if (!ok)
	{
		SetLastError(olderr);
		return "";
	}

	std::string r((LPTSTR)lpMsgBuf);
	LocalFree( lpMsgBuf );
	SetLastError(olderr);
	return r;
}

void myInvalidParameterHandler(const wchar_t* expression,
   const wchar_t* function,
   const wchar_t* file,
   unsigned int line,
   uintptr_t pReserved)
{
	CCP_LOGERR( "ExeFile CRT invalid parameter handler: %s:%d in %s",
		file?(const char*)CW2A(file):"<none>", line, function?(const char*)CW2A(function):"<none>");
	CCP_LOGERR( "Expression: %s", expression?(const char*)CW2A(expression):"<none>");
}

void PureCallHandler()
{
    CcpCrashOnPurpose();
	// Crash the process instead of showing the default MS "pure virtual call" dialog
	volatile int* crashPointer = nullptr;
	*crashPointer = 42;
}

BOOL WINAPI OnConsoleCtrl(DWORD type)
{
	// let's quit no matter the type
	g_quit = true;
	PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
	return TRUE;
}

}


//--------------------------------------------------------------------
// ShowConsoleWindow
// ExeFile is a WindowsApplication.  But we sometimes want a console,
// and in particular, we want to run the servers as regular console
// applications.  This function attemts to emulate the services
// normally provided by the system when starting a "console" app.
// When CreateProcess starts a non-console application, it doesn't
// heed the CREATE_NEW_CONSOLE flag.
// And DETACHED_PROCESS doesn't prevent the process from attaching
// to the parent process...  Therefore, to emulate a console app,
// one has to be reasonable.
//--------------------------------------------------------------------

void ShowConsoleWindow(ConsoleMode mode)
{
	if( mode == console_mode_off )
	{
		// Completely bypassing any mucking around with IO handles.
		// This tended to break Mac Remote Debugging.
		return;
	}

	//first, look at those std handles that we want to keep.  Allocating
	//or attaching to a console will trample these handles that we were given
	//by CreateProcess.  C stdio has already been initialized to use these
	//handles.
	DWORD hKeys[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
	HANDLE keep[3];
	for( int i = 0; i<3; i++ )
	{
		//We detect those file redirection handles by our ability to set
		//their inheritance flags.  Console handles can't do that.
		//(cmd.exe will pass on _its_ stdio handles, even if they are to
		//a console, and they won't work for us).
		HANDLE h = GetStdHandle(hKeys[i]);
		if (h && SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
		{
			keep[i] = h;
		}
		else
		{
			keep[i] = 0;
		}
	}

	//Now, Attach to or Create a console, depending on our mode.
	BOOL console;
	switch (mode)
	{
		case console_mode_off:
			console = FALSE;
			break;
		case console_mode_inherit:
			{
				//dynamic loading of AttachConsole, which is supported only on XP and later
				console = FALSE;
				HMODULE h = GetModuleHandle("kernel32");
				if( h )
				{
					typedef BOOL (__stdcall *at)(DWORD);
					at ft = (at)GetProcAddress(h, "AttachConsole");
					if( ft )
					{
						console = (*ft)(-1);
					}
				}
				break;
			}
		case console_mode_create:
			console = AllocConsole();
			break;
	}

	if( console )
	{
		//if we want to keep any handles, reinstate those handles now,
		//else, reopen stdio to the new handles from console alloc/attach
		//(Maybe we should close those handles that we trample but lets
		//not worry about that)
		if( mode == console_mode_inherit )
		{
			for( int i = 0; i<3; ++i )
			{
				if( keep[i] )
				{
					SetStdHandle( hKeys[i], keep[i] );
				}
			}
		}

		//reopen stdio for those handles that we didn't want to keep from before
		FILE *si, *so, *se;
		if( mode == console_mode_create || !keep[0] )
		{
			freopen_s(&si, "CONIN$", "r", stdin);
		}
		if( mode == console_mode_create || !keep[1] )
		{
			freopen_s(&so, "CONOUT$", "w", stdout);
		}
		if( mode == console_mode_create || !keep[2] )
		{
			freopen_s(&se, "CONOUT$", "w", stderr);
		}

	}
	if( mode != console_mode_off )
	{
		//We are pretending to be a console app

		// Set the output channel of the CRT error handler to stderr
		// This will prevent abort() and assert() from putting up a
		// dialogue box. (default for win app is a dialog box)
		_set_error_mode(_OUT_TO_STDERR);

		// Change abort behaviour to not invoke DRWatson for a dump when
		// abort() is called.  Turn off the _CALL_REPORTFAULT flag.
		_set_abort_behavior(0, _CALL_REPORTFAULT);
	}
}

void PreStartupTest()
{
	SetConsoleCtrlHandler( OnConsoleCtrl, TRUE );

	if(!IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
	{
		MessageBox( NULL,
					"SSE2 support is needed to run this game.\nClick \'OK\' to continue but the game will probably shut down.",
					"SSE2 needed",
					MB_ICONEXCLAMATION | MB_OK );
	}
}

void SetProcessAffinity( int affinity )
{
	int mask = affinity;
	HANDLE me = GetCurrentProcess();
	HMODULE kernel32 = LoadLibrary( "kernel32" );

	typedef WINBASEAPI BOOL WINAPI maskfn( IN HANDLE process, IN DWORD_PTR mask );
	maskfn* proc = (maskfn*)GetProcAddress( kernel32, "SetProcessAffinityMask" );

	if( me && kernel32 && proc && proc( me, mask ) )
	{
		CCP_LOG( "Process affinity mask is %d", mask );
	}
	else
	{
		CCP_LOGERR(
			"Cannot set affinity mask, mask=%d, me=%p, kernel32=%p, proc=%p, err=%d:\"%s\"",
			mask, me, kernel32, proc, GetLastError(), FmtError().c_str()
			);
	}

	if( kernel32 )
	{
		FreeLibrary( kernel32 );
	}
}

bool IsSupportedOSVersion()
{
	return IsWindows10OrGreater();
}

int APIENTRY WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
{
	//NOTE! This is only local to Exefile as long as we don't use DLL RCT
	_set_invalid_parameter_handler(myInvalidParameterHandler);

	_set_purecall_handler( &PureCallHandler );

	g_mainThreadId = GetCurrentThreadId();

	CommandLine commandLine = ParseCommandLine();
	int retcode = Main( commandLine, IsSupportedOSVersion() );


	// If the game was launched from Media Center, find the window and restore
	// (possibly the game wasn't launched from MCE even though it's running, in
	// that case it's restored anyway)
	HWND hwndMCE = FindWindow( _T("eHome Render Window"), NULL );
	if( hwndMCE )
	{
		ShowWindow( hwndMCE, SW_RESTORE );
	}

	return retcode;
}

#endif
