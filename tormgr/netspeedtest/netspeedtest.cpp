// netspeedtest.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "netspeedtest.h"
#include "netspeedtestdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CnetspeedtestApp

BEGIN_MESSAGE_MAP(CnetspeedtestApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


// CnetspeedtestApp construction

CnetspeedtestApp::CnetspeedtestApp() :
	isServer(false),
	ip_addr(INADDR_NONE),
	port(0xffff)
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CnetspeedtestApp object

CnetspeedtestApp theApp;


// CnetspeedtestApp initialization

BOOL CnetspeedtestApp::InitInstance()
{
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("IT Solutions"));

	if (__argc >= 2 && _tcslen(__targv[1]) == 2 && _tcscmp(__targv[1], _T("/s")) == 0)
		isServer = true;
	if (__argc >= 2 && !isServer)
	{
		CStringA a(__targv[1]);
		ip_addr = inet_addr(a);
	}

	if (ip_addr == INADDR_NONE)
		ip_addr = inet_addr("192.168.8.3");

	if (port == 0xffff)
		port = 15313;

	CNetSpeedTestDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
