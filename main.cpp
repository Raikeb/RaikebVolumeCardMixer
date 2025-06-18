// RaikebVolumeCardMixer - v12 Ajuste de Handles para evitar vazamento de memória.
// Autor: Raike
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

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
#include <set>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <combaseapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Constantes
constexpr UINT ID_GITHUB_BUTTON = 100;
constexpr UINT ID_LINKEDIN_BUTTON = 101;
constexpr UINT ID_X_BUTTON = 102;
constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT ID_TRAY_EXIT = 1001;
constexpr UINT ID_TRAY_OPEN = 1002;
constexpr UINT ID_CHECK_STARTUP = 2001;
constexpr UINT ID_CHECK_WINDOWED = 2002;
constexpr UINT ID_COMBO_MONITOR = 2003;
constexpr UINT TIMER_ID = 3001;
constexpr UINT VOLUME_CHECK_INTERVAL = 500;
constexpr size_t MAX_LOG_SIZE = 30 * 1024;
const wchar_t CLASS_NAME[] = L"VolumeCardMixerWindowClass";
const wchar_t CARD_CLASS_NAME[] = L"VolumeCardMixerCardClass";
constexpr UINT ID_COMBO_POSITION = 2004;
const std::vector<std::wstring> positionOptions = {
    L"Bottom right (default)",
    L"Top right",
    L"Bottom left",
    L"Top left"};

// Function declarations
void CheckAudioSessionsForDevice(IMMDevice *pDevice);
void ProcessAudioSession(IAudioSessionControl2 *pSessionControl2,
                         IAudioSessionControl *pSessionControl,
                         const std::wstring &deviceId);
std::wstring GetSessionName(IAudioSessionControl2 *pSessionControl2, DWORD pid, bool isSystemSounds);

// Estruturas
struct AudioSessionInfo
{
    DWORD pid;
    std::wstring name;
    float volume;
    bool isSystemSounds;
    HICON icon;
    bool hasChanged;
    std::wstring deviceId;
};

struct AppConfig
{
    bool launchOnStartup = false;
    bool startWindowed = false;
    int selectedMonitor = 0;
    int cardPosition = 0;
};

struct AutoHandle
{
    HANDLE handle;
    AutoHandle(HANDLE h) : handle(h) {}
    ~AutoHandle()
    {
        if (handle)
            CloseHandle(handle);
    }
    operator HANDLE() const { return handle; }
};

struct AutoIcon
{
    HICON icon;
    AutoIcon(HICON h) : icon(h) {}
    ~AutoIcon()
    {
        if (icon)
            DestroyIcon(icon);
    }
    operator HICON() const { return icon; }
};

struct CoTaskMemDeleter
{
    void operator()(void *p) const
    {
        if (p)
            CoTaskMemFree(p);
    }
};

template <typename T>
using CoTaskMemPtr = std::unique_ptr<T, CoTaskMemDeleter>;

// Variáveis Globais
HINSTANCE hInst;
NOTIFYICONDATA nid = {};
HWND hwndMain, hwndCard = nullptr;
HWND hCheckStartup, hCheckWindowed, hComboMonitor;
std::vector<std::wstring> monitorNames;
AppConfig config;
std::map<std::wstring, AudioSessionInfo> audioSessions;
UINT_PTR timerId = 0;
std::wofstream logFile;
bool firstCheck = true;

void OpenLogFile()
{
    logFile.open("VolumeCardMixer.log", std::ios::out | std::ios::app);
    if (logFile.is_open())
    {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &now_time);
        logFile << L"\n\n=== Log started at " << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S") << L" ===\n";
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

        if (logFile.tellp() > MAX_LOG_SIZE)
        {
            logFile.close();
            _wrename(L"VolumeCardMixer.log", L"VolumeCardMixer.old");
            logFile.open("VolumeCardMixer.log", std::ios::out);
        }
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
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD val, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"LaunchOnStartup", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.launchOnStartup = (val != 0);
        if (RegQueryValueExW(hKey, L"StartWindowed", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.startWindowed = (val != 0);
        if (RegQueryValueExW(hKey, L"SelectedMonitor", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.selectedMonitor = val;
        if (RegQueryValueExW(hKey, L"CardPosition", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            config.cardPosition = val;
        RegCloseKey(hKey);
    }
}

void SaveSettings()
{
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\VolumeCardMixer", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS)
    {
        DWORD val = config.launchOnStartup ? 1 : 0;
        RegSetValueExW(hKey, L"LaunchOnStartup", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = config.startWindowed ? 1 : 0;
        RegSetValueExW(hKey, L"StartWindowed", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = (DWORD)config.selectedMonitor;
        RegSetValueExW(hKey, L"SelectedMonitor", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = (DWORD)config.cardPosition;
        RegSetValueExW(hKey, L"CardPosition", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

BOOL AddToStartup(bool enable)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    HKEY hKey;
    if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS)
    {
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
    return FALSE;
}

void PopulateMonitorList(HWND combo)
{
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL
                        {
        auto& monitors = *reinterpret_cast<std::vector<HMONITOR>*>(lParam);
        monitors.push_back(hMonitor);
        return TRUE; }, reinterpret_cast<LPARAM>(&monitors));

    int monitorIndex = 0;
    for (auto hMonitor : monitors)
    {
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfo(hMonitor, &monitorInfo))
        {
            std::wstring monitorName = L"Monitor " + std::to_wstring(monitorIndex + 1);
            if (wcslen(monitorInfo.szDevice) > 0)
            {
                DISPLAY_DEVICE displayDevice = {sizeof(DISPLAY_DEVICE)};
                displayDevice.cb = sizeof(DISPLAY_DEVICE);
                if (EnumDisplayDevices(monitorInfo.szDevice, 0, &displayDevice, 0))
                {
                    monitorName += L" (" + std::wstring(displayDevice.DeviceString) + L")";
                }
            }
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)monitorName.c_str());
            monitorIndex++;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, config.selectedMonitor, 0);
}

HICON GetProcessIcon(DWORD pid)
{
    AutoHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!hProcess)
        return nullptr;

    wchar_t exePath[MAX_PATH] = {0};
    if (!GetModuleFileNameExW(hProcess, nullptr, exePath, MAX_PATH))
    {
        return nullptr; // AutoHandle garante que hProcess será fechado
    }

    SHFILEINFOW sfi = {0};
    if (SHGetFileInfoW(exePath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON))
    {
        return sfi.hIcon; // Retorna o ícone (quem chamou deve destruí-lo)
    }
    return nullptr;
}

void ShowFloatingCard(const std::wstring &processName, int volume, HICON hIcon = nullptr)
{
    static std::wstring lastProcessName;
    static int lastVolume = -1;
    static auto lastShowTime = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShowTime).count();

    if (processName == lastProcessName && volume == lastVolume && elapsed < 1000)
    {
        if (hIcon)
            DestroyIcon(hIcon);
        return;
    }

    if (hwndCard)
    {
        DestroyWindow(hwndCard);
        hwndCard = nullptr;
    }

    // Obter todos os monitores
    DISPLAY_DEVICE displayDevice = {sizeof(DISPLAY_DEVICE)};
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL
                        {
        auto& monitors = *reinterpret_cast<std::vector<HMONITOR>*>(lParam);
        monitors.push_back(hMonitor);
        return TRUE; }, reinterpret_cast<LPARAM>(&monitors));

    // Verificar se temos o monitor selecionado disponível
    HMONITOR hMonitor = NULL;
    if (config.selectedMonitor >= 0 && config.selectedMonitor < (int)monitors.size())
    {
        hMonitor = monitors[config.selectedMonitor];
    }
    else
    {
        hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    }

    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(hMonitor, &monitorInfo);

    int width = 280;
    int height = 60;
    int posX, posY;

    switch (config.cardPosition)
    {
    case 1: // Top right
        posX = monitorInfo.rcMonitor.right - width - 20;
        posY = monitorInfo.rcMonitor.top + 20;
        break;
    case 2: // Bottom left
        posX = monitorInfo.rcMonitor.left + 20;
        posY = monitorInfo.rcMonitor.bottom - height - 55;
        break;
    case 3: // Top left
        posX = monitorInfo.rcMonitor.left + 20;
        posY = monitorInfo.rcMonitor.top + 20;
        break;
    case 0: // Bottom right (default)
    default:
        posX = monitorInfo.rcMonitor.right - width - 20;
        posY = monitorInfo.rcMonitor.bottom - height - 55;
        break;
    }

    hwndCard = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        CARD_CLASS_NAME, nullptr, WS_POPUP,
        posX, posY, width, height, nullptr, nullptr, hInst, nullptr);

    if (hwndCard)
    {
        lastProcessName = processName;
        lastVolume = volume;
        lastShowTime = now;
        std::wstring text = processName + L"  Volume: " + std::to_wstring(volume) + L"%";
        SetWindowTextW(hwndCard, text.c_str());
        SetLayeredWindowAttributes(hwndCard, 0, 220, LWA_ALPHA);

        if (hIcon)
        {
            SendMessageW(hwndCard, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessageW(hwndCard, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        ShowWindow(hwndCard, SW_SHOWNOACTIVATE);
        SetTimer(hwndCard, 1, 1500, nullptr);
    }
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

        // fundo
        HBRUSH bgBrush = CreateSolidBrush(RGB(50, 50, 50));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        HICON hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
        if (hIcon)
        {
            DrawIconEx(hdc, 10, (rect.bottom - 32) / 2, hIcon, 32, 32, 0, NULL, DI_NORMAL);
        }

        HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        wchar_t text[128];
        GetWindowTextW(hwnd, text, 128);

        RECT textRect = rect;
        textRect.left += 50;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_TIMER:
        if (wParam == 1)
        {
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
            hwndCard = NULL;
        }
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
void CheckAllAudioDevices()
{
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDeviceCollection *pCollection = nullptr;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                (void **)&pEnumerator)))
    {
        return;
    }

    if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)))
    {
        UINT count = 0;
        pCollection->GetCount(&count);

        for (UINT i = 0; i < count; i++)
        {
            IMMDevice *pDevice = nullptr;
            if (SUCCEEDED(pCollection->Item(i, &pDevice)))
            {
                CheckAudioSessionsForDevice(pDevice);
                pDevice->Release();
            }
        }
        pCollection->Release();
    }
    pEnumerator->Release();
}

void CheckAudioSessionsForDevice(IMMDevice *pDevice)
{
    LPWSTR pwszDeviceId = nullptr;
    if (FAILED(pDevice->GetId(&pwszDeviceId)))
    {
        return;
    }
    std::wstring deviceId(pwszDeviceId);
    CoTaskMemFree(pwszDeviceId);

    IAudioSessionManager2 *pSessionManager = nullptr;
    if (FAILED(pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                 nullptr, (void **)&pSessionManager)))
    {
        return;
    }

    IAudioSessionEnumerator *pSessionEnumerator = nullptr;
    if (SUCCEEDED(pSessionManager->GetSessionEnumerator(&pSessionEnumerator)))
    {
        int sessionCount = 0;
        if (SUCCEEDED(pSessionEnumerator->GetCount(&sessionCount)))
        {
            for (int i = 0; i < sessionCount; i++)
            {
                IAudioSessionControl *pSessionControl = nullptr;
                if (SUCCEEDED(pSessionEnumerator->GetSession(i, &pSessionControl)))
                {
                    IAudioSessionControl2 *pSessionControl2 = nullptr;
                    if (SUCCEEDED(pSessionControl->QueryInterface(
                            __uuidof(IAudioSessionControl2), (void **)&pSessionControl2)))
                    {
                        // Passa o deviceId para ProcessAudioSession
                        ProcessAudioSession(pSessionControl2, pSessionControl, deviceId);
                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
            }
        }
        pSessionEnumerator->Release();
    }
    pSessionManager->Release();
}
std::wstring GetSessionName(IAudioSessionControl2 *pSessionControl2, DWORD pid, bool isSystemSounds)
{
    LPWSTR pswName = nullptr;
    if (SUCCEEDED(pSessionControl2->GetSessionIdentifier(&pswName)))
    {
        std::wstring name(pswName);
        CoTaskMemFree(pswName);

        if (isSystemSounds)
        {
            return L"System Sounds";
        }

        if (name.find(L"Explorer.EXE") != std::wstring::npos)
        {
            return L"Windows Explorer";
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess)
        {
            wchar_t exeName[MAX_PATH] = {0};
            if (GetProcessImageFileNameW(hProcess, exeName, MAX_PATH))
            {
                std::wstring fullPath(exeName);
                size_t pos = fullPath.find_last_of(L"\\");
                if (pos != std::wstring::npos)
                {
                    std::wstring shortName = fullPath.substr(pos + 1);
                    CloseHandle(hProcess);
                    return shortName;
                }
            }
            CloseHandle(hProcess);
        }
        return name;
    }
    return L"";
}
void ProcessAudioSession(IAudioSessionControl2 *pSessionControl2, IAudioSessionControl *pSessionControl, const std::wstring &deviceId)
{
    DWORD pid;
    if (FAILED(pSessionControl2->GetProcessId(&pid)))
    {
        return;
    }

    ISimpleAudioVolume *pAudioVolume = nullptr;
    if (FAILED(pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void **)&pAudioVolume)))
    {
        return;
    }

    float volume;
    if (SUCCEEDED(pAudioVolume->GetMasterVolume(&volume)))
    {
        BOOL isSystemSounds = pSessionControl2->IsSystemSoundsSession() == S_OK;
        if (isSystemSounds)
        {
            pAudioVolume->Release();
            return;
        }

        std::wstring sessionName = GetSessionName(pSessionControl2, pid, isSystemSounds);
        if (sessionName.empty())
        {
            pAudioVolume->Release();
            return;
        }

        std::wstring sessionKey = std::to_wstring(pid) + L"|" + deviceId;

        auto it = audioSessions.find(sessionKey);
        if (it != audioSessions.end())
        {
            if (fabs(it->second.volume - volume) > 0.01f)
            {
                it->second.volume = volume;
                it->second.hasChanged = true;

                HICON appIcon = GetProcessIcon(pid);
                if (it != audioSessions.end())
                {
                    if (it->second.icon)
                    {
                        DestroyIcon(it->second.icon); // Libera o ícone antigo
                    }
                    it->second.icon = appIcon; // Atribui o novo ícone
                }

                if (!firstCheck)
                {
                    ShowFloatingCard(sessionName, static_cast<int>(round(volume * 100)), appIcon);
                }
            }
            else
            {
                it->second.hasChanged = false;
            }
        }
        else
        {
            HICON appIcon = GetProcessIcon(pid);
            audioSessions[sessionKey] = {pid, sessionName, volume, isSystemSounds, appIcon, true, deviceId};

            if (!firstCheck && volume < 0.99f)
            {
                ShowFloatingCard(sessionName, static_cast<int>(round(volume * 100)), appIcon);
            }
        }
    }
    pAudioVolume->Release();
}

void CheckAudioSessions()
{
    static auto lastCheck = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheck).count() < 100)
    {
        return;
    }
    lastCheck = now;

    if (firstCheck)
    {
        CheckAllAudioDevices();
        firstCheck = false;
        return;
    }

    // Verificação de sessões ativas em todos os dispositivos
    std::set<std::wstring> activeSessions;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator)))
    {
        IMMDeviceCollection *pCollection = nullptr;
        if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)))
        {
            UINT count = 0;
            if (SUCCEEDED(pCollection->GetCount(&count)))
            {
                for (UINT i = 0; i < count; i++)
                {
                    IMMDevice *pDevice = nullptr;
                    if (SUCCEEDED(pCollection->Item(i, &pDevice)))
                    {
                        LPWSTR deviceId = nullptr;
                        if (SUCCEEDED(pDevice->GetId(&deviceId)))
                        {
                            std::wstring currentDeviceId(deviceId);
                            CoTaskMemFree(deviceId);

                            IAudioSessionManager2 *pSessionManager = nullptr;
                            if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void **)&pSessionManager)))
                            {
                                IAudioSessionEnumerator *pSessionEnumerator = nullptr;
                                if (SUCCEEDED(pSessionManager->GetSessionEnumerator(&pSessionEnumerator)))
                                {
                                    int sessionCount = 0;
                                    if (SUCCEEDED(pSessionEnumerator->GetCount(&sessionCount)))
                                    {
                                        for (int j = 0; j < sessionCount; j++)
                                        {
                                            IAudioSessionControl *pSessionControl = nullptr;
                                            if (SUCCEEDED(pSessionEnumerator->GetSession(j, &pSessionControl)))
                                            {
                                                IAudioSessionControl2 *pSessionControl2 = nullptr;
                                                if (SUCCEEDED(pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&pSessionControl2)))
                                                {
                                                    DWORD pid;
                                                    if (SUCCEEDED(pSessionControl2->GetProcessId(&pid)))
                                                    {
                                                        std::wstring sessionKey = std::to_wstring(pid) + L"|" + currentDeviceId;
                                                        activeSessions.insert(sessionKey);
                                                    }
                                                    pSessionControl2->Release();
                                                }
                                                pSessionControl->Release();
                                            }
                                        }
                                    }
                                    pSessionEnumerator->Release();
                                }
                                pSessionManager->Release();
                            }
                        }
                        pDevice->Release();
                    }
                }
            }
            pCollection->Release();
        }
        pEnumerator->Release();
    }

    // Limpa sessões inativas
    std::vector<std::wstring> toRemove;
    for (auto &session : audioSessions)
    {
        if (activeSessions.find(session.first) == activeSessions.end())
        {
            if (session.second.icon)
            {
                DestroyIcon(session.second.icon);
            }
            toRemove.push_back(session.first);
        }
    }

    for (auto &key : toRemove)
    {
        audioSessions.erase(key);
    }

    CheckAllAudioDevices();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // Configura o ícone da janela
        HICON hAppIcon = (HICON)LoadImageW(hInst, L"resources/Icone.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
        if (hAppIcon)
        {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppIcon);
        }

        // Configura o ícone da bandeja
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        HICON hTrayIcon = (HICON)LoadImageW(hInst, L"resources/IconeMini.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (!hTrayIcon)
            hTrayIcon = LoadIconW(NULL, IDI_APPLICATION);
        nid.hIcon = hTrayIcon;
        wcscpy_s(nid.szTip, L"Raikeb Volume Card Mixer");
        Shell_NotifyIconW(NIM_ADD, &nid);

        // Configura o fundo escuro
        SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(32, 32, 32)));

        // Cria os controles com cores escuras
        hCheckStartup = CreateWindowW(L"BUTTON", L"Launch on Startup",
                                      WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                      20, 20, 140, 20, hwnd, (HMENU)ID_CHECK_STARTUP, hInst, NULL);
        SendMessageW(hCheckStartup, BM_SETCHECK, config.launchOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme(hCheckStartup, L"DarkMode_Explorer", NULL);

        hCheckWindowed = CreateWindowW(L"BUTTON", L"Start Windowed",
                                       WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                       20, 50, 125, 20, hwnd, (HMENU)ID_CHECK_WINDOWED, hInst, NULL);
        SendMessageW(hCheckWindowed, BM_SETCHECK, config.startWindowed ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme(hCheckWindowed, L"DarkMode_Explorer", NULL);

        HWND hStaticMonitor = CreateWindowW(L"STATIC", L"Preferred display:",
                                            WS_VISIBLE | WS_CHILD,
                                            20, 80, 120, 20, hwnd, NULL, hInst, NULL);
        SetWindowTheme(hStaticMonitor, L"DarkMode_Explorer", NULL);

        hComboMonitor = CreateWindowW(L"COMBOBOX", NULL,
                                      WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                      20, 98, 200, 200, hwnd, (HMENU)ID_COMBO_MONITOR, hInst, NULL);
        SetWindowTheme(hComboMonitor, L"DarkMode_Explorer", NULL);
        PopulateMonitorList(hComboMonitor);

        HWND hStaticPosition = CreateWindowW(L"STATIC", L"Card popup location:",
                                             WS_VISIBLE | WS_CHILD,
                                             20, 130, 137, 20, hwnd, NULL, hInst, NULL);
        SetWindowTheme(hStaticPosition, L"DarkMode_Explorer", NULL);

        HWND hComboPosition = CreateWindowW(L"COMBOBOX", NULL,
                                            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                            20, 148, 200, 200, hwnd, (HMENU)ID_COMBO_POSITION, hInst, NULL);
        SetWindowTheme(hComboPosition, L"DarkMode_Explorer", NULL);

        for (const auto &option : positionOptions)
        {
            SendMessageW(hComboPosition, CB_ADDSTRING, 0, (LPARAM)option.c_str());
        }
        SendMessageW(hComboPosition, CB_SETCURSEL, config.cardPosition, 0);

        timerId = SetTimer(hwnd, TIMER_ID, 100, NULL);

        HFONT hFontMono = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Consolas");

        HWND hAsciiArt = CreateWindowW(L"STATIC",
                                       L"⠀⠀⠀⠀⠀⠀⠀⢀⣴⠾⠛⢛⣛⣷⣄⣠⣾⣛⡛⠛⠷⣦⡀⠀⠀⠀⠀⠀⠀⠀    \n"
                                       L"⢠⣤⠄⠀⠀⠀⣰⡟⠁⢠⡾⠛⢉⣹⣿⣿⣟⡉⠙⢷⣄⠈⠻⣦⠀⠀⠀⠠⣦⣄  \n"
                                       L"⣿⡀⠀⣀⣤⡾⠋⠀⣠⡟⠁⣰⡟⠁⣹⣯⠈⠻⣆⠀⠻⣆⠀⠙⠷⣦⣀⠀⢀⣿  \n"
                                       L"⠹⣿⡛⠋⠁⠀⢀⣴⠏⠀⢠⡿⣠⡴⠏⠙⢷⣄⢻⡆⠀⠙⣷⡀⠀⠈⠉⠛⣻⠏  \n"
                                       L"⠀⠈⠻⢶⣶⣶⣟⣡⣤⣴⠿⠟⠋⠀⠀⠀⠀⠉⠛⠿⣦⣤⣌⣻⣷⣶⡶⠟⠉⠀   ",
                                       WS_VISIBLE | WS_CHILD,
                                       175, 30, 400, 60, hwnd, NULL, hInst, NULL);

        SendMessageW(hAsciiArt, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        HWND hPoweredBy = CreateWindowW(L"STATIC", L"Follow Raikeb ⬇️",
                                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                                        250, 110, 111, 20, hwnd, NULL, hInst, NULL);
        SetWindowTheme(hPoweredBy, L"DarkMode_Explorer", NULL);

        // Botão GitHub
        HWND hGitHub = CreateWindowW(L"BUTTON", NULL,
                                     WS_VISIBLE | WS_CHILD | BS_ICON | BS_FLAT,
                                     250, 130, 32, 32, hwnd, (HMENU)ID_GITHUB_BUTTON, hInst, NULL);
        HICON hGitHubIcon = (HICON)LoadImageW(hInst, L"resources/IconeGithub.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (!hGitHubIcon)
            hGitHubIcon = LoadIconW(NULL, IDI_APPLICATION);
        SendMessageW(hGitHub, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hGitHubIcon);
        SetWindowTheme(hGitHub, L"DarkMode_Explorer", NULL);

        // Botão X (Twitter)
        HWND hXButton = CreateWindowW(L"BUTTON", NULL,
                                      WS_VISIBLE | WS_CHILD | BS_ICON | BS_FLAT,
                                      290, 130, 32, 32, hwnd, (HMENU)ID_X_BUTTON, hInst, NULL);
        HICON hXIcon = (HICON)LoadImageW(hInst, L"resources/IconeX.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (!hXIcon)
            hXIcon = LoadIconW(NULL, IDI_APPLICATION);
        SendMessageW(hXButton, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hXIcon);
        SetWindowTheme(hXButton, L"DarkMode_Explorer", NULL);

        // Botão LinkedIn
        HWND hLinkedIn = CreateWindowW(L"BUTTON", NULL,
                                       WS_VISIBLE | WS_CHILD | BS_ICON | BS_FLAT,
                                       330, 130, 32, 32, hwnd, (HMENU)ID_LINKEDIN_BUTTON, hInst, NULL);
        HICON hLinkedInIcon = (HICON)LoadImageW(hInst, L"resources/IconeLinkedin.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (!hLinkedInIcon)
            hLinkedInIcon = LoadIconW(NULL, IDI_APPLICATION);
        SendMessageW(hLinkedIn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hLinkedInIcon);
        SetWindowTheme(hLinkedIn, L"DarkMode_Explorer", NULL);

        // Força o redesenho da janela com o tema escuro
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        break;
    }
    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
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
            Shell_NotifyIconW(NIM_DELETE, &nid);
            KillTimer(hwnd, timerId);
            PostQuitMessage(0);
            break;
        case ID_CHECK_STARTUP:
            config.launchOnStartup = SendMessageW(hCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
            AddToStartup(config.launchOnStartup);
            SaveSettings();
            break;
        case ID_CHECK_WINDOWED:
            config.startWindowed = SendMessageW(hCheckWindowed, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SaveSettings();
            break;
        case ID_COMBO_MONITOR:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                config.selectedMonitor = (int)SendMessageW(hComboMonitor, CB_GETCURSEL, 0, 0);
                SaveSettings();
            }
            break;
        case ID_COMBO_POSITION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                config.cardPosition = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
                SaveSettings();
            }
            break;
        case ID_GITHUB_BUTTON:
            ShellExecuteW(NULL, L"open", L"https://github.com/Raikeb/RaikebVolumeCardMixer", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_X_BUTTON:
            ShellExecuteW(NULL, L"open", L"https://x.com/Raikeb", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_LINKEDIN_BUTTON:
            ShellExecuteW(NULL, L"open", L"https://www.linkedin.com/in/raikeb/", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(32, 32, 32));
        return (LRESULT)GetStockObject(BLACK_BRUSH);
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(64, 64, 64));
        return (LRESULT)CreateSolidBrush(RGB(64, 64, 64));
    }
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Abrir");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Sair");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        KillTimer(hwnd, timerId);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    HICON hIcon = (HICON)LoadImageW(hInstance, L"resources/Icone.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (!hIcon)
    {
        hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    OpenLogFile();
    LogMessage(L"Program started");

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        LogMessage(L"Failed to initialize COM");
        MessageBoxW(NULL, L"Failed to initialize COM", L"Error", MB_ICONERROR);
        CloseLogFile();
        return 1;
    }

    hInst = hInstance;
    LoadSettings();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassW(&wc))
    {
        LogMessage(L"Failed to register window class");
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }

    WNDCLASSW cardWc = {};
    cardWc.lpfnWndProc = CardWndProc;
    cardWc.hInstance = hInstance;
    cardWc.lpszClassName = CARD_CLASS_NAME;
    cardWc.hbrBackground = CreateSolidBrush(RGB(40, 40, 40));
    if (!RegisterClassW(&cardWc))
    {
        LogMessage(L"Failed to register card window class");
        MessageBoxW(NULL, L"Failed to register card window class", L"Error", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }

    hwndMain = CreateWindowW(CLASS_NAME, L"Raikeb Volume Card Mixer", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
                             NULL, NULL, hInstance, NULL);

    if (!hwndMain)
    {
        LogMessage(L"Failed to create main window");
        MessageBoxW(NULL, L"Failed to create main window", L"Error", MB_ICONERROR);
        CoUninitialize();
        CloseLogFile();
        return 1;
    }

    ShowWindow(hwndMain, config.startWindowed ? SW_HIDE : SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    CloseLogFile();
    return 0;
}