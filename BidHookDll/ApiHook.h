#pragma once

#include <crtdbg.h>

// ������HOOKFUNCDESC�ṹ,����������ṹ��Ϊ��������HookImportFunction����
typedef struct tag_HOOKFUNCDESC
{
	LPCSTR szFunc; // The name of the function to hook.
	PROC pProc; // The procedure to blast in.
} HOOKFUNCDESC, *LPHOOKFUNCDESC;

// ���ǵ�������
BOOL HookImportFunction(HMODULE hModule, LPCSTR szImportModule, LPHOOKFUNCDESC paHookFunc, PROC* paOrigFuncs);

