#include "stdafx.h"
#include "ApiHook.h"

// ���ﶨ����һ������ָ��ĺ�
#define MakePtr(cast, ptr, AddValue) (cast)((DWORD)(ptr)+(DWORD)(AddValue))

// ���������⵱ǰϵͳ�Ƿ���WindowNT
BOOL IsNT();

// ��������õ�hModule -- ��������Ҫ�ػ�ĺ������ڵ�DLLģ�������������(import descriptor)
PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor(HMODULE hModule, LPCSTR szImportModule);

// ���ǵ�������
BOOL HookImportFunction(HMODULE hModule, LPCSTR szImportModule, LPHOOKFUNCDESC paHookFunc, PROC* paOrigFuncs)
{
	/////////////////////// ����Ĵ������������Ч�� ////////////////////////////
	_ASSERT(szImportModule);
	_ASSERT(!IsBadReadPtr(paHookFunc, sizeof(HOOKFUNCDESC)));
#ifdef _DEBUG
	if (paOrigFuncs) _ASSERT(!IsBadWritePtr(paOrigFuncs, sizeof(PROC)));
	_ASSERT(paHookFunc->szFunc);
	_ASSERT(*paHookFunc->szFunc != '\0');
	_ASSERT(!IsBadCodePtr(paHookFunc->pProc));
#endif
	if ((szImportModule == NULL) || (IsBadReadPtr(paHookFunc, sizeof(HOOKFUNCDESC))))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return FALSE;
	}
	//////////////////////////////////////////////////////////////////////////////

	// ��⵱ǰģ���Ƿ�����2GB�����ڴ�ռ�֮��
	// �ⲿ�ֵĵ�ַ�ڴ�������Win32���̹����
	if (!IsNT() && ((DWORD)hModule >= 0x80000000))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_HANDLE, SLE_ERROR);
		return FALSE;
	}

	// ����
	if (paOrigFuncs) memset(paOrigFuncs, NULL, sizeof(PROC));

	// ����GetNamedImportDescriptor()����,���õ�hModule -- ��������Ҫ
	// �ػ�ĺ������ڵ�DLLģ�������������(import descriptor)
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = GetNamedImportDescriptor(hModule, szImportModule);
	if (pImportDesc == NULL)
	return FALSE; // ��Ϊ��,��ģ��δ����ǰ����������

	// ��DLLģ���еõ�ԭʼ��THUNK��Ϣ,��ΪpImportDesc->FirstThunk�����е�ԭʼ��Ϣ�Ѿ�
	// ��Ӧ�ó��������DLLʱ�����������е�������Ϣ,����������Ҫͨ��ȡ��pImportDesc->OriginalFirstThunk
	// ָ�����������뺯��������Ϣ
	PIMAGE_THUNK_DATA pOrigThunk = MakePtr(PIMAGE_THUNK_DATA, hModule,
	pImportDesc->OriginalFirstThunk);

	// ��pImportDesc->FirstThunk�õ�IMAGE_THUNK_DATA�����ָ��,����������DLL������ʱ�Ѿ������
	// ���е�������Ϣ,���������Ľػ�ʵ����������������е�
	PIMAGE_THUNK_DATA pRealThunk = MakePtr(PIMAGE_THUNK_DATA, hModule, pImportDesc->FirstThunk);

	// ���IMAGE_THUNK_DATA����,Ѱ��������Ҫ�ػ�ĺ���,������ؼ��Ĳ���!
	while (pOrigThunk->u1.Function)
	{
		// ֻѰ����Щ���������������������ĺ���
		if (IMAGE_ORDINAL_FLAG != (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG))
		{
			// �õ����뺯���ĺ�����
			PIMAGE_IMPORT_BY_NAME pByName = MakePtr(PIMAGE_IMPORT_BY_NAME, hModule,
			pOrigThunk->u1.AddressOfData);

			// �����������NULL��ʼ,����,������һ������
			if ('\0' == pByName->Name[0])
			continue;

			// bDoHook��������Ƿ�ػ�ɹ�
			BOOL bDoHook = FALSE;

			// ����Ƿ�ǰ������������Ҫ�ػ�ĺ���
			OutputDebugString((char*)pByName->Name);

			if ((paHookFunc->szFunc[0] == pByName->Name[0]) && (strcmpi(paHookFunc->szFunc, (char*)pByName->Name) == 0))
			{
				// �ҵ���!
				if (paHookFunc->pProc)
				bDoHook = TRUE;
			}

			if (bDoHook)
			{
				// �����Ѿ��ҵ�����Ҫ�ػ�ĺ���,��ô�Ϳ�ʼ���ְ�
				// ����Ҫ�����Ǹı���һ�������ڴ���ڴ汣��״̬,�����ǿ������ɴ�ȡ
				MEMORY_BASIC_INFORMATION mbi_thunk;
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				_ASSERT(VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize,
				PAGE_READWRITE, &mbi_thunk.Protect));

				// ����������Ҫ�ػ�ĺ�������ȷ��ת��ַ
				if (paOrigFuncs)
				{
					*paOrigFuncs = (PROC)pRealThunk->u1.Function;
				}

				// ��IMAGE_THUNK_DATA�����еĺ�����ת��ַ��дΪ�����Լ��ĺ�����ַ!
				// �Ժ����н��̶����ϵͳ���������е��ö�����Ϊ�������Լ���д�ĺ����ĵ���
				pRealThunk->u1.Function = (PDWORD)paHookFunc->pProc;

				OutputDebugString("done\n");

				// �������!����һ�������ڴ�Ļ�ԭ���ı���״̬
				DWORD dwOldProtect;
				_ASSERT(VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize,
				mbi_thunk.Protect, &dwOldProtect));
				SetLastError(ERROR_SUCCESS);
				return TRUE;
			}
		}
		// ����IMAGE_THUNK_DATA�����е���һ��Ԫ��
		pOrigThunk++;
		pRealThunk++;
	}
	return TRUE;
}

// GetNamedImportDescriptor������ʵ��
PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor(HMODULE hModule, LPCSTR szImportModule)
{
	// ������
	_ASSERT(szImportModule);
	_ASSERT(hModule);
	if ((szImportModule == NULL) || (hModule == NULL))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// �õ�Dos�ļ�ͷ
	PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER) hModule;

	// ����Ƿ�MZ�ļ�ͷ
	if (IsBadReadPtr(pDOSHeader, sizeof(IMAGE_DOS_HEADER)) || (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// ȡ��PE�ļ�ͷ
	PIMAGE_NT_HEADERS pNTHeader = MakePtr(PIMAGE_NT_HEADERS, pDOSHeader, pDOSHeader->e_lfanew);

	// ����Ƿ�PEӳ���ļ�
	if (IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) || (pNTHeader->Signature != IMAGE_NT_SIGNATURE))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// ���PE�ļ��������(�� .idata section)
	if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
	return NULL;

	// �õ������(�� .idata section)��ָ��
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = MakePtr(PIMAGE_IMPORT_DESCRIPTOR, pDOSHeader,
	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// ���PIMAGE_IMPORT_DESCRIPTOR����Ѱ��������Ҫ�ػ�ĺ������ڵ�ģ��
	while (pImportDesc->Name)
	{
		PSTR szCurrMod = MakePtr(PSTR, pDOSHeader, pImportDesc->Name);
		OutputDebugString(szCurrMod);

		if (stricmp(szCurrMod, szImportModule) == 0)
		break; // �ҵ�!�ж�ѭ��
		// ��һ��Ԫ��
		pImportDesc++;
	}

	// ���û���ҵ�,˵������Ѱ�ҵ�ģ��û�б���ǰ�Ľ���������!
	if (pImportDesc->Name == NULL)
	return NULL;

	// ���غ������ҵ���ģ��������(import descriptor)
	return pImportDesc;
}

// IsNT()������ʵ��
BOOL IsNT()
{
	OSVERSIONINFO stOSVI;
	memset(&stOSVI, NULL, sizeof(OSVERSIONINFO));
	stOSVI.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	BOOL bRet = GetVersionEx(&stOSVI);
	_ASSERT(TRUE == bRet);
	if (FALSE == bRet) return FALSE;
	return (VER_PLATFORM_WIN32_NT == stOSVI.dwPlatformId);
}
/////////////////////////////////////////////// End //////////////////////////////////////////////////////////////////////


BOOL WINAPI MyTextOutW(HDC hdc, int nXStart, int nYStart, LPCWSTR lpszString,int cbString)
{
// ����������lpszString�Ĵ���
// Ȼ����������TextOutA����

	OutputDebugStringW(lpszString);

	return 0;

//	return TextOutW(hdc, nXStart, nYStart, lpszString, cbString);
}

PROC pOrigMessageBoxFuns;

typedef int (*MessageBoxW_proc)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);

int
WINAPI
MyMessageBoxW(
    HWND hWnd,
    LPCWSTR lpText,
    LPCWSTR lpCaption,
    UINT uType)
{
	OutputDebugStringW(lpText);

	return ((MessageBoxW_proc)pOrigMessageBoxFuns)(hWnd, lpText, lpCaption, uType);
}

BOOL WINAPI MyTextOutA(HDC hdc, int nXStart, int nYStart, LPCSTR lpszString,int cbString)
{
// ����������lpszString�Ĵ���
// Ȼ����������TextOutA����

	char text[1024];
	sprintf(text, "MyTextOutA:%s\n", lpszString);
	OutputDebugString(text);

	return 0;
//	return TextOutA(hdc, nXStart, nYStart, lpszString, cbString);
}

PROC pOrigDCFuns;
typedef HDC(*DrawText_proc)(HWND hWnd);

int MyDrawTextA(HDC hDC, LPCTSTR lpchText, int nCount, LPRECT lpRect, UINT uFormat)
{
	OutputDebugString(lpchText);
	return 0;
}

int MyDrawTextW(HDC hDC, LPCWSTR lpchText, int nCount, LPRECT lpRect, UINT uFormat)
{
	OutputDebugStringW(lpchText);
	return 0;
}

void StartApiHook(HINSTANCE hModule)
{
//	HOOKFUNCDESC hd;
//	PROC pOrigFuns;

/*	OutputDebugString("================================= MessageBoxW");
	hd.szFunc="MessageBoxW";
	hd.pProc=(PROC)MyMessageBoxW;
	HookImportFunction(hModule,"user32.dll",&hd, &pOrigMessageBoxFuns);

	OutputDebugString("================================= DrawTextA");
	hd.szFunc="DrawTextA";
	hd.pProc=(PROC)MyDrawTextA;
	HookImportFunction(hModule,"user32.dll",&hd, &pOrigDCFuns);

	OutputDebugString("================================= DrawTextW");
	hd.szFunc="DrawTextW";
	hd.pProc=(PROC)MyDrawTextW;
	HookImportFunction(hModule,"user32.dll",&hd, &pOrigDCFuns);

	OutputDebugString("================================= TextOutA");
	hd.szFunc="TextOutA";
	hd.pProc=(PROC)MyTextOutA;
	HookImportFunction(hModule,"gdi32.dll",&hd, &pOrigFuns);

	OutputDebugString("================================= TextOutW");
	hd.szFunc="ExtTextOutW1";
	hd.pProc=(PROC)MyTextOutW;
	HookImportFunction(hModule,"gdi32.dll",&hd, &pOrigFuns);

	OutputDebugString("================================= TextOutW");
	hd.szFunc="ExtTextOutW1";
	hd.pProc=(PROC)MyTextOutW;
	HookImportFunction(hModule,"CarNetBid2.dll",&hd, &pOrigFuns);
*/

/*	OutputDebugString("================================= TextOutW");
	hd.szFunc="ExtTextOutW1";
	hd.pProc=(PROC)MyTextOutW;
	HookImportFunction(hModule,"libeay32.dll",&hd, &pOrigFuns);

	OutputDebugString("================================= TextOutW");
	hd.szFunc="ExtTextOutW1";
	hd.pProc=(PROC)MyTextOutW;
	HookImportFunction(hModule,"ssleay32.dll",&hd, &pOrigFuns);
*/
}