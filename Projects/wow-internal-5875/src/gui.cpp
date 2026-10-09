#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include "Control/RuntimeControl.h"
#include "Debug/Logger.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t WindowClassName[] = L"WowInternalHonorStyle14H2";
    constexpr wchar_t WindowTitle[] = L"WoW Internal 5875";
    constexpr wchar_t GuiPhaseMarker[] = L"GUI 14H.5 XP RATE + LEVEL ETA";

    enum ControlId
    {
        IdModeCombo = 1001,
        IdLoadProfile,
        IdSettingsTools,
        IdStartWow,
        IdStartBot,
        IdStopBot,
        IdMainTabs,
        IdLogTabs,
        IdRefresh,
        IdOpenLog,
        IdCloseWow,
        IdClearLogOnStart,
        IdAutoRefresh,
        IdVendorAutomation,
        IdWowPath,
        IdLoaderPath,
        IdDllPath,
        IdLogPath,
        IdBrowseWow,
        IdBrowseLoader,
        IdBrowseDll,
        IdBrowseLog,
        IdLogView,
        IdStatusBar,
        IdProfileValue,
        IdRuntimeValue,
        IdTelemetryValue,
        IdPlayerValue,
        IdXpValue,
        IdCombatValue,
        IdGrindValue,
        IdSafetyValue
    };

    HINSTANCE g_instance = nullptr;
    HWND g_mainWindow = nullptr;
    HFONT g_font = nullptr;
    HFONT g_headerFont = nullptr;

    HWND g_modeCombo = nullptr;
    HWND g_mainTabs = nullptr;
    HWND g_logTabs = nullptr;
    HWND g_logView = nullptr;
    HWND g_statusBar = nullptr;

    HWND g_wowPath = nullptr;
    HWND g_loaderPath = nullptr;
    HWND g_dllPath = nullptr;
    HWND g_logPath = nullptr;
    HWND g_clearLogOnStart = nullptr;
    HWND g_autoRefresh = nullptr;
    HWND g_vendorAutomation = nullptr;

    HWND g_profileValue = nullptr;
    HWND g_runtimeValue = nullptr;
    HWND g_telemetryValue = nullptr;
    HWND g_playerValue = nullptr;
    HWND g_xpValue = nullptr;
    HWND g_combatValue = nullptr;
    HWND g_grindValue = nullptr;
    HWND g_safetyValue = nullptr;

    std::vector<HWND> g_botConfigControls;
    std::vector<HWND> g_classConfigControls;
    std::vector<HWND> g_developerControls;

    HANDLE g_loaderProcess = nullptr;
    HANDLE g_wowProcess = nullptr;
    DWORD g_wowProcessId = 0;
    bool g_haveLoaderExit = false;
    DWORD g_lastLoaderExit = 0;

    Control::RuntimeControlChannel g_runtimeControl;
    std::wstring g_lastLogText;

    ULONGLONG g_botSessionStartTick = 0;
    ULONGLONG g_botSessionElapsedMs = 0;
    bool g_botSessionTimerRunning = false;
    bool g_previousRuntimeAttached = false;
    bool g_haveObservedRuntimeHeartbeat = false;
    bool g_runtimeHeartbeatStalled = false;
    LONG g_lastObservedRuntimeHeartbeat = 0;
    ULONGLONG g_lastRuntimeHeartbeatAdvanceMs = 0;
    ULONGLONG g_lastHeartbeatDiagnosticMs = 0;

    void Remember(std::vector<HWND>& group, HWND control)
    {
        if (control)
            group.push_back(control);
    }

    std::wstring ModulePath()
    {
        std::vector<wchar_t> buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return L".";
        return std::wstring(buffer.data(), length);
    }

    std::wstring DirectoryName(const std::wstring& path)
    {
        const std::size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return L".";
        return path.substr(0, slash);
    }

    std::wstring JoinPath(const std::wstring& directory, const std::wstring& name)
    {
        if (directory.empty())
            return name;
        const wchar_t last = directory.back();
        if (last == L'\\' || last == L'/')
            return directory + name;
        return directory + L"\\" + name;
    }

    std::wstring IniPath()
    {
        return JoinPath(DirectoryName(ModulePath()), L"wow_gui.ini");
    }

    bool FileExists(const std::wstring& path)
    {
        if (path.empty())
            return false;
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring EnvironmentValue(const wchar_t* name)
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0)
            return {};

        std::vector<wchar_t> buffer(required + 1, L'\0');
        const DWORD written = GetEnvironmentVariableW(name, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0 || written >= buffer.size())
            return {};
        return std::wstring(buffer.data(), written);
    }

    std::wstring ReadIniValue(const wchar_t* section, const wchar_t* key, const std::wstring& fallback)
    {
        std::vector<wchar_t> buffer(32768, L'\0');
        GetPrivateProfileStringW(
            section,
            key,
            fallback.c_str(),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            IniPath().c_str());
        return std::wstring(buffer.data());
    }

    void WriteIniValue(const wchar_t* section, const wchar_t* key, const std::wstring& value)
    {
        WritePrivateProfileStringW(section, key, value.c_str(), IniPath().c_str());
    }

    std::wstring GetWindowTextString(HWND window)
    {
        const int length = GetWindowTextLengthW(window);
        if (length <= 0)
            return {};
        std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(window, buffer.data(), static_cast<int>(buffer.size()));
        return std::wstring(buffer.data());
    }

    void AppendDisconnectDiagnostic(const std::string& details)
    {
        const std::wstring path = GetWindowTextString(g_logPath);
        if (path.empty())
            return;

        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        const std::string timestamp = std::to_string(utc.wYear) + "-" +
            std::to_string(utc.wMonth) + "-" + std::to_string(utc.wDay) +
            "T" + std::to_string(utc.wHour) + ":" +
            std::to_string(utc.wMinute) + ":" +
            std::to_string(utc.wSecond) + "Z";

        HANDLE file = CreateFileW(
            path.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;

        const std::string line = "[INFO] DISCONNECT DIAGNOSTIC 14D GUI: utc=" +
            timestamp + " " + details + "\r\n";
        DWORD written = 0;
        WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(file);
    }

    void SetControlText(HWND control, const std::wstring& value)
    {
        if (control)
            SetWindowTextW(control, value.c_str());
    }

    void ApplyFont(HWND control, bool header = false)
    {
        if (!control)
            return;
        HFONT font = header && g_headerFont ? g_headerFont : g_font;
        if (font)
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height, bool header = false)
    {
        HWND control = CreateWindowExW(
            0,
            L"STATIC",
            text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, width, height,
            parent, nullptr, g_instance, nullptr);
        ApplyFont(control, header);
        return control;
    }

    HWND CreateEdit(HWND parent, int id, int x, int y, int width, int height, DWORD extraStyle = 0)
    {
        HWND control = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extraStyle,
            x, y, width, height,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            g_instance,
            nullptr);
        ApplyFont(control);
        return control;
    }

    HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height, DWORD style = BS_PUSHBUTTON)
    {
        HWND control = CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
            x, y, width, height,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            g_instance,
            nullptr);
        ApplyFont(control);
        return control;
    }

    HWND CreateGroupBox(HWND parent, const wchar_t* text, int x, int y, int width, int height)
    {
        HWND control = CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            x, y, width, height,
            parent,
            nullptr,
            g_instance,
            nullptr);
        ApplyFont(control);
        return control;
    }

    HWND CreateCombo(HWND parent, int id, int x, int y, int width, int height)
    {
        HWND control = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            x, y, width, height,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            g_instance,
            nullptr);
        ApplyFont(control);
        return control;
    }

    bool BrowseForFile(HWND owner, HWND targetEdit, const wchar_t* title, const wchar_t* filter)
    {
        std::vector<wchar_t> path(32768, L'\0');
        const std::wstring current = GetWindowTextString(targetEdit);
        if (!current.empty() && current.size() + 1 < path.size())
            std::copy(current.begin(), current.end(), path.begin());

        OPENFILENAMEW open{};
        open.lStructSize = sizeof(open);
        open.hwndOwner = owner;
        open.lpstrFile = path.data();
        open.nMaxFile = static_cast<DWORD>(path.size());
        open.lpstrFilter = filter;
        open.nFilterIndex = 1;
        open.lpstrTitle = title;
        open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&open))
            return false;

        SetControlText(targetEdit, path.data());
        return true;
    }

    bool IsProcessRunning(const wchar_t* executableName)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        bool found = false;

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, executableName) == 0)
                {
                    found = true;
                    break;
                }
            }
            while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found;
    }

    struct WowWindowSearch
    {
        HWND window = nullptr;
    };

    BOOL CALLBACK FindWowWindowCallback(HWND window, LPARAM parameter)
    {
        auto* search = reinterpret_cast<WowWindowSearch*>(parameter);
        if (!search || search->window)
            return FALSE;

        wchar_t title[512] = {};
        GetWindowTextW(window, title, static_cast<int>(sizeof(title) / sizeof(title[0])));
        if (std::wcsstr(title, L"World of Warcraft") != nullptr)
        {
            search->window = window;
            return FALSE;
        }
        return TRUE;
    }

    HWND FindWowWindow()
    {
        WowWindowSearch search{};
        EnumWindows(FindWowWindowCallback, reinterpret_cast<LPARAM>(&search));
        return search.window;
    }

    bool IsLogFresh(const std::wstring& path, std::uint64_t maxAgeMilliseconds)
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
            return false;

        FILETIME now{};
        GetSystemTimeAsFileTime(&now);

        ULARGE_INTEGER current{};
        current.LowPart = now.dwLowDateTime;
        current.HighPart = now.dwHighDateTime;

        ULARGE_INTEGER modified{};
        modified.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
        modified.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

        if (current.QuadPart < modified.QuadPart)
            return true;

        return ((current.QuadPart - modified.QuadPart) / 10000ULL) <= maxAgeMilliseconds;
    }

    std::wstring Utf8ToWide(const std::vector<char>& bytes)
    {
        if (bytes.empty())
            return {};

        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;

        if (count <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            count = MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        }

        if (count <= 0)
            return L"Unable to decode log text.";

        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()), result.data(), count);
        return result;
    }

    std::wstring ReadLogTail(const std::wstring& path, DWORD maximumBytes = 131072)
    {
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return L"Log file is not available yet.";

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size))
        {
            CloseHandle(file);
            return L"Unable to read log size.";
        }

        const LONGLONG requested = std::min<LONGLONG>(size.QuadPart, maximumBytes);
        LARGE_INTEGER offset{};
        offset.QuadPart = size.QuadPart - requested;
        SetFilePointerEx(file, offset, nullptr, FILE_BEGIN);

        std::vector<char> bytes(static_cast<std::size_t>(requested));
        DWORD read = 0;
        if (!bytes.empty())
            ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        CloseHandle(file);
        bytes.resize(read);

        if (offset.QuadPart > 0 && !bytes.empty())
        {
            const auto firstNewline = std::find(bytes.begin(), bytes.end(), '\n');
            if (firstNewline != bytes.end())
                bytes.erase(bytes.begin(), firstNewline + 1);
        }

        return Utf8ToWide(bytes);
    }

    std::vector<std::wstring> SplitLines(const std::wstring& text)
    {
        std::vector<std::wstring> lines;
        std::size_t start = 0;
        while (start < text.size())
        {
            const std::size_t end = text.find(L'\n', start);
            std::wstring line = text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
            if (!line.empty() && line.back() == L'\r')
                line.pop_back();
            if (!line.empty())
                lines.push_back(std::move(line));
            if (end == std::wstring::npos)
                break;
            start = end + 1;
        }
        return lines;
    }

    std::wstring LatestLineContaining(const std::vector<std::wstring>& lines, const wchar_t* marker)
    {
        for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        {
            if (it->find(marker) == std::wstring::npos)
                continue;

            std::wstring value = *it;
            constexpr wchar_t prefix[] = L"[INFO] ";
            if (value.rfind(prefix, 0) == 0)
                value.erase(0, (sizeof(prefix) / sizeof(prefix[0])) - 1);
            if (value.size() > 180)
                value.resize(180);
            return value;
        }
        return L"-";
    }

    std::wstring LatestOf(const std::vector<std::wstring>& lines, const std::vector<const wchar_t*>& markers)
    {
        for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        {
            for (const wchar_t* marker : markers)
            {
                if (it->find(marker) == std::wstring::npos)
                    continue;

                std::wstring value = *it;
                constexpr wchar_t prefix[] = L"[INFO] ";
                if (value.rfind(prefix, 0) == 0)
                    value.erase(0, (sizeof(prefix) / sizeof(prefix[0])) - 1);
                if (value.size() > 180)
                    value.resize(180);
                return value;
            }
        }
        return L"-";
    }

    Control::BotMode SelectedMode()
    {
        const LRESULT selection = SendMessageW(g_modeCombo, CB_GETCURSEL, 0, 0);
        return selection == 1 ? Control::BotMode::Questing : Control::BotMode::Grind;
    }

    const wchar_t* ModeDisplayName(Control::BotMode mode)
    {
        switch (mode)
        {
            case Control::BotMode::Grind: return L"Grind Bot";
            case Control::BotMode::Questing: return L"Questing";
            default: return L"Unknown";
        }
    }

    const wchar_t* ProfileDisplayName(Control::BotMode mode)
    {
        switch (mode)
        {
            case Control::BotMode::Grind: return L"Generic Wide-Area Grind / XP + Vendor";
            case Control::BotMode::Questing: return L"QuestDB Durotar / autonomous questing";
            default: return L"No profile loaded";
        }
    }

    void SaveConfiguration()
    {
        WriteIniValue(L"paths", L"wow", GetWindowTextString(g_wowPath));
        WriteIniValue(L"paths", L"loader", GetWindowTextString(g_loaderPath));
        WriteIniValue(L"paths", L"dll", GetWindowTextString(g_dllPath));
        WriteIniValue(L"paths", L"log", GetWindowTextString(g_logPath));
        WriteIniValue(L"bot", L"mode", SelectedMode() == Control::BotMode::Questing ? L"questing" : L"grind");
        WriteIniValue(L"bot", L"vendorAutomationEnabled",
            SendMessageW(g_vendorAutomation, BM_GETCHECK, 0, 0) == BST_CHECKED
                ? L"1" : L"0");
    }

    void LoadConfiguration()
    {
        const std::wstring directory = DirectoryName(ModulePath());

        std::wstring wowFallback = EnvironmentValue(L"WOW_EXE");
        std::wstring loaderFallback = EnvironmentValue(L"WOW_LOADER");
        std::wstring dllFallback = EnvironmentValue(L"INTERNAL_DLL");
        std::wstring logFallback = EnvironmentValue(L"WOW_LOG");

        if (loaderFallback.empty())
            loaderFallback = JoinPath(directory, L"wow_loader.exe");
        if (dllFallback.empty())
            dllFallback = JoinPath(directory, L"wow_internal.dll");
        if (logFallback.empty())
            logFallback = JoinPath(directory, L"wow-internal.log");

        SetControlText(g_wowPath, ReadIniValue(L"paths", L"wow", wowFallback));
        SetControlText(g_loaderPath, ReadIniValue(L"paths", L"loader", loaderFallback));
        SetControlText(g_dllPath, ReadIniValue(L"paths", L"dll", dllFallback));
        SetControlText(g_logPath, ReadIniValue(L"paths", L"log", logFallback));

        const std::wstring mode = ReadIniValue(L"bot", L"mode", L"grind");
        SendMessageW(g_modeCombo, CB_SETCURSEL, mode == L"questing" ? 1 : 0, 0);
        const std::wstring vendorAutomation =
            ReadIniValue(L"bot", L"vendorAutomationEnabled", L"0");
        SendMessageW(g_vendorAutomation, BM_SETCHECK,
            vendorAutomation == L"1" ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    bool TrackedWowProcessAlive()
    {
        if (!g_wowProcess)
            return false;

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(g_wowProcess, &exitCode) || exitCode != STILL_ACTIVE)
        {
            CloseHandle(g_wowProcess);
            g_wowProcess = nullptr;
            g_wowProcessId = 0;
            return false;
        }

        return true;
    }

    bool CreateInheritableWowHandle(HANDLE& inheritedHandle)
    {
        inheritedHandle = nullptr;
        if (!TrackedWowProcessAlive())
            return false;

        return DuplicateHandle(
            GetCurrentProcess(),
            g_wowProcess,
            GetCurrentProcess(),
            &inheritedHandle,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS) != FALSE;
    }

    void PollLoaderProcess()
    {
        if (!g_loaderProcess)
            return;

        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(g_loaderProcess, &exitCode))
            return;

        if (exitCode != STILL_ACTIVE)
        {
            Debug::Logger::Journal("GUI ATTACH targetPid=" +
                std::to_string(g_wowProcessId) +
                " result=loader_exited exitCode=" + std::to_string(exitCode) +
                " runtimeAttached=" + (g_runtimeControl.RuntimeAttached() ? "yes" : "no"));
            g_haveLoaderExit = true;
            g_lastLoaderExit = exitCode;
            CloseHandle(g_loaderProcess);
            g_loaderProcess = nullptr;
        }
    }

    void SetStatusBarText(const std::wstring& text)
    {
        if (g_statusBar)
            SendMessageW(g_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    std::wstring StripLogPrefix(std::wstring line)
    {
        constexpr wchar_t infoPrefix[] = L"[INFO] ";
        if (line.rfind(infoPrefix, 0) == 0)
            line.erase(0, (sizeof(infoPrefix) / sizeof(infoPrefix[0])) - 1);
        return line;
    }

    std::wstring FieldValue(const std::wstring& line, const wchar_t* key)
    {
        const std::wstring marker = std::wstring(key) + L"=";
        const std::size_t start = line.find(marker);
        if (start == std::wstring::npos)
            return {};

        const std::size_t valueStart = start + marker.size();
        const std::size_t end = line.find(L' ', valueStart);
        return line.substr(valueStart, end == std::wstring::npos ? std::wstring::npos : end - valueStart);
    }

    std::wstring FieldValueUntil(
        const std::wstring& line,
        const wchar_t* key,
        const wchar_t* nextKey)
    {
        const std::wstring marker = std::wstring(key) + L"=";
        const std::wstring nextMarker = std::wstring(L" ") + nextKey + L"=";
        const std::size_t start = line.find(marker);
        if (start == std::wstring::npos)
            return {};

        const std::size_t valueStart = start + marker.size();
        const std::size_t end = line.find(nextMarker, valueStart);
        return line.substr(valueStart, end == std::wstring::npos ? std::wstring::npos : end - valueStart);
    }

    std::wstring FriendlyControllerState(const std::wstring& raw)
    {
        if (raw == L"AcquiringTarget") return L"Looking for target";
        if (raw == L"WaitingForTargetSelection") return L"Selecting target";
        if (raw == L"WarriorChargeFacing") return L"Preparing Charge";
        if (raw == L"WarriorOpening") return L"Opening combat";
        if (raw == L"Chasing") return L"Chasing target";
        if (raw == L"Fighting") return L"Fighting";
        if (raw == L"Looting") return L"Looting";
        if (raw == L"PostKillDelay") return L"Finishing kill";
        if (raw == L"Recovering") return L"Recovering";
        if (raw == L"Grinding") return L"Grinding";
        if (raw == L"ApproachingTarget") return L"Moving to target";
        if (raw == L"Vendoring") return L"Vendor trip";
        if (raw == L"Idle") return L"Idle";
        return raw.empty() ? L"-" : raw;
    }

    std::wstring TransitionDestination(const std::wstring& line)
    {
        const std::size_t arrow = line.rfind(L" -> ");
        if (arrow == std::wstring::npos)
            return {};
        return line.substr(arrow + 4);
    }

    std::wstring CompactNumeric(std::wstring value)
    {
        const std::size_t dot = value.find(L'.');
        if (dot != std::wstring::npos)
            value.erase(dot);
        return value;
    }

    std::wstring CompactFraction(const std::wstring& value)
    {
        const std::size_t slash = value.find(L'/');
        if (slash == std::wstring::npos)
            return CompactNumeric(value);

        return CompactNumeric(value.substr(0, slash)) +
               L"/" +
               CompactNumeric(value.substr(slash + 1));
    }


    std::wstring CompactTimeLeft(std::wstring value)
    {
        const std::size_t hour = value.find(L'h');
        if (hour != std::wstring::npos)
        {
            const std::size_t minute = value.find(L'm', hour + 1);
            if (minute != std::wstring::npos)
                return value.substr(0, minute + 1);
        }
        return value;
    }

    std::wstring FormatRuntimeDuration(ULONGLONG elapsedMs)
    {
        const ULONGLONG totalSeconds = elapsedMs / 1000ULL;
        const ULONGLONG hours = totalSeconds / 3600ULL;
        const ULONGLONG minutes = (totalSeconds / 60ULL) % 60ULL;
        const ULONGLONG seconds = totalSeconds % 60ULL;

        wchar_t buffer[64]{};
        swprintf(
            buffer,
            sizeof(buffer) / sizeof(buffer[0]),
            L"%02llu:%02llu:%02llu",
            static_cast<unsigned long long>(hours),
            static_cast<unsigned long long>(minutes),
            static_cast<unsigned long long>(seconds));
        return buffer;
    }

    ULONGLONG CurrentBotRuntimeMs()
    {
        if (!g_botSessionTimerRunning)
            return g_botSessionElapsedMs;
        return g_botSessionElapsedMs + (GetTickCount64() - g_botSessionStartTick);
    }

    void StartBotRuntimeTimer()
    {
        g_botSessionElapsedMs = 0;
        g_botSessionStartTick = GetTickCount64();
        g_botSessionTimerRunning = true;
    }

    void StopBotRuntimeTimer()
    {
        if (!g_botSessionTimerRunning)
            return;
        g_botSessionElapsedMs += GetTickCount64() - g_botSessionStartTick;
        g_botSessionTimerRunning = false;
    }

    std::wstring BuildPlayerSummary(const std::vector<std::wstring>& lines)
    {
        const std::wstring world = LatestLineContaining(lines, L"WorldState:");
        const std::wstring xp = LatestLineContaining(lines, L"XP14G2:");

        const std::wstring level = FieldValue(xp, L"level");
        const std::wstring hp = CompactFraction(FieldValue(world, L"hp"));
        const std::wstring rage = CompactFraction(FieldValue(world, L"rage"));

        if (world == L"-" && xp == L"-")
            return L"Waiting for player data";

        std::wstring result;
        if (!level.empty()) result += L"Level " + level;
        if (!hp.empty()) result += (result.empty() ? L"" : L" | ") + std::wstring(L"HP ") + hp;
        if (!rage.empty()) result += (result.empty() ? L"" : L" | ") + std::wstring(L"Rage ") + rage;
        return result.empty() ? L"Player data available" : result;
    }

    std::wstring BuildXpSummary(const std::vector<std::wstring>& lines)
    {
        const std::wstring xpLine = LatestLineContaining(lines, L"XP14G2:");
        const std::wstring combatLine = LatestLineContaining(lines, L"CombatLoop:");
        if (xpLine == L"-" && combatLine == L"-")
            return L"Waiting for progress data";

        const std::wstring xp = FieldValue(xpLine, L"xp");
        const std::wstring xpPerMinute = FieldValue(xpLine, L"xp/min");
        const std::wstring timeLeft = CompactTimeLeft(
            FieldValueUntil(xpLine, L"timeLeft", L"sessionGain"));
        const std::wstring kills = FieldValue(combatLine, L"kills");

        std::wstring result;
        if (!xp.empty()) result += L"XP " + xp;
        if (!xpPerMinute.empty())
            result += (result.empty() ? L"" : L" | ") + xpPerMinute + L" XP/min";
        if (!timeLeft.empty())
            result += (result.empty() ? L"" : L" | ") + std::wstring(L"Next ") + timeLeft;
        if (!kills.empty())
            result += (result.empty() ? L"" : L" | ") + std::wstring(L"Kills ") + kills;
        return result.empty() ? L"Progress data available" : result;
    }

    std::wstring BuildActivitySummary(const std::vector<std::wstring>& lines)
    {
        const std::wstring combat = LatestLineContaining(lines, L"CombatLoop:");
        const std::wstring grind = LatestLineContaining(lines, L"Grind14G2:");
        const std::wstring death = LatestLineContaining(lines, L"Death14G4.2:");

        const std::wstring deathState = FieldValue(death, L"state");
        const std::wstring combatState = FieldValue(combat, L"state");
        const std::wstring grindState = FieldValue(grind, L"state");

        if (!deathState.empty() && deathState != L"Idle")
        {
            if (deathState == L"RoutingToCorpse") return L"Returning to corpse";
            if (deathState == L"WaitingForGhost") return L"Waiting for ghost";
            if (deathState == L"RetrievingCorpse") return L"Retrieving corpse";
            return L"Death recovery";
        }

        if (!combatState.empty() && combatState != L"AcquiringTarget")
            return FriendlyControllerState(combatState);
        if (!grindState.empty())
            return FriendlyControllerState(grindState);
        if (!combatState.empty())
            return FriendlyControllerState(combatState);
        return L"Waiting";
    }

    void AppendFriendlyLine(std::vector<std::wstring>& output, const std::wstring& message)
    {
        if (message.empty())
            return;
        if (!output.empty() && output.back() == message)
            return;
        output.push_back(message);
        if (output.size() > 60)
            output.erase(output.begin());
    }

    std::wstring FriendlyActivityLine(const std::wstring& original, int tab)
    {
        const std::wstring line = StripLogPrefix(original);

        if (line.find(L"GUI CONTROL 14H.2: startup mode=Grind") != std::wstring::npos)
            return L"Bot attached - Grind Bot";
        if (line.find(L"GUI CONTROL 14H.2: startup mode=Questing") != std::wstring::npos)
            return L"Bot attached - Questing";
        if (line.find(L"GUI CONTROL 14H.2: STOP BOT requested") != std::wstring::npos)
            return L"Stopping bot and unloading from WoW...";
        if (line.find(L"GUI CONTROL 14H.2: RUNTIME DETACHED") != std::wstring::npos)
            return L"Bot stopped - DLL unloaded from WoW";

        if (tab == 0)
        {
            if (line.find(L"GRIND 14G.2: state ") != std::wstring::npos)
                return FriendlyControllerState(TransitionDestination(line));
            if (line.find(L"CombatController state: ") != std::wstring::npos)
                return FriendlyControllerState(TransitionDestination(line));
            if (line.find(L"CHARGE: command issued.") != std::wstring::npos)
                return L"Charge used";
            if (line.find(L"COMBAT LOOP: TARGET DEAD") != std::wstring::npos)
                return L"Target defeated";
            if (line.find(L"Total kills:") != std::wstring::npos)
                return line;
            if (line.find(L"LOOT: PASS") != std::wstring::npos)
                return L"Loot complete";
            if (line.find(L"GRIND 14G.1 VENDOR: START") != std::wstring::npos)
                return L"Vendor trip started";
            if (line.find(L"GRIND 14G.1 VENDOR: FAILED") != std::wstring::npos)
                return L"Vendor trip failed";
            if (line.find(L"HARD STALL") != std::wstring::npos)
                return L"Navigation stuck - recovering";
            if (line.find(L"LOCAL ESCAPE PROBE SUCCESS") != std::wstring::npos)
                return L"Navigation recovered";
            return {};
        }

        if (tab == 1)
        {
            if (line.find(L"CombatController state: ") != std::wstring::npos)
                return L"Combat: " + FriendlyControllerState(TransitionDestination(line));
            if (line.find(L"CHARGE: command issued.") != std::wstring::npos)
                return L"Charge used";
            if (line.find(L"AUTOATTACK") != std::wstring::npos && line.find(L"START") != std::wstring::npos)
                return L"Auto attack started";
            if (line.find(L"RE-ENGAGED") != std::wstring::npos)
                return L"Auto attack resumed after chase";
            if (line.find(L"REND: command issued.") != std::wstring::npos)
                return L"Rend used";
            if (line.find(L"BATTLE SHOUT: command issued.") != std::wstring::npos)
                return L"Battle Shout used";
            if (line.find(L"HEROIC STRIKE: command issued.") != std::wstring::npos)
                return L"Heroic Strike queued";
            if (line.find(L"THUNDER CLAP") != std::wstring::npos && line.find(L"command issued") != std::wstring::npos)
                return L"Thunder Clap used";
            if (line.find(L"COMBAT LOOP: TARGET DEAD") != std::wstring::npos)
                return L"Target defeated";
            if (line.find(L"LOOT: PASS") != std::wstring::npos)
                return L"Loot complete";
            return {};
        }

        if (tab == 2)
        {
            if (line.find(L"NAVMESH 11B: INITIAL PLAN COMPLETE") != std::wstring::npos)
                return L"Navigation route ready";
            if (line.find(L"NAVMESH 11B: REPLAN COMPLETE") != std::wstring::npos)
                return L"Navigation route recalculated";
            if (line.find(L"NAVMESH 11B: OBJECTIVE AREA REACHED") != std::wstring::npos)
                return L"Destination reached";
            if (line.find(L"HARD STALL") != std::wstring::npos)
                return L"Stuck detected - recovering";
            if (line.find(L"LOCAL ESCAPE PROBE SUCCESS") != std::wstring::npos)
                return L"Stuck recovery succeeded";
            if (line.find(L"replan safety limit reached") != std::wstring::npos)
                return L"Route abandoned after repeated stalls";
            return {};
        }

        return line;
    }

    std::wstring FilterLogForSelectedTab(const std::wstring& text)
    {
        if (!g_logTabs)
            return text;

        const int tab = TabCtrl_GetCurSel(g_logTabs);
        if (tab == 3)
            return text;

        const auto lines = SplitLines(text);
        std::vector<std::wstring> friendly;
        for (const auto& line : lines)
            AppendFriendlyLine(friendly, FriendlyActivityLine(line, tab));

        if (friendly.empty())
            return L"No recent activity.";

        std::wstring result;
        for (const auto& line : friendly)
        {
            result += line;
            result += L"\r\n";
        }
        return result;
    }

    void ScrollLogToBottom()
    {
        const LRESULT length = SendMessageW(g_logView, WM_GETTEXTLENGTH, 0, 0);
        SendMessageW(g_logView, EM_SETSEL, length, length);
        SendMessageW(g_logView, EM_SCROLLCARET, 0, 0);
    }

    void ApplyProfileSelection(bool showNotice)
    {
        const Control::BotMode selected = SelectedMode();
        g_runtimeControl.RequestMode(selected);
        SetControlText(g_profileValue, ProfileDisplayName(selected));
        SaveConfiguration();

        if (showNotice && g_runtimeControl.RuntimeAttached() &&
            g_runtimeControl.ActiveMode() != Control::BotMode::Unknown &&
            g_runtimeControl.ActiveMode() != selected)
        {
            MessageBoxW(
                g_mainWindow,
                L"This profile will be used the next time you press Start Bot.\n\nPress Stop Bot first to unload the current bot, choose the new profile, then press Start Bot again. WoW can stay open.",
                WindowTitle,
                MB_ICONINFORMATION);
        }
    }

    void RefreshDashboard(bool refreshLogText)
    {
        PollLoaderProcess();
        TrackedWowProcessAlive();

        const bool wowRunning = IsProcessRunning(L"WoW.exe");
        const bool attached = g_runtimeControl.RuntimeAttached() && wowRunning;
        const bool unloading = attached &&
            (g_runtimeControl.UnloadRequested(false) ||
             g_runtimeControl.RuntimeState() == Control::BotRunState::Unloading);

        const ULONGLONG nowMs = GetTickCount64();
        if (g_previousRuntimeAttached && !wowRunning)
        {
            AppendDisconnectDiagnostic(
                "monotonicMs=" + std::to_string(nowMs) +
                " processAlive=no runtimeAttached=stale"
                " classification=wow_process_not_found_cause_unknown");
        }

        if (attached && !unloading &&
            g_runtimeControl.RuntimeState() == Control::BotRunState::Running)
        {
            const LONG heartbeat = g_runtimeControl.Heartbeat();
            if (!g_haveObservedRuntimeHeartbeat ||
                heartbeat != g_lastObservedRuntimeHeartbeat ||
                nowMs < g_lastRuntimeHeartbeatAdvanceMs)
            {
                if (g_runtimeHeartbeatStalled)
                {
                    AppendDisconnectDiagnostic(
                        "monotonicMs=" + std::to_string(nowMs) +
                        " processAlive=yes runtimeAttached=yes runtimeHeartbeat=" +
                        std::to_string(heartbeat) +
                        " classification=runtime_heartbeat_resumed");
                }
                g_haveObservedRuntimeHeartbeat = true;
                g_runtimeHeartbeatStalled = false;
                g_lastObservedRuntimeHeartbeat = heartbeat;
                g_lastRuntimeHeartbeatAdvanceMs = nowMs;
            }
            else if (nowMs - g_lastRuntimeHeartbeatAdvanceMs >= 10000 &&
                (!g_runtimeHeartbeatStalled ||
                 nowMs - g_lastHeartbeatDiagnosticMs >= 30000))
            {
                g_runtimeHeartbeatStalled = true;
                g_lastHeartbeatDiagnosticMs = nowMs;
                AppendDisconnectDiagnostic(
                    "monotonicMs=" + std::to_string(nowMs) +
                    " processAlive=yes runtimeAttached=yes runtimeHeartbeat=" +
                    std::to_string(heartbeat) +
                    " runtimeHeartbeatAgeMs=" +
                    std::to_string(nowMs - g_lastRuntimeHeartbeatAdvanceMs) +
                    " classification=runtime_loop_stalled_or_blocked");
            }
        }
        else
        {
            g_haveObservedRuntimeHeartbeat = false;
            g_runtimeHeartbeatStalled = false;
        }

        if (attached && !g_previousRuntimeAttached && !g_botSessionTimerRunning)
        {
            g_botSessionStartTick = GetTickCount64();
            g_botSessionTimerRunning = true;
        }
        else if (!attached && g_previousRuntimeAttached)
        {
            StopBotRuntimeTimer();
        }
        g_previousRuntimeAttached = attached;

        const Control::BotMode displayMode =
            attached && g_runtimeControl.ActiveMode() != Control::BotMode::Unknown
                ? g_runtimeControl.ActiveMode()
                : SelectedMode();

        std::wstring runtime = wowRunning ? L"WoW: Running" : L"WoW: Stopped";
        if (unloading)
            runtime += L" | Bot: Stopping";
        else if (attached)
            runtime += L" | Bot: Running";
        else
            runtime += L" | Bot: Stopped";
        runtime += L" | Mode: ";
        runtime += ModeDisplayName(displayMode);
        SetControlText(g_runtimeValue, runtime);

        const std::wstring logPath = GetWindowTextString(g_logPath);
        g_lastLogText = ReadLogTail(logPath);
        const auto lines = SplitLines(g_lastLogText);

        if (attached)
        {
            SetControlText(g_playerValue, BuildPlayerSummary(lines));
            SetControlText(g_xpValue, BuildXpSummary(lines));
            SetControlText(g_combatValue, BuildActivitySummary(lines));
        }
        else
        {
            SetControlText(g_playerValue, wowRunning ? L"Waiting for bot" : L"Game not running");
            SetControlText(g_xpValue, L"-");
            SetControlText(g_combatValue, unloading ? L"Stopping bot" : L"Bot stopped");
        }

        SetControlText(g_telemetryValue, FormatRuntimeDuration(CurrentBotRuntimeMs()));

        if (refreshLogText)
        {
            SetControlText(g_logView, FilterLogForSelectedTab(g_lastLogText));
            ScrollLogToBottom();
        }

        std::wstring status;
        if (!wowRunning)
            status = L"Ready - press Start WoW";
        else if (unloading)
            status = L"Stopping bot and unloading DLL...";
        else if (attached)
            status = BuildActivitySummary(lines);
        else
            status = L"WoW running - choose profile and press Start Bot";

        if (g_loaderProcess)
            status += L" | Injector working";
        else if (g_haveLoaderExit && g_lastLoaderExit != 0)
            status += L" | Injector error=" + std::to_wstring(g_lastLoaderExit);

        SetStatusBarText(status);
    }

    std::wstring Quote(const std::wstring& value)
    {
        return L"\"" + value + L"\"";
    }

    bool ValidateWowPath(HWND owner)
    {
        if (!FileExists(GetWindowTextString(g_wowPath)))
        {
            MessageBoxW(owner, L"Select a valid WoW.exe first.", WindowTitle, MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool ValidateBotPaths(HWND owner)
    {
        if (!ValidateWowPath(owner))
            return false;
        if (!FileExists(GetWindowTextString(g_loaderPath)))
        {
            MessageBoxW(owner, L"wow_loader.exe was not found.", WindowTitle, MB_ICONERROR);
            return false;
        }
        if (!FileExists(GetWindowTextString(g_dllPath)))
        {
            MessageBoxW(owner, L"wow_internal.dll was not found. Build the project first.", WindowTitle, MB_ICONERROR);
            return false;
        }
        return true;
    }

    void StartWow(HWND owner)
    {
        if (IsProcessRunning(L"WoW.exe"))
        {
            SetStatusBarText(L"WoW is already running");
            RefreshDashboard(true);
            return;
        }

        if (!ValidateWowPath(owner))
            return;

        SaveConfiguration();

        const std::wstring wow = GetWindowTextString(g_wowPath);
        const std::wstring workingDirectory = DirectoryName(wow);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};

        const BOOL created = CreateProcessW(
            wow.c_str(),
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            workingDirectory.c_str(),
            &startup,
            &process);

        if (!created)
        {
            const DWORD error = GetLastError();
            MessageBoxW(owner, (L"Unable to start WoW.exe. Win32 error=" + std::to_wstring(error)).c_str(), WindowTitle, MB_ICONERROR);
            return;
        }

        CloseHandle(process.hThread);

        if (g_wowProcess)
            CloseHandle(g_wowProcess);
        g_wowProcess = process.hProcess;
        g_wowProcessId = process.dwProcessId;

        SetStatusBarText(L"WoW started - injector handle retained; log in, then press Start Bot");
        RefreshDashboard(true);
    }

    void StartBot(HWND owner)
    {
        Debug::Logger::Journal("GUI ATTACH event=start_button targetPid=" +
            std::to_string(g_wowProcessId) +
            " runtimeAttached=" + (g_runtimeControl.RuntimeAttached() ? "yes" : "no"));
        ApplyProfileSelection(false);
        const Control::BotMode selected = SelectedMode();

        if (!IsProcessRunning(L"WoW.exe"))
        {
            MessageBoxW(
                owner,
                L"WoW is not running. Press Start WoW first, then press Start Bot.",
                WindowTitle,
                MB_ICONINFORMATION);
            return;
        }

        if (!ValidateBotPaths(owner))
            return;

        if (!TrackedWowProcessAlive())
        {
            MessageBoxW(
                owner,
                L"This WoW process was not started by the current GUI session.\n\n"
                L"For reliable Wine injection, close WoW once, then use Start WoW. "
                L"The GUI will retain the original process handle and future Stop Bot / Start Bot cycles will not require restarting WoW.",
                WindowTitle,
                MB_ICONINFORMATION);
            SetStatusBarText(L"WoW running without retained injector handle - restart once via Start WoW");
            return;
        }

        if (g_runtimeControl.RuntimeAttached())
        {
            Debug::Logger::Journal("GUI ATTACH result=rejected_already_attached targetPid=" +
                std::to_string(g_wowProcessId));
            if (g_runtimeControl.UnloadRequested(false) ||
                g_runtimeControl.RuntimeState() == Control::BotRunState::Unloading)
            {
                MessageBoxW(owner, L"The bot is still unloading. Wait until the status says 'Bot: Not injected', then press Start Bot again.", WindowTitle, MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(owner, L"The bot is already injected and running. Use Stop Bot before starting it again.", WindowTitle, MB_ICONINFORMATION);
            }
            return;
        }

        if (g_loaderProcess)
        {
            MessageBoxW(owner, L"The injector is already working. Wait a moment and try again.", WindowTitle, MB_ICONWARNING);
            return;
        }

        SaveConfiguration();
        g_runtimeControl.RequestVendorAutomation(
            SendMessageW(g_vendorAutomation, BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_runtimeControl.PrepareForInjection(selected);

        const std::wstring loader = GetWindowTextString(g_loaderPath);
        const std::wstring dll = GetWindowTextString(g_dllPath);
        const std::wstring log = GetWindowTextString(g_logPath);

        if (SendMessageW(g_clearLogOnStart, BM_GETCHECK, 0, 0) == BST_CHECKED && !log.empty())
        {
            const BOOL deleted = DeleteFileW(log.c_str());
            const DWORD error = deleted ? ERROR_SUCCESS : GetLastError();
            Debug::Logger::Journal("GUI LOG CLEAR reason=start_bot targetPid=" +
                std::to_string(g_wowProcessId) +
                " deleted=" + (deleted ? "yes" : "no") +
                " error=" + std::to_string(error));
        }
        else
            Debug::Logger::Journal("GUI LOG CLEAR decision=preserve reason=option_unchecked");

        HANDLE inheritedWowHandle = nullptr;
        if (!CreateInheritableWowHandle(inheritedWowHandle))
        {
            g_runtimeControl.RequestRun(false);
            const DWORD error = GetLastError();
            Debug::Logger::Journal("GUI ATTACH result=handle_failed error=" + std::to_string(error));
            MessageBoxW(
                owner,
                (L"Unable to duplicate the retained WoW process handle. Win32 error=" +
                 std::to_wstring(error)).c_str(),
                WindowTitle,
                MB_ICONERROR);
            return;
        }

        const std::uintptr_t inheritedValue =
            reinterpret_cast<std::uintptr_t>(inheritedWowHandle);

        std::wstring command =
            Quote(loader) +
            L" --inject-handle " +
            std::to_wstring(static_cast<unsigned long long>(inheritedValue)) +
            L" " +
            Quote(dll);

        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        const std::wstring workingDirectory = DirectoryName(loader);

        const BOOL created = CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.c_str(),
            &startup,
            &process);

        CloseHandle(inheritedWowHandle);

        if (!created)
        {
            g_runtimeControl.RequestRun(false);
            const DWORD error = GetLastError();
            Debug::Logger::Journal("GUI ATTACH result=loader_start_failed error=" + std::to_string(error));
            MessageBoxW(owner, (L"Unable to start wow_loader.exe. Win32 error=" + std::to_wstring(error)).c_str(), WindowTitle, MB_ICONERROR);
            return;
        }

        CloseHandle(process.hThread);
        g_loaderProcess = process.hProcess;
        g_haveLoaderExit = false;
        StartBotRuntimeTimer();
        Debug::Logger::Journal("GUI ATTACH result=loader_started targetPid=" +
            std::to_string(g_wowProcessId) + " loaderPid=" +
            std::to_string(process.dwProcessId));
        SetStatusBarText(L"Injecting bot into the running WoW client...");
        RefreshDashboard(true);
    }

    void StopBot()
    {
        Debug::Logger::Journal("GUI CONTROL event=stop_button targetPid=" +
            std::to_string(g_wowProcessId) +
            " runtimeAttached=" + (g_runtimeControl.RuntimeAttached() ? "yes" : "no"));
        if (!g_runtimeControl.RuntimeAttached())
        {
            SetStatusBarText(L"Bot is already stopped - no DLL is injected");
            RefreshDashboard(true);
            return;
        }

        StopBotRuntimeTimer();
        g_runtimeControl.RequestRun(false);
        g_runtimeControl.RequestUnload(true);
        SetStatusBarText(L"Stopping bot and unloading DLL from WoW...");
        RefreshDashboard(true);
    }

    void RequestCloseWow(HWND owner)
    {
        HWND wowWindow = FindWowWindow();
        if (!wowWindow)
        {
            MessageBoxW(owner, L"No World of Warcraft window was found.", WindowTitle, MB_ICONINFORMATION);
            return;
        }

        const int answer = MessageBoxW(owner, L"Request WoW to close normally?", WindowTitle, MB_YESNO | MB_ICONQUESTION);
        if (answer == IDYES)
        {
            Debug::Logger::Journal("GUI CONTROL event=close_wow_confirmed targetPid=" +
                std::to_string(g_wowProcessId));
            if (g_runtimeControl.RuntimeAttached())
            {
                g_runtimeControl.RequestRun(false);
                g_runtimeControl.RequestUnload(true);
            }
            PostMessageW(wowWindow, WM_CLOSE, 0, 0);
        }
    }

    void OpenLog(HWND owner)
    {
        const std::wstring path = GetWindowTextString(g_logPath);
        if (!FileExists(path))
        {
            MessageBoxW(owner, L"The log file does not exist yet.", WindowTitle, MB_ICONINFORMATION);
            return;
        }

        const HINSTANCE result = ShellExecuteW(owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            MessageBoxW(owner, L"Windows/Wine could not open the log with a registered application.", WindowTitle, MB_ICONWARNING);
    }

    void ShowTabGroup(std::vector<HWND>& group, bool show)
    {
        for (HWND control : group)
            ShowWindow(control, show ? SW_SHOW : SW_HIDE);
    }

    void UpdateMainTabVisibility()
    {
        const int selection = TabCtrl_GetCurSel(g_mainTabs);
        ShowTabGroup(g_botConfigControls, selection == 0);
        ShowTabGroup(g_classConfigControls, selection == 1);
        ShowTabGroup(g_developerControls, selection == 2);
    }

    void SelectDeveloperToolsTab()
    {
        TabCtrl_SetCurSel(g_mainTabs, 2);
        UpdateMainTabVisibility();
    }

    void RefreshLogFilter()
    {
        SetControlText(g_logView, FilterLogForSelectedTab(g_lastLogText));
        ScrollLogToBottom();
    }

    void LayoutControls(HWND window)
    {
        RECT client{};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        if (g_statusBar)
        {
            SendMessageW(g_statusBar, WM_SIZE, 0, 0);
            RECT statusRect{};
            GetWindowRect(g_statusBar, &statusRect);
            const int statusHeight = statusRect.bottom - statusRect.top;

            const int logY = 445;
            const int logTabsHeight = std::max(145, height - logY - statusHeight - 8);
            MoveWindow(g_logTabs, 10, logY, std::max(420, width - 20), logTabsHeight, TRUE);
            MoveWindow(g_logView, 20, logY + 28, std::max(400, width - 40), std::max(100, logTabsHeight - 38), TRUE);
        }
    }

    void AddTab(HWND tabs, int index, const wchar_t* text)
    {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(text);
        TabCtrl_InsertItem(tabs, index, &item);
    }

    void CreateBotConfigControls(HWND window)
    {
        Remember(g_botConfigControls, CreateLabel(window, L"WoW Internal 5875", 205, 92, 250, 28, true));
        Remember(g_botConfigControls, CreateLabel(window, L"Autonomous WoW 1.12.1 runtime", 206, 122, 300, 20));

        Remember(g_botConfigControls, CreateGroupBox(window, L"Loaded Profile", 40, 165, 530, 78));
        Remember(g_botConfigControls, CreateLabel(window, L"Profile:", 58, 191, 70, 20));
        g_profileValue = CreateLabel(window, L"-", 128, 191, 420, 20);
        Remember(g_botConfigControls, g_profileValue);
        g_vendorAutomation = CreateButton(window,
            L"Automatic vendor routing (next Start Bot)",
            IdVendorAutomation, 58, 216, 410, 22, BS_AUTOCHECKBOX);
        Remember(g_botConfigControls, g_vendorAutomation);

        Remember(g_botConfigControls, CreateGroupBox(window, L"Runtime", 40, 253, 530, 132));
        Remember(g_botConfigControls, CreateLabel(window, L"Bot:", 58, 278, 72, 20));
        g_runtimeValue = CreateLabel(window, L"-", 130, 278, 415, 20);
        Remember(g_botConfigControls, g_runtimeValue);

        Remember(g_botConfigControls, CreateLabel(window, L"Player:", 58, 303, 72, 20));
        g_playerValue = CreateLabel(window, L"-", 130, 303, 415, 20);
        Remember(g_botConfigControls, g_playerValue);

        Remember(g_botConfigControls, CreateLabel(window, L"Progress:", 58, 328, 72, 20));
        g_xpValue = CreateLabel(window, L"-", 130, 328, 415, 20);
        Remember(g_botConfigControls, g_xpValue);

        Remember(g_botConfigControls, CreateLabel(window, L"Runtime:", 58, 353, 72, 20));
        g_telemetryValue = CreateLabel(window, L"00:00:00", 130, 353, 415, 20);
        Remember(g_botConfigControls, g_telemetryValue);

        Remember(g_botConfigControls, CreateLabel(window, L"Status:", 58, 376, 72, 20));
        g_combatValue = CreateLabel(window, L"-", 130, 376, 415, 20);
        Remember(g_botConfigControls, g_combatValue);
    }

    void CreateClassConfigControls(HWND window)
    {
        Remember(g_classConfigControls, CreateGroupBox(window, L"Class Configuration", 40, 92, 530, 290));
        Remember(g_classConfigControls, CreateLabel(window, L"Class", 60, 122, 90, 20));
        Remember(g_classConfigControls, CreateLabel(window, L"Warrior", 170, 122, 320, 20));
        Remember(g_classConfigControls, CreateLabel(window, L"Rotation", 60, 152, 90, 20));
        Remember(g_classConfigControls, CreateLabel(window, L"Charge -> Rend -> Battle Shout -> Heroic Strike -> Thunder Clap", 170, 152, 365, 38));
        Remember(g_classConfigControls, CreateLabel(window, L"Combat", 60, 202, 90, 20));
        Remember(g_classConfigControls, CreateLabel(window, L"Facing guard, chase re-engage, multi-aggro and deferred loot enabled", 170, 202, 365, 38));
        Remember(g_classConfigControls, CreateLabel(window, L"This tab is the foundation for class-specific settings in the next GUI phase.", 60, 270, 465, 40));
    }

    void CreateDeveloperControls(HWND window)
    {
        Remember(g_developerControls, CreateGroupBox(window, L"Paths", 28, 92, 555, 190));

        Remember(g_developerControls, CreateLabel(window, L"WoW.exe", 45, 120, 70, 20));
        g_wowPath = CreateEdit(window, IdWowPath, 115, 116, 350, 24);
        Remember(g_developerControls, g_wowPath);
        Remember(g_developerControls, CreateButton(window, L"Browse...", IdBrowseWow, 475, 116, 88, 24));

        Remember(g_developerControls, CreateLabel(window, L"Loader", 45, 151, 70, 20));
        g_loaderPath = CreateEdit(window, IdLoaderPath, 115, 147, 350, 24);
        Remember(g_developerControls, g_loaderPath);
        Remember(g_developerControls, CreateButton(window, L"Browse...", IdBrowseLoader, 475, 147, 88, 24));

        Remember(g_developerControls, CreateLabel(window, L"DLL", 45, 182, 70, 20));
        g_dllPath = CreateEdit(window, IdDllPath, 115, 178, 350, 24);
        Remember(g_developerControls, g_dllPath);
        Remember(g_developerControls, CreateButton(window, L"Browse...", IdBrowseDll, 475, 178, 88, 24));

        Remember(g_developerControls, CreateLabel(window, L"Log", 45, 213, 70, 20));
        g_logPath = CreateEdit(window, IdLogPath, 115, 209, 350, 24);
        Remember(g_developerControls, g_logPath);
        Remember(g_developerControls, CreateButton(window, L"Browse...", IdBrowseLog, 475, 209, 88, 24));

        g_clearLogOnStart = CreateButton(window, L"Clear log on start", IdClearLogOnStart, 45, 246, 145, 22, BS_AUTOCHECKBOX);
        Remember(g_developerControls, g_clearLogOnStart);
        SendMessageW(g_clearLogOnStart, BM_SETCHECK, BST_CHECKED, 0);

        g_autoRefresh = CreateButton(window, L"Auto refresh", IdAutoRefresh, 205, 246, 120, 22, BS_AUTOCHECKBOX);
        Remember(g_developerControls, g_autoRefresh);
        SendMessageW(g_autoRefresh, BM_SETCHECK, BST_CHECKED, 0);

        Remember(g_developerControls, CreateButton(window, L"Refresh", IdRefresh, 45, 300, 110, 28));
        Remember(g_developerControls, CreateButton(window, L"Open Log", IdOpenLog, 165, 300, 110, 28));
        Remember(g_developerControls, CreateButton(window, L"Close WoW", IdCloseWow, 285, 300, 110, 28));
        Remember(g_developerControls, CreateLabel(window, L"Stop Bot unloads wow_internal.dll. Change profile while stopped, then Start Bot again. WoW stays open.", 45, 345, 500, 38));
    }

    void CreateControls(HWND window)
    {
        (void)GuiPhaseMarker;
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
            g_font = CreateFontIndirectW(&metrics.lfMessageFont);
        if (!g_font)
            g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        LOGFONTW header{};
        if (g_font && GetObjectW(g_font, sizeof(header), &header) == sizeof(header))
        {
            header.lfHeight = static_cast<LONG>(header.lfHeight * 1.35);
            header.lfWeight = FW_BOLD;
            g_headerFont = CreateFontIndirectW(&header);
        }

        g_modeCombo = CreateCombo(window, IdModeCombo, 10, 14, 250, 200);
        SendMessageW(g_modeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Grind Bot"));
        SendMessageW(g_modeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Questing"));
        SendMessageW(g_modeCombo, CB_SETCURSEL, 0, 0);

        CreateButton(window, L"Load Profile", IdLoadProfile, 270, 14, 140, 28);
        CreateButton(window, L"Settings & Tools", IdSettingsTools, 420, 14, 165, 28);

        g_mainTabs = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
            10, 52, 575, 350,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdMainTabs)),
            g_instance,
            nullptr);
        ApplyFont(g_mainTabs);
        AddTab(g_mainTabs, 0, L"Bot Config");
        AddTab(g_mainTabs, 1, L"Class Config");
        AddTab(g_mainTabs, 2, L"Developer Tools");

        CreateBotConfigControls(window);
        CreateClassConfigControls(window);
        CreateDeveloperControls(window);

        CreateButton(window, L"Start WoW", IdStartWow, 95, 405, 120, 32);
        CreateButton(window, L"Start Bot", IdStartBot, 235, 405, 120, 32);
        CreateButton(window, L"Stop Bot", IdStopBot, 375, 405, 120, 32);

        g_logTabs = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
            10, 445, 575, 170,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdLogTabs)),
            g_instance,
            nullptr);
        ApplyFont(g_logTabs);
        AddTab(g_logTabs, 0, L"Activity");
        AddTab(g_logTabs, 1, L"Combat");
        AddTab(g_logTabs, 2, L"Navigation");
        AddTab(g_logTabs, 3, L"Debug");

        g_logView = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            20, 473, 555, 132,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdLogView)),
            g_instance,
            nullptr);
        ApplyFont(g_logView);
        SendMessageW(g_logView, EM_SETLIMITTEXT, 262144, 0);

        g_statusBar = CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStatusBar)),
            g_instance,
            nullptr);
        ApplyFont(g_statusBar);

        LoadConfiguration();
        g_runtimeControl.CreateOwner(SelectedMode(), false);
        Debug::Logger::Journal("GUI CONTROL event=display_connected runtimeAttached=" +
            std::string(g_runtimeControl.RuntimeAttached() ? "yes" : "no") +
            " heartbeat=" + std::to_string(g_runtimeControl.Heartbeat()) +
            " action=preserve_existing_runtime");
        g_runtimeControl.RequestVendorAutomation(
            SendMessageW(g_vendorAutomation, BM_GETCHECK, 0, 0) == BST_CHECKED);
        ApplyProfileSelection(false);
        UpdateMainTabVisibility();
        LayoutControls(window);
        RefreshDashboard(true);
    }

    LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
            case WM_CREATE:
                CreateControls(window);
                SetTimer(window, 1, 1000, nullptr);
                return 0;

            case WM_SIZE:
                LayoutControls(window);
                return 0;

            case WM_TIMER:
                if (wParam == 1)
                {
                    const bool automatic = !g_autoRefresh || SendMessageW(g_autoRefresh, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    if (automatic)
                        RefreshDashboard(true);
                    else
                        PollLoaderProcess();
                }
                return 0;

            case WM_NOTIFY:
            {
                auto* header = reinterpret_cast<NMHDR*>(lParam);
                if (!header)
                    break;

                if (header->idFrom == IdMainTabs && header->code == TCN_SELCHANGE)
                {
                    UpdateMainTabVisibility();
                    return 0;
                }
                if (header->idFrom == IdLogTabs && header->code == TCN_SELCHANGE)
                {
                    RefreshLogFilter();
                    return 0;
                }
                break;
            }

            case WM_COMMAND:
            {
                const int id = LOWORD(wParam);
                const int notification = HIWORD(wParam);

                if (id == IdModeCombo && notification == CBN_SELCHANGE)
                {
                    ApplyProfileSelection(false);
                    return 0;
                }

                switch (id)
                {
                    case IdLoadProfile:
                        ApplyProfileSelection(true);
                        SetStatusBarText(L"Profile loaded: " + std::wstring(ProfileDisplayName(SelectedMode())));
                        return 0;

                    case IdSettingsTools:
                        SelectDeveloperToolsTab();
                        return 0;

                    case IdStartWow:
                        StartWow(window);
                        return 0;

                    case IdStartBot:
                        StartBot(window);
                        return 0;

                    case IdVendorAutomation:
                        if (notification == BN_CLICKED)
                        {
                            g_runtimeControl.RequestVendorAutomation(
                                SendMessageW(g_vendorAutomation, BM_GETCHECK, 0, 0) == BST_CHECKED);
                            SaveConfiguration();
                        }
                        return 0;

                    case IdStopBot:
                        StopBot();
                        return 0;

                    case IdBrowseWow:
                        if (BrowseForFile(window, g_wowPath, L"Select WoW.exe", L"Executables (*.exe)\0*.exe\0All files\0*.*\0")) SaveConfiguration();
                        return 0;

                    case IdBrowseLoader:
                        if (BrowseForFile(window, g_loaderPath, L"Select wow_loader.exe", L"Executables (*.exe)\0*.exe\0All files\0*.*\0")) SaveConfiguration();
                        return 0;

                    case IdBrowseDll:
                        if (BrowseForFile(window, g_dllPath, L"Select wow_internal.dll", L"DLL files (*.dll)\0*.dll\0All files\0*.*\0")) SaveConfiguration();
                        return 0;

                    case IdBrowseLog:
                        if (BrowseForFile(window, g_logPath, L"Select wow-internal.log", L"Log files (*.log)\0*.log\0All files\0*.*\0")) SaveConfiguration();
                        return 0;

                    case IdRefresh:
                        SaveConfiguration();
                        RefreshDashboard(true);
                        return 0;

                    case IdOpenLog:
                        OpenLog(window);
                        return 0;

                    case IdCloseWow:
                        RequestCloseWow(window);
                        return 0;

                    default:
                        break;
                }
                break;
            }

            case WM_CLOSE:
                Debug::Logger::Journal("GUI SESSION STOP reason=window_close runtimeUnloadRequested=no");
                SaveConfiguration();
                DestroyWindow(window);
                return 0;

            case WM_DESTROY:
                KillTimer(window, 1);
                if (g_loaderProcess)
                {
                    CloseHandle(g_loaderProcess);
                    g_loaderProcess = nullptr;
                }
                if (g_wowProcess)
                {
                    CloseHandle(g_wowProcess);
                    g_wowProcess = nullptr;
                    g_wowProcessId = 0;
                }
                if (g_headerFont)
                {
                    DeleteObject(g_headerFont);
                    g_headerFont = nullptr;
                }
                if (g_font && g_font != GetStockObject(DEFAULT_GUI_FONT))
                {
                    DeleteObject(g_font);
                    g_font = nullptr;
                }
                PostQuitMessage(0);
                return 0;

            default:
                break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    g_instance = instance;
    Debug::Logger::SetModule(instance);
    Debug::Logger::Journal("GUI SESSION START");

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&commonControls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hIconSm = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = WindowClassName;

    if (!RegisterClassExW(&windowClass))
        return 1;

    g_mainWindow = CreateWindowExW(
        0,
        WindowClassName,
        WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        620,
        700,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_mainWindow)
        return 1;

    ShowWindow(g_mainWindow, showCommand);
    UpdateWindow(g_mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
