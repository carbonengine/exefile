// Copyright © 2015 CCP ehf.
#pragma once
#ifndef CommandArguments_H
#define CommandArguments_H

enum ConsoleMode
{
	console_mode_off,		//deprecated: Neither attach nor create.
	console_mode_inherit,   //attach to whatever you have access to
	console_mode_create,	//create a new console.
};

struct CommandArguments
{
	// Search paths - add an entry with /path:key=value
	std::vector<std::wstring> searchPaths;
	ConsoleMode consoleMode; // console_mode_create
	bool uploadMinidump; // = true
	int affinity; // = -1
	int pyOptimize;// = 1 or = -1 for _DEBUG
	std::wstring redirectStdErr;
	std::wstring redirectStdOut;
	bool asService;
	std::wstring buildFlavor;

	CommandArguments()
		:consoleMode( console_mode_create ),
		uploadMinidump( true ),
		affinity( -1 ),
#ifdef _DEBUG
		pyOptimize( -1 ),
#else
		pyOptimize( 1 ),
#endif
		asService( false )
	{
	}
};

typedef std::vector<std::wstring> CommandLine;

#if _WIN32
CommandLine ParseCommandLine();
#elif __APPLE__ || __linux__
CommandLine ParseCommandLine( int argc, char* argv[] );
#endif

void DumpCommandLineToDebugger( const CommandLine& commandLine );
CommandArguments GetCommandArguments( const CommandLine& commandLine );

#endif