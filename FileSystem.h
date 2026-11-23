// Copyright © 2021 CCP ehf.
#pragma once

#if __APPLE__ || __linux__
std::string GetAppdataFolder();
#elif _WIN32
std::wstring GetAppdataFolder();
#else
#error Unsupported platform
#endif

bool CreateDirectoryRec(const wchar_t* dir);
const std::wstring& GetLogsFolder();
