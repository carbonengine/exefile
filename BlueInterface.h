// Copyright © 2021 CCP ehf.
class BlueInterface
{
public:
	~BlueInterface();

	bool LoadBlue( const std::wstring& buildFlavor );

	void SetCrashReporter( ICrashReporter* crashReporter ) const;
	void LogFuncChannel( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* format, ... ) const;
	void ModuleStartup() const;
	void InitializeSocketLogger() const;
	bool InitializePaths( const std::wstring& initialPath ) const;
	void ShutdownSocketLogger() const;
	void InstallPythonMemoryHooks() const;
	PyObject* LoadPythonExtension( const char* name ) const;
	void ShowInvalidOSVersionError() const;
	void ShowError() const;
	void Terminate( int exitCode = 0 );
	bool RunStackless();
	bool IsPackaged();
	bool SetSearchPaths( const std::vector<std::wstring>& searchPaths );
	void SetStartupArgs( const std::vector<std::wstring>& args );
	bool HasStartupArg( const std::wstring& name );
	void GetInitTab( std::vector<_inittab>& inittab );
	bool ConstructPathListFromManifest( std::vector<std::wstring>& pathList, bool verifyManifest );
	std::wstring ResolvePathForWritingW( const std::wstring& path );

	using BlueSetCrashReporterRoutine = void( * )( ICrashReporter* );
	using BlueLogFuncChannelRoutine = void( * )( CcpLogChannel_t&, CCP::LogType, unsigned long, const char*, va_list );
	using BlueModuleStartupRoutine = void( * )();
	using BlueInitializeSocketLoggerRoutine = void( * )();
	using BlueInitializePathsRoutine = bool( * )( const std::wstring& );
	using BlueShutdownSocketLoggerRoutine = void( * )();
	using BlueInstallPythonMemoryHooksRoutine = void(* )();
	using BlueLoadPythonExtensionRoutine = PyObject*(*) ( const char* );
	using BlueShowInvalidOSVersionErrorRoutine = void( * )();
	using BlueShowErrorRoutine = void( * )();
	using BlueTerminateRoutine = void( * )( int );
	using BlueRunStacklessRoutine = bool( * )();
	using BlueIsPackagedRoutine = bool( * )();
	using BlueSetSearchPathsRoutine = bool( * )( const std::vector<std::wstring>& );
	using BlueSetStartupArgsRoutine = void( * )( const std::vector<std::wstring>& );
	using BlueHasStartupArgRoutine = bool( * )( const std::wstring& name );
	using BlueGetInitTabRoutine = void( * )( std::vector<struct _inittab>& );
	using BlueResolvePathForWritingWRoutine = void( * )( const std::wstring&, std::wstring& );
	using BlueConstructPathListFromManifestRoutine = bool( * )( std::vector<std::wstring>&, bool );

private:
	BlueSetCrashReporterRoutine m_blueSetCrashReporterRoutine = nullptr;
	BlueLogFuncChannelRoutine m_blueLogFuncChannelRoutine = nullptr;
	BlueModuleStartupRoutine m_blueModuleStartupRoutine = nullptr;
	BlueInitializeSocketLoggerRoutine m_blueInitializeSocketLoggerRoutine = nullptr;
	BlueInitializePathsRoutine m_blueInitializePathsRoutine = nullptr;
	BlueShutdownSocketLoggerRoutine m_blueShutdownSocketLoggerRoutine = nullptr;
	BlueInstallPythonMemoryHooksRoutine m_blueInstallPythonMemoryHooksRoutine = nullptr;
	BlueLoadPythonExtensionRoutine m_blueLoadPythonExtensionRoutine = nullptr;
	BlueShowInvalidOSVersionErrorRoutine m_blueShowInvalidOSVersionErrorRoutine = nullptr;
	BlueShowErrorRoutine m_blueShowErrorRoutine = nullptr;
	BlueTerminateRoutine m_blueTerminateRoutine = nullptr;
	BlueRunStacklessRoutine m_blueRunStacklessRoutine = nullptr;
	BlueIsPackagedRoutine m_blueIsPackagedRoutine = nullptr;
	BlueSetSearchPathsRoutine m_blueSetSearchPathsRoutine = nullptr;
	BlueSetStartupArgsRoutine m_blueSetStartupArgsRoutine = nullptr;
	BlueHasStartupArgRoutine m_blueHasStartupArgRoutine = nullptr;
	BlueGetInitTabRoutine m_blueGetInitTabRoutine = nullptr;
	BlueResolvePathForWritingWRoutine m_blueResolvePathForWritingWRoutine = nullptr;
	BlueConstructPathListFromManifestRoutine m_blueConstructPathListFromManifestRoutine = nullptr;

	void *m_module = nullptr;
};
