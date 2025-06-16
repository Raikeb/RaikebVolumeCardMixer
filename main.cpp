// VolumeCardMixer - Passo 2: Janela com opções e seleção de monitor
// Autor: Raike
// Descrição: Adiciona checkboxes para Launch on Startup, Start Windowed e seleção de monitor

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>
#include <string>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_OPEN 1002
#define ID_CHECK_STARTUP 2001
#define ID_CHECK_WINDOWED 2002
#define ID_COMBO_MONITOR 2003

const wchar_t CLASS_NAME[] = L"VolumeCardMixerWindowClass";
HINSTANCE hInst;
NOTIFYICONDATA nid = {};
HWND hwndMain;
HWND hCheckStartup, hCheckWindowed, hComboMonitor;
std::vector<std::wstring> monitorNames;

void SaveSettings() {
    // TODO: salvar configurações em arquivo .ini ou registro
}

BOOL AddToStartup(bool enable) {
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    HKEY hKey;
    RegOpenKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
    if (enable) {
        RegSetValueEx(hKey, L"VolumeCardMixer", 0, REG_SZ, (BYTE*)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
    } else {
        RegDeleteValue(hKey, L"VolumeCardMixer");
    }
    RegCloseKey(hKey);
    return TRUE;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT, LPARAM lParam) {
    int* count = (int*)lParam;
    wchar_t name[32];
    swprintf_s(name, L"Monitor %d", (*count) + 1);
    monitorNames.push_back(name);
    (*count)++;
    return TRUE;
}

void PopulateMonitorList(HWND combo) {
    int count = 0;
    monitorNames.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&count);
    for (auto& name : monitorNames) {
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    }
    SendMessage(combo, CB_SETCURSEL, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Tray icon setup
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"Volume Card Mixer");
        Shell_NotifyIcon(NIM_ADD, &nid);

        // Checkboxes
        hCheckStartup = CreateWindow(L"BUTTON", L"Launch on Startup",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 20, 180, 20, hwnd, (HMENU)ID_CHECK_STARTUP, hInst, NULL);

        hCheckWindowed = CreateWindow(L"BUTTON", L"Start Windowed",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 50, 180, 20, hwnd, (HMENU)ID_CHECK_WINDOWED, hInst, NULL);

        // ComboBox para monitores
        CreateWindow(L"STATIC", L"Mostrar card no monitor:",
            WS_VISIBLE | WS_CHILD,
            20, 90, 200, 20, hwnd, NULL, hInst, NULL);

        hComboMonitor = CreateWindow(L"COMBOBOX", NULL,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            20, 110, 200, 200, hwnd, (HMENU)ID_COMBO_MONITOR, hInst, NULL);

        PopulateMonitorList(hComboMonitor);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN:
            ShowWindow(hwnd, SW_SHOW);
            break;
        case ID_TRAY_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        case ID_CHECK_STARTUP:
            AddToStartup(SendMessage(hCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN, L"Abrir");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Sair");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    hInst = hInstance;

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    hwndMain = CreateWindowEx(0, CLASS_NAME, L"Volume Card Mixer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
        NULL, NULL, hInstance, NULL);

    if (!hwndMain) return 0;

    ShowWindow(hwndMain, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}