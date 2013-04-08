// TorMgrDlg.h : header file
//

#pragma once

#include <map>

class Socket;
class SocketMap : public std::map<SOCKET, Socket*>
{
public:
	virtual ~SocketMap();

	void Broadcast(const char *buf, int len);
};

// CTorMgrDlg dialog
class CTorMgrDlg : public CDialog
{
typedef CDialog super;
// Construction
public:
	CTorMgrDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CTorMgrDlg() {}

// Dialog Data
	enum { IDD = IDD_TORMGR_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON		m_hIcon;
	UINT_PTR	timerID;
	int			last_clock;
	CTime		last_man_time;
	int			deadtime;
	CTime		close_request_time;
	SOCKET		SrvSocket;
	bool		connecting;
	SocketMap	Sockets;

	SOCKET		FakeTorSrvSocket;
	SocketMap	FakeTorSockets;

	enum Tstatus
	{
		unknown,
		stopped,
		stopping,
		started
	};
	int	tstatus;

	enum Astate
	{
		a_normal,
		a_starting,
		a_stopping
	};
	int	astate;

	void startTor();
	void SrvOnTimer();
	void InitSrvSocket(SOCKET &s, unsigned short port);
	void InitClntSocket();
	void SetDlgItemTextI(int nID, int strID);
	void OnSocketEventSrv(SOCKET s, int event, int error);
	void OnSocketEventClnt(SOCKET s, int event, int error);
	void BroadcastStatus();
	void SetIndicators(int i_astate, int i_tstatus);
	void StartStop(bool start, bool man);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnNcDestroy();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnSocketEvent(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTorMgr_Msg(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedStartStop();
};
