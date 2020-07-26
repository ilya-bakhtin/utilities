// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <windows.h>

#ifdef UNICODE
#define tstring std::wstring
#else
#define tstring std::string
#endif

// TODO: reference additional headers your program requires here
