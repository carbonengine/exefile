// Copyright © 2021 CCP ehf.
#include "StdAfx.h"
#include "BlueInterface.h"

#if __APPLE__ || __linux__
#include <dlfcn.h>
#endif


BlueInterface::~BlueInterface()
{
	if( m_module )
	{
#if __APPLE__ || __linux__
		dlclose( m_module );
#elif _WIN32
		FreeLibrary( static_cast<HMODULE>( m_module ) );
#else
#error Unsupported platform
#endif
		// Make invalid access obvious
		memset( this, 0, sizeof( *this ) );
	}
}

bool BlueInterface::LoadBlue( const std::wstring& buildFlavor )
{
	if( m_module )
	{
		return true;
	}

	std::wstring name = L"blue" + ( buildFlavor.empty() ? L"" : L"_" + buildFlavor );

#if __APPLE__
	std::string str = std::string( std::begin( name ), std::end( name ) ) + ".so";
	m_module = dlopen( str.c_str(), RTLD_LAZY );
#elif __linux__
	std::string str = "./" + std::string( std::begin( name ), std::end( name ) ) + ".so";
	m_module = dlopen( str.c_str(), RTLD_LAZY );
#elif _WIN32
	name += L".pyd";
	m_module = LoadLibraryW( name.c_str() );
#else
#error Unsupported platform
#endif

	if( !m_module )
	{
		return false;
	}

#if __APPLE__ || __linux__
#define LoadBlueRoutine( name )                                                                                                \
	if( !( m_blue##name##Routine = reinterpret_cast<Blue##name##Routine>( dlsym( m_module, CCP_STRINGIZE( Blue##name ) ) ) ) ) \
	{                                                                                                                          \
        fprintf(stderr, "Failed to resolve %s\n", CCP_STRINGIZE( Blue##name ) );                                               \
		return false;                                                                                                          \
	}
#elif _WIN32
#define LoadBlueRoutine( name )                                                                                                                                 \
	if( !( m_blue##name##Routine = reinterpret_cast<Blue##name##Routine>( GetProcAddress( static_cast<HMODULE>( m_module ), CCP_STRINGIZE( Blue##name ) ) ) ) ) \
	return false
#else
#error Unsupported platform
#endif
	LoadBlueRoutine( SetCrashReporter );
	LoadBlueRoutine( LogFuncChannel );
	LoadBlueRoutine( ModuleStartup );
	LoadBlueRoutine( InitializeSocketLogger );
	LoadBlueRoutine( InitializePaths );
	LoadBlueRoutine( ShowInvalidOSVersionError );
	LoadBlueRoutine( ShutdownSocketLogger );
	LoadBlueRoutine( InstallPythonMemoryHooks );
	LoadBlueRoutine( LoadPythonExtension );
	LoadBlueRoutine( ConstructPathListFromManifest );
	LoadBlueRoutine( ResolvePathForWritingW );
	LoadBlueRoutine( GetInitTab );
	LoadBlueRoutine( SetStartupArgs );
	LoadBlueRoutine( HasStartupArg );
	LoadBlueRoutine( SetSearchPaths );
	LoadBlueRoutine( IsPackaged );
	LoadBlueRoutine( Terminate );
	LoadBlueRoutine( RunStackless );
	LoadBlueRoutine( ShowError );
#undef LoadBlueRoutine

	return true;
}

void BlueInterface::SetCrashReporter( ICrashReporter* crashReporter ) const
{
	m_blueSetCrashReporterRoutine( crashReporter );
}

void BlueInterface::LogFuncChannel( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* format, ... ) const
{
	va_list args;
	va_start( args, format );
	m_blueLogFuncChannelRoutine( logObject, type, userData, format, args );
	va_end( args );
}

void BlueInterface::ModuleStartup() const
{
	m_blueModuleStartupRoutine();
}

void BlueInterface::InitializeSocketLogger() const
{
	m_blueInitializeSocketLoggerRoutine();
}

bool BlueInterface::InitializePaths( const std::wstring& initialPath ) const
{
	return m_blueInitializePathsRoutine( initialPath );
}

void BlueInterface::ShowInvalidOSVersionError() const
{
	m_blueShowInvalidOSVersionErrorRoutine();
}


void BlueInterface::ShutdownSocketLogger() const
{
	m_blueShutdownSocketLoggerRoutine();
}

void BlueInterface::InstallPythonMemoryHooks() const
{
	m_blueInstallPythonMemoryHooksRoutine();
}

PyObject* BlueInterface::LoadPythonExtension( const char* name ) const
{
	return m_blueLoadPythonExtensionRoutine(name);
}

bool BlueInterface::ConstructPathListFromManifest( std::vector<std::wstring>& pathList, bool verifyManifest )
{
	return m_blueConstructPathListFromManifestRoutine( pathList, verifyManifest );
}

void BlueInterface::GetInitTab( std::vector<_inittab>& inittab )
{
	m_blueGetInitTabRoutine( inittab );
}

std::wstring BlueInterface::ResolvePathForWritingW( const std::wstring& path )
{
	std::wstring resolved;
	m_blueResolvePathForWritingWRoutine( path, resolved );
	return resolved;
}

bool BlueInterface::IsPackaged()
{
	return m_blueIsPackagedRoutine();
}

bool BlueInterface::RunStackless()
{
	return m_blueRunStacklessRoutine();
}

bool BlueInterface::HasStartupArg( const std::wstring& name )
{
	return m_blueHasStartupArgRoutine( name );
}

void BlueInterface::SetStartupArgs( const std::vector<std::wstring>& args )
{
	m_blueSetStartupArgsRoutine( args );
}

bool BlueInterface::SetSearchPaths( const std::vector<std::wstring>& searchPaths )
{
	return m_blueSetSearchPathsRoutine( searchPaths );
}

void BlueInterface::ShowError() const
{
	m_blueShowErrorRoutine();
}

void BlueInterface::Terminate( int exitCode )
{
	m_blueTerminateRoutine( exitCode );
}
