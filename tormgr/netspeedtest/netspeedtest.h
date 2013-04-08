// netspeedtest.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols


// CnetspeedtestApp:
// See netspeedtest.cpp for the implementation of this class
//

class CnetspeedtestApp : public CWinApp
{
public:
	CnetspeedtestApp();

// Overrides
public:
	virtual BOOL InitInstance();
	bool IsServer() {return isServer;}
	unsigned long ServerIP() {return ip_addr;}
	unsigned short ServerPort() {return port;}

// Implementation
protected:
	bool			isServer;
	unsigned long	ip_addr;
	unsigned short	port;

	DECLARE_MESSAGE_MAP()
};

extern CnetspeedtestApp theApp;