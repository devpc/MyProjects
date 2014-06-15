#include "stdafx.h"
#include "ExtTextOutHook.h"

void HookOn(hook_func_t& hook_func)
{
//	OutputDebugString("HookOn");

	FARPROC CodeAdress = hook_func.CodeAdress;
	BYTE HookCode[8];
	memcpy(HookCode, hook_func.HookCode, sizeof(HookCode));

//	char text[1024];
//	sprintf(text, "%x, %x", CodeAdress, HookCode);
//	OutputDebugString(text);

	VirtualLock(CodeAdress, 32);
	VirtualProtect(CodeAdress, 32, PAGE_READWRITE, &hook_func.SourceOld);

	// 修改ExtTextOutW 前32个字节的页面属性为可写
	_asm
	{
		mov		eax, CodeAdress
		movq	mm0, HookCode
		movq	[eax], mm0
		emms
	}

	VirtualProtect(CodeAdress, 32, hook_func.SourceOld, &hook_func.SourceOld);
}

void HookOff(hook_func_t& hook_func)
{
//	OutputDebugString("HookOff");

	FARPROC CodeAdress = hook_func.CodeAdress;
	BYTE SourceCode[8];
	memcpy(SourceCode, hook_func.SourceCode, sizeof(SourceCode));

	VirtualProtect(CodeAdress, 32, PAGE_READWRITE, &hook_func.SourceOld);

	_asm
	{
		mov		eax, CodeAdress
		movq	mm0, SourceCode
		movq	[eax], mm0
		emms
	}

	VirtualProtect(CodeAdress, 32, hook_func.SourceOld, &hook_func.SourceOld);	
	VirtualUnlock(CodeAdress, 32);
}

bool init(const char *dllname, const char *funcname, void* funcadress, hook_func_t& hook_func)
{
	HMODULE hMoudle = GetModuleHandle(dllname);	
	FARPROC CodeAdress = GetProcAddress(hMoudle, funcname);

//	char text[1024];
//	sprintf(text, "%x, %x", hMoudle, CodeAdress);
//	OutputDebugString(text);

	if (CodeAdress == NULL)
	{
		OutputDebugString("Could not find Module");
		OutputDebugString(funcname);

		return false;
	}

	BYTE SourceCode[8] = {0};

	_asm
	{
		lea		edi, SourceCode
		mov		esi, CodeAdress
		cld
		movsd
		movsd
	}

	BYTE HookCode[8] = {0};
	HookCode[0] = 0xe9;

	_asm
	{
		mov		eax, funcadress
		mov		ebx, CodeAdress
		sub		eax, ebx
		sub		eax, 5
		mov		dword ptr[HookCode+1], eax
	}

	hook_func.CodeAdress = CodeAdress;
	memcpy(hook_func.SourceCode, SourceCode, sizeof(hook_func.SourceCode));
	memcpy(hook_func.HookCode, HookCode, sizeof(hook_func.HookCode));

	OutputDebugString("init");

	HookOn(hook_func);

	return true;
}

