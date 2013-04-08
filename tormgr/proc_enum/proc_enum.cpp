// proc_enum.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <tlhelp32.h>
#include "proc_enum.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// The one and only application object

CWinApp theApp;

using namespace std;

static
void PrintModules( DWORD processID )
{
    HMODULE hMods[1024];
    HANDLE hProcess;
    DWORD cbNeeded;
    unsigned int i;

    // Print the process identifier.

    printf( "\nProcess ID: %u\n", processID );

    // Get a list of all the modules in this process.

    hProcess = OpenProcess(  PROCESS_QUERY_INFORMATION |
                                    PROCESS_VM_READ,
                                    FALSE, processID );
    if (NULL == hProcess)
        return;

    if( EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        for ( i = 0; i < (cbNeeded / sizeof(HMODULE)); i++ )
        {
            TCHAR szModName[MAX_PATH];

            // Get the full path to the module's file.

            if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName)/sizeof(TCHAR)))
            {
                // Print the module name and handle value.

                printf("\t%s (0x%08IX)\n", szModName, hMods[i]);
            }
        }
    }

    CloseHandle( hProcess );
}

static
void PrintProcessNameAndID(DWORD processID, LPTSTR proc_name)
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	proc_name[0] = 0;

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
            GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR));
			_tcscpy(proc_name, szProcessName);
        }
    }

    // Print the process name and identifier.

    _tprintf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

    CloseHandle( hProcess );
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
			*(BOOL*)lParam = TRUE;
//			PostMessage(hwnd, WM_CLOSE, 0, 0);
			PostMessage(hwnd, WM_COMMAND, 64, 0);
			return FALSE;
		}
	}
	return TRUE;
}

static
int do_stuff(LPCTSTR proc_name)
{
    // Get the list of process identifiers.

    DWORD			*aProcesses = NULL;
	DWORD			cbAllocated = 16;
	DWORD			cbNeeded = cbAllocated;
	DWORD			cProcesses;
    unsigned int	i;

	for (;cbNeeded >= cbAllocated;)
	{
		delete [] aProcesses;
		cbAllocated *= 2;
		aProcesses = new DWORD[cbAllocated];

		if (!EnumProcesses(aProcesses, cbAllocated, &cbNeeded))
			return 0;
	}

    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name of the modules for each process.

    for ( i = 0; i < cProcesses; i++ )
	{
		TCHAR ProcessName[MAX_PATH];
//        PrintModules(aProcesses[i]);
		PrintProcessNameAndID(aProcesses[i], ProcessName);
		if (_tcsicmp(ProcessName, proc_name) == 0)
		{
			HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (snapshot == INVALID_HANDLE_VALUE)
				throw("CreateToolhelp32Snapshot() failed");

			THREADENTRY32 te32;
			// Fill in the size of the structure before using it. 
			te32.dwSize = sizeof(THREADENTRY32); 

			// Retrieve information about the first thread,
			// and exit if unsuccessful
			if (!Thread32First(snapshot, &te32))
			{
				CloseHandle(snapshot);		// Must clean up the snapshot object!
				throw("Thread32First");		// Show cause of failure
			}
			do 
			{ 
				if (te32.th32OwnerProcessID == aProcesses[i])
				{
					BOOL found = FALSE;
					EnumThreadWindows(te32.th32ThreadID, WndEnumProc, (LPARAM)&found);
					if (found)
					{
//						PostThreadMessage(te32.th32ThreadID, WM_CLOSE, 0, 0);
						found = found;
					}
/*
					printf( "\n\n     THREAD ID      = 0x%08X", te32.th32ThreadID ); 
					printf( "\n     base priority  = %d", te32.tpBasePri ); 
					printf( "\n     delta priority = %d", te32.tpDeltaPri ); 
*/
				}
			} while(Thread32Next(snapshot, &te32)); 

			CloseHandle(snapshot);
		}
	}

	delete [] aProcesses;

	return 0;
}

static
void init_socket()
{
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
		throw("Can not open socket");

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(15303);

	if (bind(s, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
		throw("bind() failed.");

	if (listen(s, 1 ) == SOCKET_ERROR)
		throw("Error listening on socket.");

	SOCKET AcceptSocket = accept(s, NULL, NULL);

    closesocket(s);
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	if (!AfxSocketInit())
	{
		AfxMessageBox(_T("Failed to Initialize Sockets"), MB_OK | MB_ICONSTOP);
		return 0;
	}

//	init_socket();

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		AfxMessageBox(_T("Fatal Error: MFC initialization failed\n"), MB_OK | MB_ICONSTOP);
		nRetCode = 1;
	}
	else
	{
		nRetCode = do_stuff(_T("utorrent.exe"));
	}

	return nRetCode;
}
