// Copyright © 2014 CCP ehf.
// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently
#define STRICT
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers


// comment this out if you want python
//#define NOPYTHON

#if (defined(_WIN32) && _MSC_VER < 1400 && !_DLL)
#define NOSTDEXCEPT
#endif

#ifdef NOSTDEXCEPT
// Not using c++ exceptions
#define _HAS_EXCEPTIONS 0
#if _MSC_VER < 1400
#include <exception>
using std::exception;
#endif
#endif

#define NOMINMAX

#ifdef _WIN32
#include <windows.h>
#endif

// STL
#include <map>
#include <string>
#include <vector>

// carbon-core
#include <CcpCore.h>

// carbon-log
#include <CcpLog.h>

// Python
#include <Python.h>

#ifdef __APPLE__
#ifdef toupper
#undef toupper
#endif
#ifdef tolower
#undef tolower
#endif
#ifdef isalnum
#undef isalnum
#endif
#ifdef isalpha
#undef isalpha
#endif
#ifdef islower
#undef islower
#endif
#ifdef isspace
#undef isspace
#endif
#ifdef isupper
#undef isupper
#endif
#endif
