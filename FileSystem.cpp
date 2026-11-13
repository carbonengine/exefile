#include "StdAfx.h"
#if __APPLE__ || __linux__
#include "FileSystem.h"

#include <errno.h>
#include <sys/stat.h>

bool CreateDirectoryRec( const wchar_t* dir )
{
	if ( !mkdir( CW2A( dir ), ACCESSPERMS ) )
    {
        return true;
    }
	if (errno == EEXIST)
    {
        return true;
    }
	// find the last separator
	std::wstring full( dir );
	std::wstring::size_type s = full.find_last_of(L"/\\");
	if (s != std::wstring::npos)
    {
        std::wstring pre = full.substr(0, s);
        if (!CreateDirectoryRec(pre.c_str()))
        {
            return false;
        }
        return !mkdir( CW2A( dir ), ACCESSPERMS );
    }
    return false;
}
const std::wstring& GetLogsFolder()
{
	static std::wstring result;
	if( result.empty() )
	{
		std::wstring appDataFolder = (const wchar_t*)CA2W( GetAppdataFolder().c_str() );
		if( !appDataFolder.empty() )
		{
			result = appDataFolder + L"/CCP/EVE";
		}
	}
	return result;
}

#endif