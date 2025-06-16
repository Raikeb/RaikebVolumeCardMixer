// VolumeCardMixer - Passo 4: Volume de processos e card flutuante v3
// Autor: Raike

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <shlobj.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <processthreadsapi.h>
#include <vector>
#include <string>
#include <map>
extern "C" BOOL WINAPI QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);

// Forward declarations for audio interfaces
class IAudioSessionManager2;
class IAudioSessionEnumerator;
class IAudioSessionControl;
class IAudioSessionControl2;

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_OPEN 1002
#define ID_CHECK_STARTUP 2001
#define ID_CHECK_WINDOWED 2002
#define ID_COMBO_MONITOR 2003
#define TIMER_ID 3001
#define VOLUME_CHECK_INTERVAL 500  // ms

const wchar_t CLASS_NAME[] = L"VolumeCardMixerWindowClass";
const wchar_t CARD_CLASS_NAME[] = L"VolumeCardMixerCardClass";

HINSTANCE hInst;
NOTIFYICONDATA nid = {};
HWND hwndMain, hwndCard = NULL;
HWND hCheckStartup, hCheckWindowed, hComboMonitor;
std::vector<std::wstring> monitorNames;

struct AudioSessionInfo {
    DWORD pid;
    std::wstring name;
    float volume;
    bool isSystemSounds;
};

std::map<DWORD, AudioSessionInfo> audioSessions;
UINT_PTR timerId = 0;

struct AppConfig {
    bool launchOnStartup = false;
    bool startWindowed = false;
    int selectedMonitor = 0;
} config;

void LoadSettings() {
    HKEY hKey;
    DWORD val, size = sizeof(DWORD);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, L"LaunchOnStartup", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.launchOnStartup = (val != 0);
        if (RegQueryValueEx(hKey, L"StartWindowed", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.startWindowed = (val != 0);
        if (RegQueryValueEx(hKey, L"SelectedMonitor", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.selectedMonitor = val;
        RegCloseKey(hKey);
    }
}

void SaveSettings() {
    HKEY hKey;
    DWORD disp;
    RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp);
    DWORD val;
    val = (config.launchOnStartup ? 1 : 0);
    RegSetValueEx(hKey, L"LaunchOnStartup", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    val = (config.startWindowed ? 1 : 0);
    RegSetValueEx(hKey, L"StartWindowed", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    val = (DWORD)config.selectedMonitor;
    RegSetValueEx(hKey, L"SelectedMonitor", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    RegCloseKey(hKey);
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
    SendMessage(combo, CB_SETCURSEL, config.selectedMonitor, 0);
}

LRESULT CALLBACK CardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        // Fundo
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        
        // Texto
        wchar_t text[128];
        GetWindowText(hwnd, text, 128);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hwnd, 1);
            ShowWindow(hwnd, SW_HIDE);
            DestroyWindow(hwnd);
            hwndCard = NULL;
        }
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowFloatingCard(const std::wstring& processName, int volume) {
    if (hwndCard) {
        KillTimer(hwndCard, 1);
        DestroyWindow(hwndCard);
    }

    // Posiciona no monitor selecionado
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(hwndMain, MONITOR_DEFAULTTONEAREST), &mi);
    int x = mi.rcMonitor.left + 50;
    int y = mi.rcMonitor.bottom - 150;

    hwndCard = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CARD_CLASS_NAME,
        NULL,
        WS_POPUP,
        x, y, 200, 60,
        NULL, NULL, hInst, NULL);

    std::wstring text = processName + L"\nVolume: " + std::to_wstring(volume) + L"%";
    SetWindowText(hwndCard, text.c_str());
    
    // Estilo visual
    SetLayeredWindowAttributes(hwndCard, 0, 220, LWA_ALPHA);
    
    ShowWindow(hwndCard, SW_SHOWNOACTIVATE);
    UpdateWindow(hwndCard);
    
    // Esconde após 3 segundos
    SetTimer(hwndCard, 1, 3000, NULL);
}

void CheckAudioSessions() {
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager2* pSessionManager = NULL;
    IAudioSessionEnumerator* pSessionEnumerator = NULL;
    IAudioSessionControl* pSessionControl = NULL;
    IAudioSessionControl2* pSessionControl2 = NULL;
    ISimpleAudioVolume* pAudioVolume = NULL;

    CoInitialize(NULL);

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    if (SUCCEEDED(hr)) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    }

    if (SUCCEEDED(hr)) {
        hr = pDevice->Activate(
            __uuidof(IAudioSessionManager2), CLSCTX_ALL,
            NULL, (void**)&pSessionManager);
    }

    if (SUCCEEDED(hr)) {
        hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
    }

    int sessionCount = 0;
    if (SUCCEEDED(hr) && pSessionEnumerator) {
        hr = pSessionEnumerator->GetCount(&sessionCount);
    }

    for (int i = 0; i < sessionCount; i++) {
        hr = pSessionEnumerator->GetSession(i, &pSessionControl);

        if (SUCCEEDED(hr)) {
            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
        }

        if (SUCCEEDED(hr)) {
            DWORD pid;
            hr = pSessionControl2->GetProcessId(&pid);

            if (SUCCEEDED(hr)) {
                hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pAudioVolume);
            }

            if (SUCCEEDED(hr)) {
                float volume;
                hr = pAudioVolume->GetMasterVolume(&volume);

                if (SUCCEEDED(hr)) {
                    BOOL isSystemSounds = pSessionControl2->IsSystemSoundsSession() == S_OK;

                    LPWSTR pswName = NULL;
                    pSessionControl2->GetSessionIdentifier(&pswName);

                    std::wstring sessionName;
                    if (isSystemSounds) {
                        sessionName = L"System Sounds";
                    } else {
                        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                        if (hProcess) {
                            wchar_t exePath[MAX_PATH];
                            DWORD pathSize = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &pathSize)) {
                                sessionName = exePath;
                                size_t pos = sessionName.find_last_of(L"\\");
                                if (pos != std::wstring::npos) {
                                    sessionName = sessionName.substr(pos + 1);
                                }
                            }
                            CloseHandle(hProcess);
                        }
                    }

                    if (!sessionName.empty()) {
                        int newVolume = (int)(volume * 100);
                        auto it = audioSessions.find(pid);
                        if (it != audioSessions.end()) {
                            if (abs(it->second.volume - volume) > 0.01f) {
                                it->second.volume = volume;
                                ShowFloatingCard(sessionName, newVolume);
                            }
                        } else {
                            AudioSessionInfo info = { pid, sessionName, volume, isSystemSounds != FALSE };
                            audioSessions[pid] = info;
                            ShowFloatingCard(sessionName, newVolume);
                        }
                    }

                    if (pswName) {
                        CoTaskMemFree(pswName);
                    }
                }
            }
        }

        if (pAudioVolume) {
            pAudioVolume->Release();
            pAudioVolume = NULL;
        }
        if (pSessionControl2) {
            pSessionControl2->Release();
            pSessionControl2 = NULL;
        }
        if (pSessionControl) {
            pSessionControl->Release();
            pSessionControl = NULL;
        }
    }

    // Limpa sessões que não existem mais
    for (auto it = audioSessions.begin(); it != audioSessions.end(); ) {
        bool found = false;
        for (int i = 0; i < sessionCount; i++) {
            IAudioSessionControl* pTempSession = NULL;
            if (SUCCEEDED(pSessionEnumerator->GetSession(i, &pTempSession))) {
                IAudioSessionControl2* pTempSession2 = NULL;
                if (SUCCEEDED(pTempSession->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pTempSession2))) {
                    DWORD tempPid;
                    if (SUCCEEDED(pTempSession2->GetProcessId(&tempPid))) {
                        if (tempPid == it->first) {
                            found = true;
                        }
                    }
                    pTempSession2->Release();
                }
                pTempSession->Release();
            }
        }
        
        if (!found) {
            it = audioSessions.erase(it);
        } else {
            ++it;
        }
    }

    if (pSessionEnumerator) {
        pSessionEnumerator->Release();
    }
    if (pSessionManager) {
        pSessionManager->Release();
    }
    if (pDevice) {
        pDevice->Release();
    }
    if (pEnumerator) {
        pEnumerator->Release();
    }

    CoUninitialize();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"Volume Card Mixer");
        Shell_NotifyIcon(NIM_ADD, &nid);

        hCheckStartup = CreateWindow(L"BUTTON", L"Launch on Startup",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 20, 180, 20, hwnd, (HMENU)ID_CHECK_STARTUP, hInst, NULL);
        SendMessage(hCheckStartup, BM_SETCHECK, config.launchOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);

        hCheckWindowed = CreateWindow(L"BUTTON", L"Start Windowed",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 50, 180, 20, hwnd, (HMENU)ID_CHECK_WINDOWED, hInst, NULL);
        SendMessage(hCheckWindowed, BM_SETCHECK, config.startWindowed ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindow(L"STATIC", L"Mostrar card no monitor:",
            WS_VISIBLE | WS_CHILD,
            20, 90, 200, 20, hwnd, NULL, hInst, NULL);

        hComboMonitor = CreateWindow(L"COMBOBOX", NULL,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            20, 110, 200, 200, hwnd, (HMENU)ID_COMBO_MONITOR, hInst, NULL);

        PopulateMonitorList(hComboMonitor);

        // Inicia o timer para verificar o volume periodicamente
        timerId = SetTimer(hwnd, TIMER_ID, VOLUME_CHECK_INTERVAL, NULL);
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            CheckAudioSessions();
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN:
            ShowWindow(hwnd, SW_SHOW);
            break;
        case ID_TRAY_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            KillTimer(hwnd, timerId);
            PostQuitMessage(0);
            break;
        case ID_CHECK_STARTUP:
            config.launchOnStartup = SendMessage(hCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
            AddToStartup(config.launchOnStartup);
            SaveSettings();
            break;
        case ID_CHECK_WINDOWED:
            config.startWindowed = SendMessage(hCheckWindowed, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SaveSettings();
            break;
        case ID_COMBO_MONITOR:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                config.selectedMonitor = (int)SendMessage(hComboMonitor, CB_GETCURSEL, 0, 0);
                SaveSettings();
            }
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
        KillTimer(hwnd, timerId);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    hInst = hInstance;
    LoadSettings();

    // Registra a classe da janela principal
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Registra a classe do card flutuante
    WNDCLASS cardWc = {};
    cardWc.lpfnWndProc = CardWndProc;
    cardWc.hInstance = hInstance;
    cardWc.lpszClassName = CARD_CLASS_NAME;
    cardWc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&cardWc);

    hwndMain = CreateWindowEx(0, CLASS_NAME, L"Volume Card Mixer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
        NULL, NULL, hInstance, NULL);

    if (!hwndMain) return 0;

    ShowWindow(hwndMain, config.startWindowed ? SW_HIDE : SW_SHOW);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}