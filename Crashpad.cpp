// Copyright © 2020 CCP ehf.
#include "StdAfx.h"

#if !_DEBUG

#include "Crashpad.h"
#include "FileSystem.h"

#include "client/annotation.h"
#include "client/crashpad_client.h"
#include "client/crash_report_database.h"
#include "client/settings.h"

#if _WIN32

auto executablePath = L"eve_crashmon.exe";

#elif __APPLE__

auto executablePath = "eve_crashmon";

#endif

// Data for an annotation isn't allowed to move around after created, so we use this wrapper class
class AnnotationData
{
public:
	AnnotationData( const char* name, const char* value )
	{
		memset( m_name, 0, sizeof( m_name ) );
		memset( m_value, 0, sizeof( m_value ) );
		
		strncpy_s( m_name, sizeof( m_name ), name, strlen( name ) );
		
		m_annotation = std::make_unique<crashpad::Annotation>( crashpad::Annotation::Type::kString, m_name, m_value );
		SetValue( value );
	}
	
	AnnotationData( const AnnotationData& ) = delete;
	AnnotationData& operator=( const AnnotationData& ) = delete;
	
	~AnnotationData()
	{
		m_annotation->Clear();
	}
	
	void SetValue( const char* value )
	{
		size_t len = strlen( value );
		memset( m_value, 0, strlen( m_value ) );
		strncpy_s( m_value, sizeof( m_value ), value, len );
		
		m_annotation->SetSize( static_cast<crashpad::Annotation::ValueSizeType>( len ) );
	}
	
private:
	char m_name[crashpad::Annotation::kNameMaxLength];
	char m_value[crashpad::Annotation::kValueMaxSize];
	std::unique_ptr<crashpad::Annotation> m_annotation;
};

bool CrashpadCrashInterface::InitializeCrashpad()
{
	auto appdata = GetAppdataFolder();
	if( appdata.empty() )
	{
		return false;
	}

    std::wstring logsFolder = GetLogsFolder();
    if( logsFolder.empty() )
    {
        fprintf( stderr, "Failed to find folder for crashpad logs" );
        return false;
    }
    if( !CreateDirectoryRec(logsFolder.c_str()) )
    {
        fprintf( stderr, "Failed to create folder for crashpad logs" );
        return false;
    }


	
#ifdef __APPLE__
	std::string exePath = (const char*)CW2A( CcpExecutablePath().c_str() );
	base::FilePath handler(exePath);
	handler = handler.DirName().Append( executablePath );
	std::string sLogsFolder = (const char*)CW2A( logsFolder.c_str() );
	base::FilePath reportsDir( sLogsFolder );
#elif _WIN32
	std::wstring exePath = CcpExecutablePath();
	base::FilePath handler( exePath );
	handler = handler.DirName().Append( executablePath );
	base::FilePath reportsDir( logsFolder );
#else
#error Unsupported platform
#endif
	base::FilePath metricsDir( reportsDir );
	std::string url = "https://sentry.io/api/1434648/minidump/?sentry_key=0b0785270cff40ab88073f3429a81eb4";
	std::map<std::string, std::string> annotations;
	std::vector<std::string> arguments = { "--no-rate-limit" };

	m_database = crashpad::CrashReportDatabase::Initialize( reportsDir );
	if( !m_database )
	{
		return false;
	}

	// Enable automated crash uploads
	m_settings = m_database->GetSettings();
	if( !m_settings )
	{
		return false;
	}
	m_settings->SetUploadsEnabled( m_enabled );

	m_client = std::make_unique<crashpad::CrashpadClient>();
	return m_client->StartHandler( handler, reportsDir, metricsDir, url, annotations, arguments, true, false );
}

void CrashpadCrashInterface::EnableCrashReporting( bool enable )
{
	m_enabled = enable;
	if( m_settings )
	{
		m_settings->SetUploadsEnabled( enable );
	}
}

void CrashpadCrashInterface::SetCrashKeyValue( const char* key, const char* val )
{
	if( !m_settings )
	{
		return;
	}
	auto found = m_annotations.find( key );
	if( found != m_annotations.end() )
	{
		found->second->SetValue( val );
	}
	else
	{
		m_annotations.emplace( key, std::make_unique<AnnotationData>( key, val ) );
	}
}

bool CrashpadCrashInterface::IsCrashReportingEnabled()
{
	return m_enabled;
}

void CrashpadCrashInterface::ProduceImmediateDump()
{
	if( m_client )
	{
#if _WIN32
		CONTEXT context{};
		context.ContextFlags = CONTEXT_ALL;
		RtlCaptureContext( &context );
		m_client->DumpWithoutCrash( context );
#endif
	}
}

CrashpadCrashInterface* GetCrashReporter()
{
#if !_DEBUG
	static CrashpadCrashInterface s_crashpadCrashInterface;
	return &s_crashpadCrashInterface;
#else
	return nullptr;
#endif
}

#endif