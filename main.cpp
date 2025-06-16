// VolumeCardMixer - Passo 4: Volume de processos e card flutuante v5
// Autor: Raike

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm> // Adicionado

// Forward declarations
void ProcessAudioSession(IAudioSessionControl2 *pSessionControl2, IAudioSessionControl *pSessionControl);
std::wstring GetSessionName(IAudioSessionControl2 *pSessionControl2, DWORD pid, BOOL isSystemSounds);
void UpdateSessionInfo(DWORD pid, const std::wstring &sessionName, float volume, BOOL isSystemSounds, int newVolume);
void CleanupStaleSessions(IAudioSessionEnumerator *pSessionEnumerator, int activeSessionCount);

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
#define VOLUME_CHECK_INTERVAL 500 // ms

const wchar_t CLASS_NAME[] = L"VolumeCardMixerWindowClass";
const wchar_t CARD_CLASS_NAME[] = L"VolumeCardMixerCardClass";

HINSTANCE hInst;
NOTIFYICONDATA nid = {};
HWND hwndMain, hwndCard = NULL;
HWND hCheckStartup, hCheckWindowed, hComboMonitor;
std::vector<std::wstring> monitorNames;

struct AudioSessionInfo
{
    DWORD pid;
    std::wstring name;
    float volume;
    bool isSystemSounds;
};

std::map<DWORD, AudioSessionInfo> audioSessions;
UINT_PTR timerId = 0;

struct AppConfig
{
    bool launchOnStartup = false;
    bool startWindowed = false;
    int selectedMonitor = 0;
} config;

std::wofstream logFile;

void OpenLogFile()
{
    std::string logFileName = "VolumeCardMixer.log";
    logFile.open(logFileName, std::ios::out | std::ios::app);
    if (logFile.is_open())
    {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);

        std::tm tm;
        localtime_s(&tm, &now_time);

        logFile << L"\n\n=== Log started at " << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S") << L" ===\n";
        logFile.flush();
    }
}

void LogMessage(const std::wstring &message)
{
    if (logFile.is_open())
    {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);

        std::tm tm;
        localtime_s(&tm, &now_time);

        logFile << std::put_time(&tm, L"[%H:%M:%S] ") << message << std::endl;
        logFile.flush();
    }
#ifdef _DEBUG
    OutputDebugStringW((message + L"\n").c_str());
#endif
}

void CloseLogFile()
{
    if (logFile.is_open())
    {
        logFile << L"=== Log ended ===\n";
        logFile.close();
    }
}

void LoadSettings()
{
    HKEY hKey;
    DWORD val, size = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(hKey, L"LaunchOnStartup", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.launchOnStartup = (val != 0);
        if (RegQueryValueExW(hKey, L"StartWindowed", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.startWindowed = (val != 0);
        if (RegQueryValueExW(hKey, L"SelectedMonitor", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.selectedMonitor = val;
        RegCloseKey(hKey);
    }
}

void SaveSettings()
{
    HKEY hKey;
    DWORD disp;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp);
    DWORD val;
    val = (config.launchOnStartup ? 1 : 0);
    RegSetValueExW(hKey, L"LaunchOnStartup", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    val = (config.startWindowed ? 1 : 0);
    RegSetValueExW(hKey, L"StartWindowed", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    val = (DWORD)config.selectedMonitor;
    RegSetValueExW(hKey, L"SelectedMonitor", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
    RegCloseKey(hKey);
}

BOOL AddToStartup(bool enable)
{
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    HKEY hKey;
    RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
    if (enable)
    {
        RegSetValueExW(hKey, L"VolumeCardMixer", 0, REG_SZ, (BYTE *)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
    }
    else
    {
        RegDeleteValueW(hKey, L"VolumeCardMixer");
    }
    RegCloseKey(hKey);
    return TRUE;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT, LPARAM lParam)
{
    int *count = (int *)lParam;
    wchar_t name[32];
    swprintf_s(name, L"Monitor %d", (*count) + 1);
    monitorNames.push_back(name);
    (*count)++;
    return TRUE;
}

void PopulateMonitorList(HWND combo)
{
    SendMessage(combo, CB_RESETCONTENT, 0, 0);

    DISPLAY_DEVICE displayDevice;
    ZeroMemory(&displayDevice, sizeof(displayDevice));
    displayDevice.cb = sizeof(displayDevice);

    int monitorIndex = 0;
    for (int i = 0; EnumDisplayDevices(NULL, i, &displayDevice, 0); i++)
    {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
        {
            std::wstring monitorName = L"Monitor " + std::to_wstring(monitorIndex + 1);
            if (wcslen(displayDevice.DeviceString) > 0)
            {
                monitorName += L" (" + std::wstring(displayDevice.DeviceString) + L")";
            }
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)monitorName.c_str());
            monitorIndex++;
        }
    }

    SendMessage(combo, CB_SETCURSEL, config.selectedMonitor, 0);
}

LRESULT CALLBACK CardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
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
        if (wParam == 1)
        {
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

void ShowFloatingCard(const std::wstring &processName, int volume)
{
    if (hwndCard)
    {
        KillTimer(hwndCard, 1);
        DestroyWindow(hwndCard);
        hwndCard = NULL;
    }

    // Obter informações do monitor
    DISPLAY_DEVICE displayDevice = {sizeof(DISPLAY_DEVICE)};
    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEX);

    // Encontrar o monitor selecionado
    int monitorIndex = 0;
    bool monitorFound = false;
    for (int i = 0; EnumDisplayDevices(NULL, i, &displayDevice, 0); i++)
    {
        if (monitorIndex == config.selectedMonitor)
        {
            HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
            if (hMonitor && GetMonitorInfo(hMonitor, &monitorInfo))
            {
                monitorFound = true;
                break;
            }
        }
        if (displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
        {
            monitorIndex++;
        }
    }

    if (!monitorFound)
    {
        // Usar monitor primário como fallback
        HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        GetMonitorInfo(hMonitor, &monitorInfo);
    }

    // Criar a janela do card
    hwndCard = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        CARD_CLASS_NAME,
        NULL,
        WS_POPUP,
        monitorInfo.rcMonitor.left + 50,
        monitorInfo.rcMonitor.bottom - 150,
        200, 60,
        NULL, NULL, hInst, NULL);

    if (!hwndCard)
    {
        LogMessage(L"Falha ao criar janela do card");
        return;
    }

    // Configurar texto e estilo
    std::wstring text = processName + L"\nVolume: " + std::to_wstring(volume) + L"%";
    SetWindowText(hwndCard, text.c_str());
    SetLayeredWindowAttributes(hwndCard, 0, 220, LWA_ALPHA);

    // Mostrar e atualizar
    ShowWindow(hwndCard, SW_SHOWNOACTIVATE);
    UpdateWindow(hwndCard);

    // Configurar timer para fechar
    SetTimer(hwndCard, 1, 3000, NULL);
}

void CheckAudioSessions()
{
    LogMessage(L"Iniciando verificação de sessões de áudio");

    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    IAudioSessionManager2 *pSessionManager = nullptr;
    IAudioSessionEnumerator *pSessionEnumerator = nullptr;
    IAudioSessionControl *pDefaultSession = nullptr;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void **)&pEnumerator);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao criar MMDeviceEnumerator: " + std::to_wstring(hr));
        return;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao obter endpoint de áudio padrão: " + std::to_wstring(hr));
        pEnumerator->Release();
        return;
    }

    hr = pDevice->Activate(
        __uuidof(IAudioSessionManager2), CLSCTX_ALL,
        NULL, (void **)&pSessionManager);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao ativar IAudioSessionManager2: " + std::to_wstring(hr));
        pDevice->Release();
        pEnumerator->Release();
        return;
    }

    hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao obter enumerador de sessões: " + std::to_wstring(hr));
        pSessionManager->Release();
        pDevice->Release();
        pEnumerator->Release();
        return;
    }

    // Obter sessão padrão
    hr = pSessionManager->GetAudioSessionControl(NULL, 0, &pDefaultSession);
    if (FAILED(hr))
    {
        pDefaultSession = nullptr; // Não é crítico, continuamos sem ela
    }

    int sessionCount = 0;
    hr = pSessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao obter contagem de sessões: " + std::to_wstring(hr));
        sessionCount = 0;
    }

    // Processar sessões
    for (int i = 0; i < sessionCount; i++)
    {
        IAudioSessionControl *pSessionControl = nullptr;
        IAudioSessionControl2 *pSessionControl2 = nullptr;
        ISimpleAudioVolume *pAudioVolume = nullptr;

        hr = pSessionEnumerator->GetSession(i, &pSessionControl);
        if (SUCCEEDED(hr))
        {
            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&pSessionControl2);
        }

        if (SUCCEEDED(hr))
        {
            ProcessAudioSession(pSessionControl2, pSessionControl);
        }

        // Liberar recursos
        if (pAudioVolume)
            pAudioVolume->Release();
        if (pSessionControl2)
            pSessionControl2->Release();
        if (pSessionControl)
            pSessionControl->Release();
    }

    // Processar sessão padrão se existir
    if (pDefaultSession)
    {
        IAudioSessionControl2 *pDefaultSession2 = nullptr;
        if (SUCCEEDED(pDefaultSession->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&pDefaultSession2)))
        {
            ProcessAudioSession(pDefaultSession2, pDefaultSession);
            pDefaultSession2->Release();
        }
        pDefaultSession->Release();
    }

    // Limpar sessões que não existem mais
    CleanupStaleSessions(pSessionEnumerator, sessionCount);

    // Liberar todos os recursos COM
    if (pSessionEnumerator)
        pSessionEnumerator->Release();
    if (pSessionManager)
        pSessionManager->Release();
    if (pDevice)
        pDevice->Release();
    if (pEnumerator)
        pEnumerator->Release();

    LogMessage(L"Verificação de sessões de áudio concluída");
}

void ProcessAudioSession(IAudioSessionControl2 *pSessionControl2, IAudioSessionControl *pSessionControl)
{
    HRESULT hr;
    DWORD pid;
    ISimpleAudioVolume *pAudioVolume = nullptr;

    hr = pSessionControl2->GetProcessId(&pid);
    if (FAILED(hr))
        return;

    hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void **)&pAudioVolume);
    if (FAILED(hr))
        return;

    float volume;
    hr = pAudioVolume->GetMasterVolume(&volume);
    if (FAILED(hr))
    {
        pAudioVolume->Release();
        return;
    }

    BOOL isSystemSounds = pSessionControl2->IsSystemSoundsSession() == S_OK;
    std::wstring sessionName = GetSessionName(pSessionControl2, pid, isSystemSounds);

    if (!sessionName.empty())
    {
        int newVolume = (int)(volume * 100);
        UpdateSessionInfo(pid, sessionName, volume, isSystemSounds, newVolume);
    }

    pAudioVolume->Release();
}

std::wstring GetSessionName(IAudioSessionControl2 *pSessionControl2, DWORD pid, BOOL isSystemSounds)
{
    if (isSystemSounds)
    {
        return L"System Sounds";
    }

    std::wstring sessionName;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess)
    {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH))
        {
            sessionName = exePath;
            size_t pos = sessionName.find_last_of(L"\\");
            if (pos != std::wstring::npos)
            {
                sessionName = sessionName.substr(pos + 1);
            }
        }
        CloseHandle(hProcess);
    }
    return sessionName;
}

void UpdateSessionInfo(DWORD pid, const std::wstring &sessionName, float volume, BOOL isSystemSounds, int newVolume)
{
    auto it = audioSessions.find(pid);
    if (it != audioSessions.end())
    {
        if (abs(it->second.volume - volume) > 0.01f)
        {
            it->second.volume = volume;
            ShowFloatingCard(sessionName, newVolume);
        }
    }
    else
    {
        AudioSessionInfo info = {pid, sessionName, volume, isSystemSounds != FALSE};
        audioSessions[pid] = info;
        ShowFloatingCard(sessionName, newVolume);
    }
}

void CleanupStaleSessions(IAudioSessionEnumerator *pSessionEnumerator, int activeSessionCount)
{
    std::vector<DWORD> activePids;

    // Coletar todos os PIDs ativos
    for (int i = 0; i < activeSessionCount; i++)
    {
        IAudioSessionControl *pTempSession = nullptr;
        if (SUCCEEDED(pSessionEnumerator->GetSession(i, &pTempSession)))
        {
            IAudioSessionControl2 *pTempSession2 = nullptr;
            if (SUCCEEDED(pTempSession->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&pTempSession2)))
            {
                DWORD tempPid;
                if (SUCCEEDED(pTempSession2->GetProcessId(&tempPid)))
                {
                    activePids.push_back(tempPid);
                }
                pTempSession2->Release();
            }
            pTempSession->Release();
        }
    }

    // Remover sessões não encontradas
    for (auto it = audioSessions.begin(); it != audioSessions.end();)
    {
        if (std::find(activePids.begin(), activePids.end(), it->first) == activePids.end())
        {
            it = audioSessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        LogMessage(L"WM_CREATE recebido");
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
        if (wParam == TIMER_ID)
        {
            LogMessage(L"WM_TIMER recebido - verificando sessões de áudio");
            CheckAudioSessions();
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
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
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                config.selectedMonitor = (int)SendMessage(hComboMonitor, CB_GETCURSEL, 0, 0);
                SaveSettings();
            }
            break;
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    OpenLogFile();
    LogMessage(L"Program started");

    // Inicializa COM com tratamento de erro melhorado
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        LogMessage(L"Falha ao inicializar COM");
        MessageBox(NULL, L"Falha ao inicializar COM", L"Erro", MB_ICONERROR);
        CloseLogFile();
        return 1;
    }
    LogMessage(L"COM inicializado com sucesso");

    hInst = hInstance;
    LogMessage(L"Carregando configurações...");
    LoadSettings();
    LogMessage(L"Configurações carregadas");

    // Registra a classe da janela principal
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClass(&wc))
    {
        LogMessage(L"Falha ao registrar classe da janela principal");
        MessageBox(NULL, L"Falha ao registrar classe da janela", L"Erro", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }
    LogMessage(L"Classe da janela principal registrada");

    // Registra a classe do card flutuante
    WNDCLASS cardWc = {};
    cardWc.lpfnWndProc = CardWndProc;
    cardWc.hInstance = hInstance;
    cardWc.lpszClassName = CARD_CLASS_NAME;
    cardWc.hbrBackground = CreateSolidBrush(RGB(40, 40, 40));
    if (!RegisterClass(&cardWc))
    {
        LogMessage(L"Falha ao registrar classe do card flutuante");
        MessageBox(NULL, L"Falha ao registrar classe do card", L"Erro", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }
    LogMessage(L"Classe do card flutuante registrada");

    LogMessage(L"Criando janela principal...");
    hwndMain = CreateWindowEx(
        0, CLASS_NAME, L"Volume Card Mixer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
        NULL, NULL, hInstance, NULL);

    if (!hwndMain)
    {
        LogMessage(L"Falha ao criar janela principal");
        MessageBox(NULL, L"Falha ao criar janela principal", L"Erro", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }
    LogMessage(L"Janela principal criada com sucesso");

    LogMessage(L"Mostrando janela principal...");
    ShowWindow(hwndMain, config.startWindowed ? SW_HIDE : SW_SHOW);
    LogMessage(L"Janela principal mostrada");

    MSG msg = {};
    LogMessage(L"Entrando no loop de mensagens...");
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    LogMessage(L"Saindo do loop de mensagens");

    CoUninitialize();
    LogMessage(L"COM desinicializado");
    CloseLogFile();
    return 0;
}