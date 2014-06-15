// BidHookDll.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ApiHook.h"
#include "WindowsHook.h"
#include "ExtTextOutHook.h"
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <openssl/ssl.h>
#include <process.h>

//#pragma data_seg(".DLLSharedSection")      // 声明共享数据段，并命名该数据段
//int init_count = 0;
//#pragma data_seg()

//bool injected = false;

//#pragma comment(linker,"/section:.DLLSharedSection,rws")

char id_code[7] = {0};
int id_code_num = 0;

int high_bid_money = 0; //接收到系统最高出价
int cur_bid_money = 0; //我的当前出价

int second_bit_incre = 600;

FILE *log_file = NULL;
FILE *log_file_w = NULL;

HWND hWndImageForm = NULL; //输入验证码的对话框
HHOOK hEditHooker;
HINSTANCE s_hModule;
HWND hWndMoneyEdit = NULL;
HWND hWndCodeEdit = NULL;
const char *MoneyEditName = "TNoPastedEdit";
//const char *MoneyEditName = "TEdit";

void OpenLogFiles()
{
	log_file = fopen("C:\\bid_log_file.txt", "wb");
	log_file_w = fopen("C:\\bid_log_file_w.txt", "wb");
	WORD Unicode = 0xFEFF; // UNICODE BOM
	fwrite(&Unicode, 2, 1, log_file_w);
}

void CloseLogFiles()
{
	fclose(log_file);
	fclose(log_file_w);
}

const char *GetDateString()
{
	time_t t = time(NULL);
	tm *cur_tm = localtime(&t);

	static char text[1024];
	sprintf(text, "%d:%d:%d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
	return text;
}

void Log(const char *prefix, const void *buf, int len, FILE *file)
{
	char text[1024];
	sprintf(text, "[%s][%s]", GetDateString(), prefix);

	fwrite(text, strlen(text), 1, file);
	fwrite(buf, len, 1, file);
	fwrite("\n", 1, 1, file);

	fflush(file);
}

const wchar_t *GetDateStringW()
{
	time_t t = time(NULL);
	tm *cur_tm = localtime(&t);

	static wchar_t text[1024];
	wsprintfW(text, L"%d:%d:%d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
	return text;
}

void LogW(const wchar_t *prefix, const void *buf, int len, FILE *file)
{
	wchar_t text[1024];
	wsprintfW(text, L"[%s][%s]", GetDateStringW(), prefix);

	fwrite(text, lstrlenW(text), 2, file);
	fwrite(buf, len, 2, file);
	fwrite(L"\n", 1, 2, file);

	fflush(file);
}

void InitBidEditHook()
{
	HWND hWnd = FindWindow("TMainForm", NULL);
	if (hWnd)
	{
		HWND wnd = FindWindowEx(hWnd, NULL, MoneyEditName, NULL);
		if (wnd)
		{
			wnd = FindWindowEx(hWnd, wnd, MoneyEditName, NULL);
			if (wnd)
			{
				hWndMoneyEdit = wnd;

				OutputDebugString("InitBidEditHook 成功");
			}
		}
	}
}

void SetBidEditText(int money)
{
	if (hWndMoneyEdit)
	{
		char str_num[128] = {0};
		sprintf(str_num, "%d", money);
//		OutputDebugStringA(str_num);
		SetWindowText(hWndMoneyEdit, str_num);
	}
}

void SetImageFormIdCode(HWND hWnd)
{
	HWND wnd = FindWindowEx(hWnd, NULL, "TEdit", NULL);

	if (wnd)
	{
		SetWindowText(wnd, id_code);

		id_code_num = 0;
		memset(id_code, 0, sizeof(id_code));

		PostMessage(hWnd, WM_LBUTTONDOWN, 1, 0x00DF00AC); // 模拟鼠标按键
		PostMessage(hWnd, WM_LBUTTONUP, 1, 0x00DF00AC); // 模拟鼠标按键
	}
}

void ShowImagFormWnd()
{
	HWND hWnd = FindWindow("TMainForm", NULL);
	if (hWnd)
	{
		// 自动出价
		PostMessage(hWnd, WM_LBUTTONDOWN, 1, 0x019D032D); // 模拟鼠标按键
		PostMessage(hWnd, WM_LBUTTONUP, 1, 0x019D032D); // 模拟鼠标按键
	}
}

bool CheckThenSendCode()
{
	char str[128] = {0};
	sprintf(str, "cur_bid_money = %d, high_bid_money = %d\n", cur_bid_money, high_bid_money);
	OutputDebugString(str);

	if (cur_bid_money > 0 && high_bid_money >= cur_bid_money && hWndImageForm && hWndCodeEdit) 
	{
		char str_code[128] = {0};
		GetWindowTextA(hWndCodeEdit, str_code, sizeof(str_code)-1);

		OutputDebugString(str_code);

		if (strlen(str_code) >= 6)
		{
			OutputDebugString("满足条件，自动发送验证码");

			PostMessage(hWndImageForm, WM_LBUTTONDOWN, 1, 0x00DF00AC); // 模拟鼠标按键
			PostMessage(hWndImageForm, WM_LBUTTONUP, 1, 0x00DF00AC); // 模拟鼠标按键

			hWndImageForm = NULL;
			hWndCodeEdit = NULL;

			return true;
		}
	}

	return false;
}

bool CheckLowestBidMoney(wchar_t *local_string, int len)
{
	const wchar_t *key_str = L"目前最低可成交价：";
	const int short_len = 9;

	if (len > short_len && wcsncmp(key_str, local_string, short_len) == 0 && !(local_string[len-1] >= 0x30 && local_string[len-1] <= 0x39))
	{
		local_string[len-1] = 0;

		OutputDebugStringW(local_string+short_len);

		high_bid_money = _wtoi(local_string+short_len) + 300;

/*		if (cur_bid_money && high_bid_money > cur_bid_money) // 已经弹出过一次验证码对话框
		{
			SetBidEditText(high_bid_money);
		}
*/
		CheckThenSendCode();

		return true;
	}

	return false;
}

hook_func_t ExtTextOutW_func;

BOOL WINAPI myExtTextOutW(HDC hdc, int x, int y, UINT options, CONST RECT * lprect, LPCWSTR lpString, UINT c, CONST INT * lpDx)
{
	HookOff(ExtTextOutW_func);
	BOOL retval = ExtTextOutW(hdc, x, y, options, lprect, lpString, c, lpDx);
	HookOn(ExtTextOutW_func);

	wchar_t local_string[1024] = {0};

	lstrcpyW(local_string, lpString);

	int len = lstrlenW(local_string);

	LogW(L"TextOut", local_string, len, log_file_w);

	if (len && *local_string >= 0x30 && *local_string <= 0x39)
	{
		if (len > 1 && local_string[1] == 0x0012)
		{
//			if (id_code_num < 6)
//			{
//				id_code[id_code_num] = (char)local_string[0];
//				id_code_num ++;
//				id_code[id_code_num] = 0;
//			}

			if (id_code_num == 6 && hWndImageForm)
			{
				SetImageFormIdCode(hWndImageForm);
				hWndImageForm = NULL;
			}
		}
	}
	else
	{
		wchar_t local_str[1024] = {0};

		lstrcpyW(local_str, lpString);
		CheckLowestBidMoney(local_str, len);

//		lstrcpyW(local_str, lpString);
//		CheckMyBidMoney(local_str, len);
	}

	return retval;
}

hook_func_t EnableWindow_func;
BOOL WINAPI myEnableWindow(HWND hWnd, BOOL bEnable)
{
	char text[1024];
	sprintf(text, "myEnableWindow, %x, %d", hWnd, bEnable);
	OutputDebugStringA(text);

	if (bEnable == FALSE)
	{
		return 0;
	}

	HookOff(EnableWindow_func);
	BOOL retval = EnableWindow(hWnd, bEnable);
	HookOn(EnableWindow_func);

	return retval;
}

hook_func_t SetWindowText_func;
BOOL WINAPI mySetWindowText(HWND hWnd, LPCSTR lpString)
{
	char text[1024];
	sprintf(text, "mySetWindowText, %x, %s", hWnd, lpString);
	OutputDebugStringA(text);

	HookOff(SetWindowText_func);
	BOOL retval = SetWindowTextA(hWnd, lpString);
	HookOn(SetWindowText_func);

	return retval;
}

hook_func_t DefWindowProc_func;
LRESULT WINAPI myDefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HookOff(DefWindowProc_func);
	LRESULT retval = DefWindowProcA(hWnd, Msg, wParam, lParam);
	HookOn(DefWindowProc_func);

	return retval;
}

hook_func_t SendMessage_func;
LRESULT WINAPI mySendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (hWnd == hWndMoneyEdit)
	{
		
	}
	else if (hWnd == hWndCodeEdit)
	{
		OutputDebugString("mySendMessage");
		CheckThenSendCode();
	}

	HookOff(SendMessage_func);
	LRESULT retval = SendMessageA(hWnd, Msg, wParam, lParam);
	HookOn(SendMessage_func);

	return retval;
}

hook_func_t PostMessage_func;
LRESULT WINAPI myPostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (hWnd == hWndMoneyEdit)
	{
		char text[128];
		sprintf(text, "window msg : %x", Msg); 
		OutputDebugString(text);

		*text = 0;
		int len = GetWindowText(hWnd, text, sizeof(text)-1);
		text[len] = 0;

		int m = atoi(text);

		if (m > 0)
		{
//			last_bid_money = m;
//			OutputDebugString(text);
		}
	}
	else if (hWnd == hWndCodeEdit)
	{
		OutputDebugString("myPostMessage");
		CheckThenSendCode();
	}

	HookOff(PostMessage_func);
	LRESULT retval = PostMessageA(hWnd, Msg, wParam, lParam);
	HookOn(PostMessage_func);

	return retval;
}

hook_func_t CloseWindow_func;
BOOL WINAPI myCloseWindow(HWND hWnd)
{
	char text[1024];
	sprintf(text, "CloseWindow_func, %x", hWnd);
	OutputDebugStringA(text);

	HookOff(CloseWindow_func);
	BOOL retval = CloseWindow(hWnd);
	HookOn(CloseWindow_func);

	return retval;
}

void UpdateCurBidMoney()
{
	char text[128] = {0};
	int len = GetWindowText(hWndMoneyEdit, text, sizeof(text)-1);
	text[len] = 0;

	std::string str = "UpdateCurBidMoney ";
	str += text;
	OutputDebugStringA(str.data());

	int m = atoi(text);

	if (m > 0)
	{
		cur_bid_money = m;
	}

	sprintf(text, "当前出价, %d", cur_bid_money);
	OutputDebugStringA(text);
}

hook_func_t ShowWindow_func;
BOOL WINAPI myShowWindow(HWND hWnd, int nCmdShow)
{
	HookOff(ShowWindow_func);
	BOOL retval = ShowWindow(hWnd, nCmdShow);
	HookOn(ShowWindow_func);

	char class_name[1024];
	int len = GetClassName(hWnd, class_name, 128);
	class_name[len] = 0;

	char text[1024];
	sprintf(text, "myShowWindow, %s, %x, %d, %s", class_name, hWnd, nCmdShow, id_code);
	OutputDebugStringA(text);

	if (stricmp(class_name, "TImageForm") == 0 && nCmdShow == 1)
	{
		if (hWndImageForm)
		{
			BOOL v = IsWindowVisible(hWndImageForm);

//			sprintf(text, "window visiable, %x, %d", hWndImageForm, v);
//			OutputDebugStringA(text);

//			CloseWindow(hWndImageForm);
		}

		hWndImageForm = hWnd;

		if (id_code_num > 0) // 如果正确输入了验证码再自动按键
		{
			SetImageFormIdCode(hWnd);
		}

		UpdateCurBidMoney();

		hWndCodeEdit = FindWindowEx(hWndImageForm, NULL, "TEdit", NULL);
	}
	else if (stricmp(class_name, "TImageCodeForm") == 0 && nCmdShow == 1)
	{
		hWndImageForm = hWnd;

		if (id_code_num > 0) // 如果正确输入了验证码再自动按键
		{
			SetImageFormIdCode(hWnd);
		}

		UpdateCurBidMoney();

		hWndCodeEdit = FindWindowEx(hWndImageForm, NULL, "TEdit", NULL);
	}
	else if (stricmp(class_name, "TErrorBoxForm") == 0)
	{
		HWND wnd = FindWindowEx(hWnd, NULL, "TMemo", NULL);

		if (wnd)
		{
			*text = 0;
			GetWindowText(wnd, text, 512);
			OutputDebugString(text);

			std::string str = text;

			if (str.find("出价成功") != std::string::npos)
			{
				//  第二次出价的时机，先放在这里
				if (cur_bid_money) // 再出价一次
				{
					PostMessage(hWnd, WM_LBUTTONDOWN, 1, 0x00E800F2); // 模拟鼠标按键
					PostMessage(hWnd, WM_LBUTTONUP, 1, 0x00E800F2); // 模拟鼠标按键

					OutputDebugString("===========================");

					SetBidEditText(cur_bid_money+second_bit_incre);

					ShowImagFormWnd();
				}
			}
		}
	}

	return retval;
}

hook_func_t gethostbyname_func;
struct hostent* WSAAPI my_gethostbyname(const char *name)
{
	char text[1024];

	sprintf(text, "my_gethost: %s", name);
	OutputDebugString(text);

	HookOff(gethostbyname_func);
	struct hostent *retval = gethostbyname(name);
	HookOn(gethostbyname_func);

	return retval;
}

hook_func_t connect_func;
int WSAAPI my_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
	sockaddr_in *sa = (struct sockaddr_in*)name;

	HookOff(connect_func);
	int retval = connect(s, name, namelen);
	HookOn(connect_func);

	char text[1024];
	sprintf(text, "my_connect: %s, %d, %d, %d", inet_ntoa(sa->sin_addr), htons(sa->sin_port), s, retval);
	OutputDebugString(text);

	return retval;
}

hook_func_t recv_func;
int WSAAPI my_recv(SOCKET s, char *buf, int len, int flags)
{
	HookOff(recv_func);
	int retval = recv(s, buf, len, flags);
	HookOn(recv_func);

	if (retval > 0)
	{
		Log("myrecv", buf, retval, log_file);
	}

	return retval;
}

hook_func_t send_func;
int WSAAPI my_send(SOCKET s, const char *buf, int len, int flags)
{
	Log("mysend", buf, len, log_file);

	HookOff(send_func);
	int retval = send(s, buf, len, flags);
	HookOn(send_func);

	return retval;
}

hook_func_t ssl_read_func;
int my_SSL_read(SSL *ssl,void *buf,int num)
{
	Log("ssl_read", buf, num, log_file);

	HookOff(ssl_read_func);
	int retval = SSL_read(ssl, buf, num);
	HookOn(ssl_read_func);

	return retval;
}

hook_func_t ssl_write_func;
int my_SSL_write(SSL *ssl,const void *buf,int num)
{
	Log("ssl_write", buf, num, log_file);

	HookOff(ssl_write_func);
	int retval = SSL_write(ssl, buf, num);
	HookOn(ssl_write_func);

	return retval;
}

void StartApiHook(HINSTANCE hModule);

unsigned int __stdcall runBidTimer(LPVOID)
{
	bool first_bid = false;

	while (true)
	{
		time_t cur_t = time(NULL);

		tm cur_tm = *localtime(&cur_t);

//		if (cur_tm.tm_hour == 11 && cur_tm.tm_min == 29)
		{
			if (cur_tm.tm_sec >= 20 && first_bid == false && hWndImageForm == NULL)
			{
				SetBidEditText(high_bid_money+300);
				ShowImagFormWnd();

				first_bid = true;
			}
		}

		Sleep(100);
	}

	return 0;
}

BOOL APIENTRY DllMain( HINSTANCE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			s_hModule = hModule;

//			StartApiHook(GetModuleHandle(NULL));
//			StartWindowsHook(hModule);
			
			OpenLogFiles();

//			_beginthreadex(0,0,(unsigned int (__stdcall *)(void *))runBidTimer,0,0,0);

			init("gdi32.dll", "ExtTextOutW", &myExtTextOutW, ExtTextOutW_func);
			init("ws2_32.dll", "gethostbyname", &my_gethostbyname, gethostbyname_func);
//			init("ws2_32.dll", "connect", &my_connect, connect_func);
//			init("ws2_32.dll", "recv", &my_recv, recv_func);
//			init("ws2_32.dll", "send", &my_send, send_func);
			init("user32.dll", "EnableWindow", &myEnableWindow, EnableWindow_func);
			init("user32.dll", "ShowWindow", &myShowWindow, ShowWindow_func);
			init("user32.dll", "CloseWindow", &myCloseWindow, CloseWindow_func);
			init("user32.dll", "SetWindowTextA", &mySetWindowText, SetWindowText_func);
			init("user32.dll", "DefWindowProcA", &myDefWindowProc, DefWindowProc_func);
			init("user32.dll", "SendMessageA", &mySendMessage, SendMessage_func);
			init("user32.dll", "PostMessageA", &myPostMessage, PostMessage_func);

			init("ssleay32.dll", "SSL_read", &my_SSL_read, ssl_read_func);
			init("ssleay32.dll", "SSL_write", &my_SSL_write, ssl_write_func);

			int ret = MessageBox(NULL, "请选择第二次出价加价幅度，选是加600，选否加300。", "选择出价",  MB_YESNO);

			if (ret == IDNO)
			{
				second_bit_incre = 300;
			}
			else if (ret == IDYES)
			{
				second_bit_incre = 600;
			}

			InitBidEditHook();

			return true;
		}
		break;

		case DLL_PROCESS_DETACH:
		{
			HookOff(ExtTextOutW_func);
			HookOff(gethostbyname_func);
//			HookOff(connect_func);
//			HookOff(recv_func);
//			HookOff(send_func);
			HookOff(EnableWindow_func);
			HookOff(ShowWindow_func);
			HookOff(CloseWindow_func);
			HookOff(ssl_write_func);
			HookOff(ssl_read_func);
			HookOff(SetWindowText_func);
			HookOff(DefWindowProc_func);
			HookOff(SendMessage_func);
			HookOff(PostMessage_func);

			CloseLogFiles();
		}
		break;

//		case DLL_THREAD_ATTACH:
//		case DLL_THREAD_DETACH:

    }
    return TRUE;
}
