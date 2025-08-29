#pragma once

// 最小化Windows API声明 - 避免头文件冲突
// 只声明CutMask.cpp实际需要的Windows API函数和类型

#ifndef _MINIMAL_WINDOWS_API_
#define _MINIMAL_WINDOWS_API_

// 基本类型定义
typedef void* HWND;
typedef void* HMODULE;
typedef long LONG;
typedef unsigned long DWORD;
typedef int BOOL;
typedef long long LPARAM;

// RECT结构体
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

// 常量定义
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// DWM常量
#define DWMWA_EXTENDED_FRAME_BOUNDS 9

// 函数声明 - 只声明我们需要的函数
extern "C" {
    // 来自user32.dll
    BOOL __stdcall EnumWindows(BOOL(__stdcall* lpEnumFunc)(HWND, LPARAM), LPARAM lParam);
    BOOL __stdcall IsWindowVisible(HWND hWnd);
    BOOL __stdcall IsIconic(HWND hWnd);
    int __stdcall GetWindowTextLength(HWND hWnd);
    
    // 来自dwmapi.dll
    LONG __stdcall DwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute);
}

// 链接必要的库
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

#endif // _MINIMAL_WINDOWS_API_
