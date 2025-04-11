/*******************************************************************************
    Exclusion Monitor - мониторинг исключений Windows Defender
    Copyright (C) 2025 Egor Lvov

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*******************************************************************************/

#include <Windows.h>
#include <stdio.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <format>
#include "reg.hpp"
#include "notify.hpp"
#include "resource.h"

#define IDT_TIMER1 1001

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

HANDLE hIcon       = NULL;
HANDLE hIconGreen  = NULL;
HANDLE hIconRed    = NULL;
HANDLE hIconAlert  = NULL;
HWND hButtonEnable = NULL;
HWND hButtonHide   = NULL;

const char* defenderKeys[] = {
    "Paths",
    "Extensions",
    "Processes"
};
const size_t defKeysAmount = std::size(defenderKeys); 
exceptionsState states[defKeysAmount];
bool gWindowWantsToQuit = false;
bool gTimerSet = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int QueryRegistry(exceptionsState* states);

int WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmd, int nShowCmd) {
    /* Регистрируем класс */
    const char* CLASSNAME = "Exclusion Monitor";    // Название класса

    WNDCLASSA winClass;
    winClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;    // Стиль класса
    winClass.lpfnWndProc = &WndProc;                        // Обработчик сообщений
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = GetModuleHandle(NULL); // hInstance

    hIconGreen = LoadImageA(hinst, MAKEINTRESOURCE(Enabled), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
    hIconRed   = LoadImageA(hinst, MAKEINTRESOURCE(Disabled), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
    hIconAlert = LoadImageA(hinst, MAKEINTRESOURCE(Alert), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
    hIcon      = LoadImageA(hinst, MAKEINTRESOURCE(Logo), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);

    winClass.hIcon = HICON(hIcon);                           // Иконка класса
    winClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Курсор
    winClass.hbrBackground = (HBRUSH)COLOR_BACKGROUND;       // Кисть бэкграунда
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = CLASSNAME;

    if (!RegisterClassA(&winClass)) {
        MessageBoxA(NULL, "Error", "Error registering window class", MB_OK);
        return 1;
    }

    /* Создаём главное окно */
    const int width = 300;
    const int height = 190;
    const DWORD exStyle = WS_EX_APPWINDOW;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

    HWND hWnd = CreateWindowExA(exStyle, CLASSNAME, "Exclusion Monitor", style, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, NULL, NULL, winClass.hInstance, NULL);

    if (!hWnd) {
        return 2;
    }

    /* Задаём кнопки окна */
    hButtonEnable = CreateWindowA("BUTTON", "Enable monitor", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 25, 25, 100, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
    hButtonHide   = CreateWindowA("BUTTON", "Hide window",    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 25, 80, 100, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

    /* Задаём шрифт кнопкам */
    HDC hdc = GetDC(hWnd);
    long lfHeight = -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    long lfWeight = FW_BOLD;
    ReleaseDC(hWnd, hdc);

    HFONT hFont = CreateFont(lfHeight, 0, 0, 0, lfWeight, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma UI");
    SendMessage(hButtonEnable, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELONG(TRUE, 0));
    SendMessage(hButtonHide, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELONG(TRUE, 0));

    /* Создаём иконку в трее */
    if (!CreateTrayIcon(hWnd)) {
        return 4;
    }

    /* Основной цикл */
    while (!gWindowWantsToQuit) {
        MSG msg;
        /* Обработка сообщений */
        if (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        /* Чтобы не грузить ЦПУ на 100% */
        Sleep(10);
    }

    /* Чистимся перед выходом */
    if (gTimerSet) {
        KillTimer(hWnd, IDT_TIMER1);
    }
    DeleteObject(hFont);
    DestroyWindow(hButtonEnable);
    DestroyWindow(hWnd);
    UnregisterClassA(CLASSNAME, GetModuleHandle(NULL));

    return 0;
    UNREFERENCED_PARAMETER(prev);
    UNREFERENCED_PARAMETER(cmd);
    UNREFERENCED_PARAMETER(nShowCmd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    switch (uMsg) {
        case WM_COMMAND:
        {
            // Сообщение от кнопки интерфейса
            switch (HIWORD(wParam)) {
                case BN_CLICKED:
                    {
                        if ((HWND)lParam == hButtonEnable) {
                            if (!gTimerSet) {
                                if (QueryRegistry(states) != ERROR_SUCCESS) {
                                    return -1;
                                }
                                SetTimer(hWnd, IDT_TIMER1, 6000, (TIMERPROC)NULL);
                                ShowTrayIconBalloon("Monitoring enabled!", "Exclusion monitoring started", 3, 0, (char)gTimerSet);
                                SetWindowTextA(hButtonEnable, "Disable monitor");
                            } else {
                                KillTimer(hWnd, IDT_TIMER1);
                                ShowTrayIconBalloon("Monitoring disabled!", "Exclusion monitoring stopped", 3, 0, (char)gTimerSet);
                                SetWindowTextA(hButtonEnable, "Enable monitor");
                            }
                            gTimerSet = !gTimerSet;
                        } else if ((HWND)lParam == hButtonHide) {
                            ShowWindow(hWnd, SW_HIDE);
                        }
                    }
                    break;
            }

            // Сообщение от контекстного меню
            switch (LOWORD(wParam)) {
                case TRAYID_SHOW:
                    {
                        if (IsWindowVisible(hWnd)) {
                            ShowWindow(hWnd, SW_HIDE);
                        } else {
                            ShowWindow(hWnd, SW_SHOW);
                        }
                    }
                    break;

                case TRAYID_EXIT:
                {
                    SendMessageA(hWnd, WM_CLOSE, 0, 0);
                }
                break;
            }
        }
        break;

        // Обработка таймера проверки реестра
        case WM_TIMER:
        {
            exceptionsState oldstate[defKeysAmount];
            std::copy(std::begin(states), std::end(states), std::begin(oldstate));
            QueryRegistry(states);

            for (int i = 0; i < defKeysAmount; i++) {
                if (states[i].count    != oldstate[i].count ||
                    states[i].highTime != oldstate[i].highTime ||
                    states[i].lowTime  != oldstate[i].lowTime
                ) {
                    // ALARMA!!!
                    std::string message = std::format("Windows Defender {} exclusions have changed!", defenderKeys[i]);
                    ShowTrayIconBalloon("Exclusions changed!", message.c_str(), 5, 0, 2);
                    break;
                }
            }
        }
        break;

        // Закрытие окна
        case WM_CLOSE:
            gWindowWantsToQuit = true;
            break;

        // Пользовательское сообщение - ПКМ по иконке в трее (см. CreateTrayIcon в notify.cpp)
        case APPWM_ICONNOTIFY:
        {
            switch (LOWORD(lParam)) {
                case WM_RBUTTONUP:
                    BOOL isVisible = IsWindowVisible(hWnd);
                    POINT cursorPos = {};
                    GetCursorPos(&cursorPos);

                    HMENU hPopupMenu = CreatePopupMenu();
                    InsertMenu(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, TRAYID_SHOW, isVisible ? "Hide" : "Show");
                    InsertMenu(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, TRAYID_EXIT, "Exit");
                    TrackPopupMenu(hPopupMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, cursorPos.x, cursorPos.y, 0, hWnd, NULL);
                    DestroyMenu(hPopupMenu);
                    break;
            }

            break;
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

// Получение состояния ключей реестра
int QueryRegistry(exceptionsState* states) {
    int status;
    HKEY hBaseKey;
    status = OpenExclusions(&hBaseKey);
    if (status != ERROR_SUCCESS) {
        char* message = (char*)malloc(sizeof(char)*64);
        sprintf(message, "Error OpenExclusions: %d", status);
        MessageBoxA(NULL, message, "Error OpenExclusions", MB_OK);
        return status;
    }

    int i = 0;
    for (auto k : defenderKeys) {
        HKEY hSubKey;
        status = OpenExclSubkey(hBaseKey, k, &hSubKey);
        if (status != ERROR_SUCCESS) {
            char* message = (char*)malloc(sizeof(char)*64);
            sprintf(message, "Error opening subkey: %d", status);
            MessageBoxA(NULL, message, "Error OpenExclSubkey", MB_OK);
            return status;
        }

        if (i >= sizeof(defenderKeys)/sizeof(char*)) {
            MessageBoxA(NULL, "State array too small", "Error", MB_OK);
            return -1;
        }
        
        status = QueryKey(hSubKey, &states[i]);
        if (status != ERROR_SUCCESS) {
            char* message = (char*)malloc(sizeof(char)*64);
            sprintf(message, "Error query key: %d", status);
            MessageBoxA(NULL, message, "Error QueryKey", MB_OK);
            return status;
        }

        i++;
    }

    return ERROR_SUCCESS;
}