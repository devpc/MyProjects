#include "stdafx.h"
#include "ApiHook.h"

// 这里定义了一个产生指针的宏
#define MakePtr(cast, ptr, AddValue) (cast)((DWORD)(ptr)+(DWORD)(AddValue))

// 这个函数监测当前系统是否是WindowNT
BOOL IsNT();

// 这个函数得到hModule -- 即我们需要截获的函数所在的DLL模块的引入描述符(import descriptor)
PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor(HMODULE hModule, LPCSTR szImportModule);

// 我们的主函数
BOOL HookImportFunction(HMODULE hModule, LPCSTR szImportModule, LPHOOKFUNCDESC paHookFunc, PROC* paOrigFuncs)
{
	/////////////////////// 下面的代码检测参数的有效性 ////////////////////////////
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

	// 监测当前模块是否是在2GB虚拟内存空间之上
	// 这部分的地址内存是属于Win32进程共享的
	if (!IsNT() && ((DWORD)hModule >= 0x80000000))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_HANDLE, SLE_ERROR);
		return FALSE;
	}

	// 清零
	if (paOrigFuncs) memset(paOrigFuncs, NULL, sizeof(PROC));

	// 调用GetNamedImportDescriptor()函数,来得到hModule -- 即我们需要
	// 截获的函数所在的DLL模块的引入描述符(import descriptor)
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = GetNamedImportDescriptor(hModule, szImportModule);
	if (pImportDesc == NULL)
	return FALSE; // 若为空,则模块未被当前进程所引入

	// 从DLL模块中得到原始的THUNK信息,因为pImportDesc->FirstThunk数组中的原始信息已经
	// 在应用程序引入该DLL时覆盖上了所有的引入信息,所以我们需要通过取得pImportDesc->OriginalFirstThunk
	// 指针来访问引入函数名等信息
	PIMAGE_THUNK_DATA pOrigThunk = MakePtr(PIMAGE_THUNK_DATA, hModule,
	pImportDesc->OriginalFirstThunk);

	// 从pImportDesc->FirstThunk得到IMAGE_THUNK_DATA数组的指针,由于这里在DLL被引入时已经填充了
	// 所有的引入信息,所以真正的截获实际上正是在这里进行的
	PIMAGE_THUNK_DATA pRealThunk = MakePtr(PIMAGE_THUNK_DATA, hModule, pImportDesc->FirstThunk);

	// 穷举IMAGE_THUNK_DATA数组,寻找我们需要截获的函数,这是最关键的部分!
	while (pOrigThunk->u1.Function)
	{
		// 只寻找那些按函数名而不是序号引入的函数
		if (IMAGE_ORDINAL_FLAG != (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG))
		{
			// 得到引入函数的函数名
			PIMAGE_IMPORT_BY_NAME pByName = MakePtr(PIMAGE_IMPORT_BY_NAME, hModule,
			pOrigThunk->u1.AddressOfData);

			// 如果函数名以NULL开始,跳过,继续下一个函数
			if ('\0' == pByName->Name[0])
			continue;

			// bDoHook用来检查是否截获成功
			BOOL bDoHook = FALSE;

			// 检查是否当前函数是我们需要截获的函数
			OutputDebugString((char*)pByName->Name);

			if ((paHookFunc->szFunc[0] == pByName->Name[0]) && (strcmpi(paHookFunc->szFunc, (char*)pByName->Name) == 0))
			{
				// 找到了!
				if (paHookFunc->pProc)
				bDoHook = TRUE;
			}

			if (bDoHook)
			{
				// 我们已经找到了所要截获的函数,那么就开始动手吧
				// 首先要做的是改变这一块虚拟内存的内存保护状态,让我们可以自由存取
				MEMORY_BASIC_INFORMATION mbi_thunk;
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				_ASSERT(VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize,
				PAGE_READWRITE, &mbi_thunk.Protect));

				// 保存我们所要截获的函数的正确跳转地址
				if (paOrigFuncs)
				{
					*paOrigFuncs = (PROC)pRealThunk->u1.Function;
				}

				// 将IMAGE_THUNK_DATA数组中的函数跳转地址改写为我们自己的函数地址!
				// 以后所有进程对这个系统函数的所有调用都将成为对我们自己编写的函数的调用
				pRealThunk->u1.Function = (PDWORD)paHookFunc->pProc;

				OutputDebugString("done\n");

				// 操作完毕!将这一块虚拟内存改回原来的保护状态
				DWORD dwOldProtect;
				_ASSERT(VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize,
				mbi_thunk.Protect, &dwOldProtect));
				SetLastError(ERROR_SUCCESS);
				return TRUE;
			}
		}
		// 访问IMAGE_THUNK_DATA数组中的下一个元素
		pOrigThunk++;
		pRealThunk++;
	}
	return TRUE;
}

// GetNamedImportDescriptor函数的实现
PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor(HMODULE hModule, LPCSTR szImportModule)
{
	// 检测参数
	_ASSERT(szImportModule);
	_ASSERT(hModule);
	if ((szImportModule == NULL) || (hModule == NULL))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// 得到Dos文件头
	PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER) hModule;

	// 检测是否MZ文件头
	if (IsBadReadPtr(pDOSHeader, sizeof(IMAGE_DOS_HEADER)) || (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// 取得PE文件头
	PIMAGE_NT_HEADERS pNTHeader = MakePtr(PIMAGE_NT_HEADERS, pDOSHeader, pDOSHeader->e_lfanew);

	// 检测是否PE映像文件
	if (IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) || (pNTHeader->Signature != IMAGE_NT_SIGNATURE))
	{
		_ASSERT(FALSE);
		SetLastErrorEx(ERROR_INVALID_PARAMETER, SLE_ERROR);
		return NULL;
	}

	// 检查PE文件的引入段(即 .idata section)
	if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
	return NULL;

	// 得到引入段(即 .idata section)的指针
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = MakePtr(PIMAGE_IMPORT_DESCRIPTOR, pDOSHeader,
	pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// 穷举PIMAGE_IMPORT_DESCRIPTOR数组寻找我们需要截获的函数所在的模块
	while (pImportDesc->Name)
	{
		PSTR szCurrMod = MakePtr(PSTR, pDOSHeader, pImportDesc->Name);
		OutputDebugString(szCurrMod);

		if (stricmp(szCurrMod, szImportModule) == 0)
		break; // 找到!中断循环
		// 下一个元素
		pImportDesc++;
	}

	// 如果没有找到,说明我们寻找的模块没有被当前的进程所引入!
	if (pImportDesc->Name == NULL)
	return NULL;

	// 返回函数所找到的模块描述符(import descriptor)
	return pImportDesc;
}

// IsNT()函数的实现
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
// 这里进行输出lpszString的处理
// 然后调用正版的TextOutA函数

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
// 这里进行输出lpszString的处理
// 然后调用正版的TextOutA函数

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