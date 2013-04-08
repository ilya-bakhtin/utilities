// netspeedtestdlg.cpp : implementation file
//

#include "stdafx.h"
#include "netspeedtest.h"
#include "netspeedtestdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static UINT WSA_Msg = ::RegisterWindowMessage(_T("OnSocketEventMessage"));
static UINT WR_buffer_ready = ::RegisterWindowMessage(_T("OnBufferReadyMessage"));

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

// CNetSpeedTestDlg dialog

CNetSpeedTestDlg::CNetSpeedTestDlg(CWnd* pParent /*=NULL*/) :
	CDialog(CNetSpeedTestDlg::IDD, pParent),
	timerID(0),
	tstatus(unknown),
	last_clock(-1),
	deadtime(-1),
	close_request_time(CTime::GetCurrentTime()-CTimeSpan(0, 24, 0, 0)),
	astate(a_normal),
	srv_socket(INVALID_SOCKET),
	connecting(false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_NETSPEEDTEST);
/*
	if (theApp.IsServer())
		m_hIcon = AfxGetApp()->LoadIcon(IDI_TORMGR_S);
	else
		m_hIcon = AfxGetApp()->LoadIcon(IDI_TORMGR_C);
*/
}

void CNetSpeedTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNetSpeedTestDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_NCDESTROY()
	ON_WM_QUERYDRAGICON()
	ON_REGISTERED_MESSAGE(WSA_Msg, OnSocketEvent)
	ON_REGISTERED_MESSAGE(WR_buffer_ready, OnBufferReady)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_START_STOP, OnBnClickedStartStop)
END_MESSAGE_MAP()


// CNetSpeedTestDlg message handlers

BOOL CNetSpeedTestDlg::OnInitDialog()
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
			InitSrvSocket();
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

	SetDlgItemText(IDC_STATUS_STATIC, "");

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

void CNetSpeedTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CNetSpeedTestDlg::OnPaint() 
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
HCURSOR CNetSpeedTestDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CNetSpeedTestDlg::OnNcDestroy()
{
	if (timerID != 0)
		KillTimer(timerID);
	super::OnNcDestroy();
}

void CNetSpeedTestDlg::SetDlgItemTextI(int nID, int strID)
{
	CString str;
	str.LoadString(strID);
	SetDlgItemText(nID, str);
}

void CNetSpeedTestDlg::BroadcastStatus()
{
	char buf[] = {
		0,
		astate,
		tstatus
	};

	Sockets.Broadcast(buf, sizeof(buf));
}

void CNetSpeedTestDlg::SetIndicators(int i_astate, int i_tstatus)
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

void CNetSpeedTestDlg::SrvOnTimer()
{
}

void CNetSpeedTestDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != 1)
		return;

	if (!theApp.IsServer())
	{
		if (Sockets.size() == 0)
			InitClntSocket();
	}
}

void CNetSpeedTestDlg::startTor()
{
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

	TCHAR cmd[] = _T("C:\\Program Files\\uTorrent\\uTorrent.exe");
	LPCTSTR dir = _T("C:\\Program Files\\uTorrent");

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

void CNetSpeedTestDlg::StartStop(bool start)
{
}

void CNetSpeedTestDlg::OnBnClickedStartStop()
{
	if (theApp.IsServer())
	{
		StartStop(tstatus == stopped);
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

#define buffer_size (1024*1024)

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

	char	send_buf[buffer_size];
	int		send_len;
	int		send_ptr;

	char	recv_buf[buffer_size];
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

void CNetSpeedTestDlg::InitSrvSocket()
{
	srv_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (srv_socket == INVALID_SOCKET)
		throw(_T("Can not open socket"));

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(theApp.ServerPort());

	if (bind(srv_socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
		throw(_T("bind() failed."));

	WSAAsyncSelect(srv_socket, *this, WSA_Msg, FD_ACCEPT|FD_CLOSE|FD_READ|FD_WRITE);

	if (listen(srv_socket, 1 ) == SOCKET_ERROR)
		throw(_T("Error listening on socket."));
}

void CNetSpeedTestDlg::InitClntSocket()
{
	if (connecting)
		return;

	SOCKET clnt_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clnt_socket == INVALID_SOCKET)
		throw(_T("Can not open socket"));

	sockaddr_in service;

	service.sin_family = AF_INET;

	service.sin_addr.s_addr = theApp.ServerIP();
	service.sin_port = htons(theApp.ServerPort());

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

static clock_t start_clock = 0;
static double total_bytes = 0;
static clock_t last_clock = 0;

void CNetSpeedTestDlg::OnSocketEventSrv(SOCKET s, int event, int error)
{
	if (error != 0)
	{
		CString es;
		es.Format(_T("WSAGETSELECTERROR %d"), error);
		AfxMessageBox(es);
	}
	else if (s == srv_socket && event == FD_ACCEPT)
	{
		SOCKET as = accept(srv_socket, NULL, NULL);
		Socket *s_as = Sockets[as] = new Socket(as);

		SetDlgItemInt(IDC_CONNECTION_STATUS, (UINT)Sockets.size());

		PostMessage(WR_buffer_ready, as, 0);
		connecting = false;
		start_clock = last_clock = clock();
		total_bytes = 0;
	}
}

void CNetSpeedTestDlg::OnSocketEventClnt(SOCKET s, int event, int error)
{
	if (event == FD_CONNECT)
	{
		if (error == 0)
		{
			Sockets[s] = new Socket(s);

			SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTED);

			PostMessage(WR_buffer_ready, s, 0);
			connecting = false;
			start_clock = last_clock = clock();
			total_bytes = 0;
		}
	}
	else if (error != 0)
	{
		CString es;
		es.Format(_T("WSAGETSELECTERROR %d"), error);
		AfxMessageBox(es);
	}
}

LRESULT CNetSpeedTestDlg::OnSocketEvent(WPARAM wParam, LPARAM lParam)
{
	int event = WSAGETSELECTEVENT(lParam);
	int error = WSAGETSELECTERROR(lParam);

	SOCKET s = wParam;
	SocketMap::iterator i = Sockets.find(s);

	if (event == FD_CLOSE)
	{
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
			{
				SetDlgItemInt(IDC_CONNECTION_STATUS, (UINT)Sockets.size());
				if (Sockets.size() == 0)
					SetDlgItemText(IDC_STATUS_STATIC, "");
			}
			else
			{
				SetDlgItemTextI(IDC_CONNECTION_STATUS, IDS_CONNECTING);
				SetDlgItemText(IDC_STATUS_STATIC, "");
				GetDlgItem(IDC_START_STOP)->EnableWindow(FALSE);
			}
		}
	}
	else if (event == FD_WRITE)
	{
		if (i != Sockets.end())
		{
			Socket *as = i->second;
			as->send_impl();
			if (as->send_len == 0)
				PostMessage(WR_buffer_ready, s, 0);
		}
	}
	else if (event == FD_READ)
	{
		if (i != Sockets.end())
		{
			Socket *cs = i->second;
			int rl = recv(s, cs->recv_buf+cs->recv_len, sizeof(cs->recv_buf)-cs->recv_len, 0);
			if (rl != SOCKET_ERROR)
			{
				cs->recv_len += rl;
				cs->recv_len = 0;

				static const int period = 1000;

				total_bytes += rl;
				if (clock() - last_clock >= period)
				{
					CString str;
//					str.Format("%10.2f kbps", (double)total_bytes/(clock()-start_clock));
					str.Format("%10.2f bps", (double)total_bytes*1000*8/(clock()-start_clock));
					SetDlgItemText(IDC_STATUS_STATIC, str);
					last_clock = clock();
				}
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

LRESULT CNetSpeedTestDlg::OnBufferReady(WPARAM wParam, LPARAM lParam)
{
	SocketMap::iterator i = Sockets.find(wParam);
	if (i != Sockets.end())
	{
		Socket *as = i->second;
		while (as->send_len == 0)
		{
			static char buf[buffer_size];
			as->send(buf, sizeof(buf));
		}
	}
	return 0;
}
