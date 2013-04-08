#include "StdAfx.h"
#include <Psapi.h>
#include <tlhelp32.h>
#include "torsupervisor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

TorSupervisor::TorSupervisor(void)
{
}

TorSupervisor::~TorSupervisor(void)
{
}

static
void GetProcessNameByID(DWORD processID, LPTSTR proc_name)
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	proc_name[0] = 0;

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );
    // Get the process name.

    if (hProcess != NULL)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
            GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR));
    }

	_tcscpy(proc_name, szProcessName);

    CloseHandle( hProcess );
}

class ResHolder
{
friend class TorSupervisor;

	ResHolder() :
		aProcesses(NULL),
		ThreadsSnapshot(INVALID_HANDLE_VALUE)
	{}

	virtual ~ResHolder()
	{
		delete [] aProcesses;
		if (ThreadsSnapshot != INVALID_HANDLE_VALUE)
			CloseHandle(ThreadsSnapshot);
	}

    DWORD	*aProcesses;
	HANDLE	ThreadsSnapshot;
};

void TorSupervisor::EnumProcThreads(LPCTSTR proc_name, ThreadEnumerator *e)
{
    // Get the list of process identifiers.
	DWORD		cbAllocated = 16;
	DWORD		cbNeeded = cbAllocated;
	ResHolder	resou;

	for (;cbNeeded >= cbAllocated;)
	{
		delete [] resou.aProcesses;
		cbAllocated *= 2;
		resou.aProcesses = new DWORD[cbAllocated];

		if (!EnumProcesses(resou.aProcesses, cbAllocated, &cbNeeded))
			throw(_T("EnumProcesses failed"));
	}

    // Calculate how many process identifiers were returned.

	DWORD cProcesses = cbNeeded / sizeof(DWORD);

    for (unsigned int i = 0; i < cProcesses; ++i)
	{
		TCHAR ProcessName[MAX_PATH];
		GetProcessNameByID(resou.aProcesses[i], ProcessName);

		if (_tcsicmp(ProcessName, proc_name) == 0)
		{
			resou.ThreadsSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (resou.ThreadsSnapshot == INVALID_HANDLE_VALUE)
				throw(_T("CreateToolhelp32Snapshot() failed"));

			THREADENTRY32 te32;
			// Fill in the size of the structure before using it. 
			te32.dwSize = sizeof(THREADENTRY32); 

			// Retrieve information about the first thread,
			// and exit if unsuccessful
			if (!Thread32First(resou.ThreadsSnapshot, &te32))
				throw(_T("Thread32First failed"));		// Show cause of failure

			for (bool stop = false; !stop; stop = stop || !Thread32Next(resou.ThreadsSnapshot, &te32))
			{ 
				if (te32.th32OwnerProcessID == resou.aProcesses[i])
				{
					if (!e->OnThread(te32.th32ThreadID))
						stop = true;
				}
			}
		}
	}
}

class ThrdEnumTor : public ThreadEnumerator
{
friend class TorSupervisor;

	bool	found;
	bool	*wind;
	bool	term;

	ThrdEnumTor() :
		found(false),
		wind(NULL),
		term(false)
	{}

	bool OnThread(DWORD threadID)
	{
		found = true;
		if (wind != NULL)
			EnumThreadWindows(threadID, WndEnumProc, (LPARAM)this);

		return wind == NULL ? !found : !*wind;
	}

	static
	BOOL CALLBACK WndEnumProc(HWND hwnd, LPARAM lParam)
	{
		if (GetWindow(hwnd, GW_OWNER) == NULL)
		{
			TCHAR t[1024];
			GetWindowText(hwnd, t, sizeof(t)/sizeof(TCHAR));
			if (memcmp(t+1, _T("Torrent"), sizeof(_T("Torrent"))-sizeof(TCHAR)) == 0)
			{
				ThrdEnumTor *e = (ThrdEnumTor*)lParam;
				*e->wind = true;
				if (e->term)
					PostMessage(hwnd, WM_COMMAND, 64, 0);

				return FALSE;
			}
		}
		return TRUE;
	}
};

bool TorSupervisor::isTorRunning(bool *in_window)
{
	ThrdEnumTor e;
	e.wind = in_window;
	if (e.wind != NULL)
		*e.wind = false;

	EnumProcThreads(_T("utorrent.exe"), &e);

	return e.found;
}

void TorSupervisor::terminateTor()
{
	bool in_window = false;
	ThrdEnumTor e;
	e.wind = &in_window;
	e.term = true;

	EnumProcThreads(_T("utorrent.exe"), &e);
}
