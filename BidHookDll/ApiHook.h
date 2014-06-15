#pragma once

#include <crtdbg.h>

// 定义了HOOKFUNCDESC结构,我们用这个结构作为参数传给HookImportFunction函数
typedef struct tag_HOOKFUNCDESC
{
	LPCSTR szFunc; // The name of the function to hook.
	PROC pProc; // The procedure to blast in.
} HOOKFUNCDESC, *LPHOOKFUNCDESC;

// 我们的主函数
BOOL HookImportFunction(HMODULE hModule, LPCSTR szImportModule, LPHOOKFUNCDESC paHookFunc, PROC* paOrigFuncs);

