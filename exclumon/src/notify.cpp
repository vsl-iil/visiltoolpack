#include <Windows.h>
#include <minwindef.h>
#include <shellapi.h>
#include <strsafe.h>
#include <winnt.h>
#include "resource.h"

extern HANDLE hIcon;
extern HANDLE hIconGreen;
extern HANDLE hIconRed;
extern HANDLE hIconAlert;
NOTIFYICONDATA m_NID;

BOOL CreateTrayIcon(HWND hWnd) {
    memset(&m_NID, 0, sizeof(m_NID));
    m_NID.cbSize = sizeof(m_NID);

    m_NID.uID = Logo;

    // set handle to the window that receives tray icon notifications
    m_NID.hWnd = hWnd;

    // fields that are being set when adding tray icon 
    m_NID.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    // set image
    m_NID.hIcon = HICON(hIcon);
    m_NID.uCallbackMessage = APPWM_ICONNOTIFY;
    StringCchCopy(m_NID.szTip, ARRAYSIZE(m_NID.szTip), "Exclusion Monitor");

    if (!m_NID.hIcon) {
        char* buf = (char*)malloc(64*sizeof(char));
        sprintf(buf, "Error: %lu", GetLastError());
        MessageBoxA(NULL, buf, "FOO", MB_OK);
        return FALSE;
    }

    m_NID.uVersion = NOTIFYICON_VERSION_4;

    if (!Shell_NotifyIcon(NIM_ADD, &m_NID)) {
        char* buf = (char*)malloc(64*sizeof(char));
        sprintf(buf, "Error: %lu", GetLastError());
        MessageBoxA(NULL, buf, "BAR", MB_OK);
        return FALSE;
    }

    return Shell_NotifyIcon(NIM_SETVERSION, &m_NID);
}

BOOL ShowTrayIconBalloon(LPCTSTR pszTitle, LPCTSTR pszText, UINT unTimeout, DWORD dwInfoFlags, char status) {
    m_NID.uFlags |= NIF_INFO;
    m_NID.uTimeout = unTimeout;
    m_NID.dwInfoFlags = dwInfoFlags;

    switch (status) {
        case 0:
            m_NID.hIcon = HICON(hIconGreen);
            break;
        case 1:
            m_NID.hIcon = HICON(hIconRed);
            break;
        case 2:
            m_NID.hIcon = HICON(hIconAlert);
            break;
    }

    if (StringCchCopy(m_NID.szInfoTitle, sizeof(m_NID.szInfoTitle), pszTitle) != S_OK)
        return FALSE;

    if (StringCchCopy(m_NID.szInfo, sizeof(m_NID.szInfo), pszText) != S_OK)
        return FALSE;

    return Shell_NotifyIcon(NIM_MODIFY, &m_NID);
}