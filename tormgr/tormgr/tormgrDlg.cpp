// TorMgrDlg.cpp : implementation file
//

#include "stdafx.h"
#include "tormgr.h"
#include "torsupervisor.h"
#include "TorMgrDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static UINT WSA_Msg = ::RegisterWindowMessage(_T("OnSocketEventMessage"));
static UINT TorMgr_Msg = ::RegisterWindowMessage(_T("TorMgrMessage"));

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// CTorMgrDlg dialog

CTorMgrDlg::CTorMgrDlg(CWnd* pParent /*=NULL*/) :
	CDialog(CTorMgrDlg::IDD, pParent),
	timerID(0),
	tstatus(unknown),
	last_clock(-1),
	last_man_time(CTime::GetCurrentTime()-CTimeSpan(0, 24, 0, 0)),
	deadtime(-1),
	close_request_time(CTime::GetCurrentTime()-CTimeSpan(0, 24, 0, 0)),
	astate(a_normal),
	SrvSocket(INVALID_SOCKET),
	FakeTorSrvSocket(INVALID_SOCKET),
	connecting(false)
{
//	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	if (theApp.IsServer())
		m_hIcon = AfxGetApp()->LoadIcon(IDI_TORMGR_S);
	else
		m_hIcon = AfxGetApp()->LoadIcon(IDI_TORMGR_C);
}

void CTorMgrDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTorMgrDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_NCDESTROY()
	ON_WM_QUERYDRAGICON()
	ON_REGISTERED_MESSAGE(WSA_Msg, OnSocketEvent)
	ON_REGISTERED_MESSAGE(TorMgr_Msg, OnTorMgr_Msg)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_START_STOP, OnBnClickedStartStop)
END_MESSAGE_MAP()


// CTorMgrDlg message handlers

BOOL CTorMgrDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	try
	{
		if (theApp.IsServer())
		{
			InitSrvSocket(SrvSocket, 15303);
			SetDlgItemText(IDC_CONNECTION_STATUS, _T(""));
		}
		else
		{
			SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTING);
		}
	}
	catch (TCHAR *e_str)
	{
		AfxMessageBox(e_str);
		EndDialog(IDCANCEL);
		return TRUE;
	}

	SetDlgItemText(IDC_FAKE_CONN_NO, _T(""));
	SetDlgItemTextI(IDC_STATUS_STATIC, IDS_UNKNOWN);

	GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);

	int timeout = (theApp.IsServer()?1:10)*1000;
	timerID = SetTimer(1, timeout, 0);
	if (timerID == 0)
	{
		CString s;
		s.LoadString(IDS_TIMER_UNAVAIL);
		AfxMessageBox(s, MB_OK|MB_ICONSTOP);
		EndDialog(IDCANCEL);
		return TRUE;
	}
	OnTimer(1);

	GetDlgItem(IDC_STATUS_STATIC)->SetFocus();

	return FALSE;  // return TRUE  unless you set the focus to a control
}

void CTorMgrDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTorMgrDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTorMgrDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTorMgrDlg::OnNcDestroy()
{
	if (timerID != 0)
		KillTimer(timerID);
	super::OnNcDestroy();
}

void CTorMgrDlg::SetDlgItemTextI(int nID, int strID)
{
	CString str;
	str.LoadString(strID);
	SetDlgItemText(nID, str);
}

void CTorMgrDlg::BroadcastStatus()
{
	char buf[] = {
		0,
		astate,
		tstatus
	};

	Sockets.Broadcast(buf, sizeof(buf));
}

void CTorMgrDlg::SetIndicators(int i_astate, int i_tstatus)
{
	tstatus = i_tstatus;
	astate =  i_astate;

	GetDlgItem(IDC_START_STOP)->EnableWindow(tstatus == started || tstatus == stopped);
	if (astate != a_normal)
		SetDlgItemText(IDC_START_STOP, _T(""));
	else
		SetDlgItemTextI(IDC_START_STOP, (tstatus == started)?IDS_STOP:IDS_START);


	if (astate != a_normal)
		SetDlgItemTextI(IDC_STATUS_STATIC, astate==a_starting?IDS_STARTING:IDS_STOPPING);
	else
	{
		if (tstatus==stopping)
			SetDlgItemTextI(IDC_STATUS_STATIC, IDS_STOPPING);
		else
			SetDlgItemTextI(IDC_STATUS_STATIC, tstatus==started?IDS_STARTED:IDS_STOPPED);
	}
}

void CTorMgrDlg::SrvOnTimer()
{
	int clk = clock();
	int td = clk - last_clock;

	if (astate == a_normal && last_clock > 0 && td < 30 * 1000)
		return;

	if ((astate == a_starting || astate == a_stopping) && clk > deadtime)
	{
		astate = a_normal;
		deadtime = -1;
	}

	last_clock = clk;

	try
	{
		TorSupervisor	ts;
		bool			tor_in_wind = false;

		bool running = ts.isTorRunning(&tor_in_wind);

		int status;

		if (running)
		{
			tstatus = tor_in_wind?started:stopping;
			status = tor_in_wind?IDS_STARTED:IDS_STOPPING;
		}
		else
		{
			tstatus = stopped;
			status = IDS_STOPPED;

			if (FakeTorSrvSocket == INVALID_SOCKET)
			{
				try
				{
					InitSrvSocket(FakeTorSrvSocket, 12566);
				}
				catch (...)
				{
					closesocket(FakeTorSrvSocket);
					FakeTorSrvSocket = INVALID_SOCKET;
				}
			}
		}

		if ((astate == a_starting && tstatus == started) || (astate == a_stopping && tstatus == stopped))
			astate = a_normal;


		if (astate == a_normal)
		{
			SetDlgItemTextI(IDC_STATUS_STATIC, status);
			GetDlgItem(IDC_START_STOP)->EnableWindow(tor_in_wind || !running);
			SetDlgItemTextI(IDC_START_STOP, tor_in_wind?IDS_STOP:IDS_START);
		}

		CTime tim = CTime::GetCurrentTime();
		int h = tim.GetHour();
		int d = tim.GetDay();

#define end_night_hour 8

		if (theApp.IsNightMode())
		{
			if (h >= end_night_hour && last_man_time.GetDay() != d)
			{
				last_man_time = CTime::GetCurrentTime()-CTimeSpan(1, 0, 0, 0);
				StartStop(false, false);
			}
		}

		if (!running && (!theApp.IsNightMode() || h < end_night_hour || last_man_time.GetDay() == d))
		{
			CTimeSpan t = tim - close_request_time;
			int minutes = h <= end_night_hour ? 30 : 60;
			if (t.GetTotalMinutes() > minutes)
				startTor();
		}
	}
	catch (TCHAR *e_str)
	{
		SetDlgItemText(IDC_START_STOP, _T(""));
		GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);
		AfxMessageBox(e_str);
	}
	BroadcastStatus();
}

void CTorMgrDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != 1)
		return;

	if (theApp.IsServer())
		SrvOnTimer();
	else
	{
		if (Sockets.size() == 0)
			InitClntSocket();
	}
}

void CTorMgrDlg::startTor()
{
	closesocket(FakeTorSrvSocket);
	FakeTorSrvSocket = INVALID_SOCKET;

	PROCESS_INFORMATION piProcInfo;
	memset(&piProcInfo, 0, sizeof(piProcInfo));

	STARTUPINFO siStartInfo;
	siStartInfo.cb			= sizeof(STARTUPINFO);
	siStartInfo.lpReserved	= NULL;
	siStartInfo.lpDesktop	= NULL;
	siStartInfo.lpTitle		= NULL;
	siStartInfo.cbReserved2	= 0;
	siStartInfo.lpReserved2	= NULL;
	siStartInfo.wShowWindow	= SW_SHOWNORMAL;

//	TCHAR cmd[] = _T("C:\\Program Files\\uTorrent\\uTorrent.exe");
//	LPCTSTR dir = _T("C:\\Program Files\\uTorrent");
	
	TCHAR cmd[] = _T("C:\\Program Files (x86)\\uTorrent\\uTorrent.exe");
	LPCTSTR dir = _T("C:\\Program Files (x86)\\uTorrent");

	if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &siStartInfo, &piProcInfo))
	{
		WaitForInputIdle(piProcInfo.hProcess, INFINITE);
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);

		astate = a_starting;
		SetDlgItemTextI(IDC_STATUS_STATIC, IDS_STARTING);
		deadtime = clock() + 2*1000;
	}
}

void CTorMgrDlg::StartStop(bool start, bool man)
{
	if (theApp.IsServer())
	{
		if ((start && tstatus == stopped) || (!start && tstatus == started))
		{
			GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);
			SetDlgItemText(IDC_START_STOP, _T(""));

			if (start && tstatus == stopped)
			{
				startTor();
				if (man)
					last_man_time = CTime::GetCurrentTime();
			}
			else if (!start && tstatus == started)
			{
				try
				{
					TorSupervisor	ts;
					ts.terminateTor();
					astate = a_stopping;
					SetDlgItemTextI(IDC_STATUS_STATIC, IDS_STOPPING);
					deadtime = clock() + 2*1000;
					close_request_time = CTime::GetCurrentTime();
				}
				catch (TCHAR *e_str)
				{
					AfxMessageBox(e_str);
				}
			}

			OnTimer(1);
		}
	}
}

void CTorMgrDlg::OnBnClickedStartStop()
{
	if (theApp.IsServer())
	{
		StartStop(tstatus == stopped, true);
		OnTimer(1);
	}
	else
	{
		GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);
		SetDlgItemText(IDC_START_STOP, _T(""));

		char b = tstatus == started ? 0 : 1;
		Sockets.Broadcast(&b, 1);
	}
}

class Socket
{
public:
	Socket(SOCKET inp_s = INVALID_SOCKET) :
		s(inp_s),
		send_len(0),
		send_ptr(0),
		recv_len(0)
	{}
	~Socket() {}

	char	send_buf[128];
	int		send_len;
	int		send_ptr;

	char	recv_buf[128];
	int		recv_len;

	bool send(const char *buf, int len);
	void send_impl();

protected:
	SOCKET	s;
};

void Socket::send_impl()
{
	if (send_len != 0)
	{
		int sl = ::send(s, send_buf+send_ptr, send_len, 0);
		if (sl != SOCKET_ERROR)
		{
			send_len -= sl;
			if (send_len == 0)
				send_ptr = 0;
			else
				send_ptr += sl;
		}
	}
}

bool Socket::send(const char *buf, int len)
{
	if (send_ptr+send_len+len > sizeof(send_buf))
		return false;

	memcpy(send_buf+send_ptr+send_len, buf, len);

	send_len += len;
	if (send_len == len)
		send_impl();

	return true;
}

SocketMap::~SocketMap()
{
	for (SocketMap::iterator i = begin(); i != end(); i = begin())
	{
		closesocket(i->first);
		delete i->second;
		erase(i);
	}
}

void SocketMap::Broadcast(const char *buf, int len)
{
	for (SocketMap::iterator i = begin(); i != end(); ++i)
		i->second->send(buf, len);
}

void CTorMgrDlg::InitSrvSocket(SOCKET &s, unsigned short port)
{
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		throw(_T("Can not open socket"));

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(port);

	if (bind(s, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
		throw(_T("bind() failed."));

	WSAAsyncSelect(s, *this, WSA_Msg, FD_ACCEPT|FD_CLOSE|FD_READ|FD_WRITE);

	if (listen(s, 1 ) == SOCKET_ERROR)
		throw(_T("Error listening on socket."));
}

void CTorMgrDlg::InitClntSocket()
{
	if (connecting)
		return;

	SOCKET clnt_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clnt_socket == INVALID_SOCKET)
		throw(_T("Can not open socket"));

	sockaddr_in service;

	service.sin_family = AF_INET;

	service.sin_addr.s_addr = theApp.ServerIP();
	service.sin_port = htons(15303);

	WSAAsyncSelect(clnt_socket, *this, WSA_Msg, FD_CONNECT|FD_CLOSE|FD_READ|FD_WRITE);

	if (connect(clnt_socket, (SOCKADDR*)&service, sizeof(service)) == 0)
	{
		Sockets[clnt_socket] = new Socket(clnt_socket);
		SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTED);
	}
	else if (WSAGetLastError() == WSAEWOULDBLOCK)
		connecting = true;
	else
	{
//		throw(_T("Failed to connect."));
	}
}

void CTorMgrDlg::OnSocketEventSrv(SOCKET s, int event, int error)
{
	if (error != 0)
	{
		CString es;
		es.Format(_T("WSAGETSELECTERROR %d"), error);
		AfxMessageBox(es);
	}
	else if (event == FD_ACCEPT)
	{
		SOCKET as = accept(s, NULL, NULL);

		if (s == SrvSocket)
		{
			Sockets[as] = new Socket(as);
			BroadcastStatus();
			SetDlgItemInt(IDC_CONNECTION_STATUS, (UINT)Sockets.size());
		}
		else if (s == FakeTorSrvSocket)
		{
			FakeTorSockets[as] = new Socket(as);
			SetDlgItemInt(IDC_FAKE_CONN_NO, (UINT)FakeTorSockets.size());

			PostMessage(TorMgr_Msg, 0, as);
		}
		else
		{
			closesocket(as);
			ASSERT(s != SrvSocket && s != FakeTorSrvSocket);
		}
	}
	else if (event == FD_READ)
	{
		SocketMap::iterator i = Sockets.find(s);
		if (i != Sockets.end())
		{
			Socket *cs = i->second;
			int rl = recv(s, cs->recv_buf+cs->recv_len, sizeof(cs->recv_buf)-cs->recv_len, 0);
			if (rl != SOCKET_ERROR)
			{
				cs->recv_len += rl;

				while (cs->recv_len != 0)
				{
					StartStop(cs->recv_buf[0] != 0, true);
					memmove(cs->recv_buf, cs->recv_buf+1, 1);
					--cs->recv_len;
				}
			}
		}
		else
		{
			SocketMap::iterator i = FakeTorSockets.find(s);
			if (i != FakeTorSockets.end())
			{
				static char buf[256];
				recv(s, buf, sizeof(buf), 0);
			}
		}
	}
	else if (event == FD_WRITE)
	{
		SocketMap::iterator i = Sockets.find(s);
		if (i != Sockets.end())
		{
			Socket *as = i->second;
			as->send_impl();
		}
		else
		{
			SocketMap::iterator i = FakeTorSockets.find(s);
			if (i != FakeTorSockets.end())
			{
				i = i;
			}
		}
	}
}

void CTorMgrDlg::OnSocketEventClnt(SOCKET s, int event, int error)
{
	if (event == FD_CONNECT)
	{
		if (error == 0)
		{
			Sockets[s] = new Socket(s);
			SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTED);
		}
		connecting = false;
	}
	else if (error != 0)
	{
		CString es;
		es.Format(_T("WSAGETSELECTERROR %d"), error);
		AfxMessageBox(es);
	}
	else if (event == FD_READ)
	{
		SocketMap::iterator i = Sockets.find(s);
		if (i != Sockets.end())
		{
			Socket *cs = i->second;
			int rl = recv(s, cs->recv_buf+cs->recv_len, sizeof(cs->recv_buf)-cs->recv_len, 0);
			if (rl != SOCKET_ERROR)
			{
				cs->recv_len += rl;

				while (cs->recv_len >= 3)
				{
					SetIndicators(cs->recv_buf[1], cs->recv_buf[2]);
					memmove(cs->recv_buf, cs->recv_buf+3, 3);
					cs->recv_len -= 3;
				}
			}
		}
	}
	else if (event == FD_WRITE)
	{
		SocketMap::iterator i = Sockets.find(s);
		if (i != Sockets.end())
		{
			Socket *as = i->second;
			as->send_impl();
		}
	}
}

LRESULT CTorMgrDlg::OnTorMgr_Msg(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
	{
		SOCKET s = lParam;
		shutdown(s, SD_BOTH);
	}

	return 0;
}

LRESULT CTorMgrDlg::OnSocketEvent(WPARAM wParam, LPARAM lParam)
{
	int event = WSAGETSELECTEVENT(lParam);
	int error = WSAGETSELECTERROR(lParam);

	if (event == FD_CLOSE)
	{
		SOCKET s = wParam;

		SocketMap::iterator i = Sockets.find(s);
		if (i != Sockets.end())
		{
			if (error == 0)
			{
//graceful shutdown
				shutdown(s, SD_BOTH);
			}
			closesocket(s);
			delete i->second;
			Sockets.erase(i);

			if (theApp.IsServer())
				SetDlgItemInt(IDC_CONNECTION_STATUS, (UINT)Sockets.size());
			else
			{
				SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTING);
				SetDlgItemTextI(IDC_STATUS_STATIC, IDS_UNKNOWN);
				GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);
			}
		}
		else
		{
			SocketMap::iterator i = FakeTorSockets.find(s);
			if (i != FakeTorSockets.end())
			{
				if (error == 0)
				{
	//graceful shutdown
					shutdown(s, SD_BOTH);
				}
				closesocket(s);
				delete i->second;
				FakeTorSockets.erase(i);
				SetDlgItemInt(IDC_FAKE_CONN_NO, (UINT)FakeTorSockets.size());
			}
		}
	}
	else
	{
		if (theApp.IsServer())
			OnSocketEventSrv(wParam, event, error);
		else
			OnSocketEventClnt(wParam, event, error);
	}

	return 0;
}
