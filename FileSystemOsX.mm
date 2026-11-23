#include "StdAfx.h"
#if __APPLE__

#include "FileSystem.h"
#include <Foundation/Foundation.h>

std::string GetAppdataFolder()
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory, NSUserDomainMask, YES );
    return paths.count && paths.firstObject ? [paths.firstObject UTF8String] : "";
}

#endif