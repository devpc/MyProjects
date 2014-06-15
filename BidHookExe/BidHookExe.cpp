// BidHookExe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

bool InjectionDll(LPCTSTR lpszProcess, LPCTSTR lpszDll);

void InitHook(const char *dll)
{
	HMODULE hDll = LoadLibraryA(dll);
	if (!hDll)
	{
		printf("LoadLibraryA Error. %d\n", GetLastError());
		return;
	}

	HOOKPROC f = (HOOKPROC)GetProcAddress(hDll, "_HookProc1@12");
	if (!f)
	{
		printf("GetProcAddress Error.\n");
		return;
	}

	HWND hWnd = FindWindowA("TMainForm", NULL);
	if (hWnd == NULL)
	{
		printf("FindWindow Error.\n");
		return;
	}
	else
	{
		HWND wnd = FindWindowEx(hWnd, NULL, "TEdit", NULL);
		if (wnd)
		{
			wnd = FindWindowEx(hWnd, wnd, "TEdit", NULL);
			if (wnd)
			{
				hWnd = wnd;
			}
		}
	}

	HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, f, hDll, GetWindowThreadProcessId(hWnd, NULL));
	if (!hook)
	{
		printf("HOOK Error %d.\n", GetLastError());
		return;
	}

	printf("InitHook Done\n");

	while (1)
		Sleep(1000);
}

int main(int argc, char* argv[])
{
	if (argc <= 2)
	{
		printf("USAGE: main [exe file name] [dll file name]\n");
	}
	else
	{
//		LoadLibrary(argv[2]);

		if (InjectionDll(argv[1], argv[2]))
		{
			printf("Injection done.\n");
		}

//		InitHook(argv[2]);
	}

	printf("Press Enter to exit.");

	getchar();//这里getchar是为了防止程序退出，若程序过快退出，钩子可能没有效果

	return 0;
}

DWORD FindTarget( LPCTSTR lpszProcess )
{
	DWORD dwRet = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	Process32First( hSnapshot, &pe32 );
	do
	{
		printf("%s\n", pe32.szExeFile);

		if ( lstrcmpi( pe32.szExeFile, lpszProcess ) == 0 )
		{
			dwRet = pe32.th32ProcessID;
			break;
		}
	} while ( Process32Next( hSnapshot, &pe32 ) );
	CloseHandle( hSnapshot );
	return dwRet;
}

bool InjectionDll(LPCTSTR lpszProcess, LPCTSTR lpszDll)
{
	DWORD dwProcessID = FindTarget( lpszProcess );

	if (dwProcessID == 0)
	{
		printf("Could not find the exe file: %s\n", lpszProcess);
		return false;
	}

	HANDLE hProcess = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, dwProcessID );

	if (hProcess == 0)
	{
		printf("Could not open process for id: %d\n", dwProcessID);
		return false;
	}

	DWORD dwSize, dwWritten;
	dwSize = lstrlenA( lpszDll ) + 1;
	LPVOID lpBuf = VirtualAllocEx( hProcess, NULL, dwSize, MEM_COMMIT, PAGE_READWRITE );
	if ( NULL == lpBuf )
	{
		CloseHandle( hProcess );

		printf("VirtualAllocEx error for id: %d\n", dwProcessID);
		return false;
	}
	if ( WriteProcessMemory( hProcess, lpBuf, (LPVOID)lpszDll, dwSize, &dwWritten ) )
	{
		if ( dwWritten != dwSize )
		{
			VirtualFreeEx( hProcess, lpBuf, dwSize, MEM_DECOMMIT );
			CloseHandle( hProcess );

			printf("WriteProcessMemory [dwWritten != dwSize] error for id: %d\n", dwProcessID);
			return false;
		}
	}
	else
	{
		CloseHandle( hProcess );

		printf("WriteProcessMemory error for id: %d\n", dwProcessID);
		return false;
	}

	DWORD dwID;
	LPVOID pFunc = LoadLibraryA;
	HANDLE hThread = CreateRemoteThread( hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFunc, lpBuf, 0, &dwID );

	WaitForSingleObject( hThread, INFINITE );
	
	VirtualFreeEx( hProcess, lpBuf, dwSize, MEM_DECOMMIT );
	CloseHandle( hThread );
	CloseHandle( hProcess );

	return true;
}