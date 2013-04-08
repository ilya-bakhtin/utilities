// tormgr.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols


// CtormgrApp:
// See tormgr.cpp for the implementation of this class
//

class CtormgrApp : public CWinApp
{
public:
	CtormgrApp();

// Overrides
public:
	virtual BOOL InitInstance();
	bool IsServer() {return isServer;}
	bool IsNightMode() {return night_mode;}
	unsigned long ServerIP() {return ip_addr;}

// Implementation
protected:
	bool			isServer;
	bool			night_mode;
	unsigned long	ip_addr;

	DECLARE_MESSAGE_MAP()
};

extern CtormgrApp theApp;