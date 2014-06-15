#pragma once

struct hook_func_t
{
	BYTE HookCode[8];
	BYTE SourceCode[8];
	FARPROC CodeAdress;
	DWORD SourceOld;
};

void HookOn(hook_func_t& hook_func);
void HookOff(hook_func_t& hook_func);
bool init(const char *dllname, const char *funcname, void* funcadress, hook_func_t& hook_func);

