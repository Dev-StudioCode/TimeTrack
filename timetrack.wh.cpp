// ==WindhawkMod==
// @id              timetrack
// @name            TimeTrack
// @description     Digital wellbeing & daily screen time monitor for Windows
// @version         1.0.0
// @author          Dev-StudioCode
// @github          https://github.com/Dev-StudioCode
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lpsapi -lshlwapi -lshell32 -lole32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# TimeTrack

**TimeTrack** is a lightweight, privacy-focused digital wellbeing and screen
time monitor for Windows. It runs natively inside Windows Explorer via Windhawk
without needing external runtimes, heavy frameworks, or internet access.

### ✨ Features
- ⏱️ **Active Screen Time:** Accurately measures active screen time for each
foreground application.
- 💤 **Smart Inactivity Detection:** Automatically pauses counting when you are
away from your computer (keyboard/mouse idle).
- 📊 **Per-App Breakdown & Focus Count:** Tracks duration and focus switch
counts for every app you use.
- 🔔 **System Tray Integration:** Displays today's screen time and active app
directly in the notification area.
- 📁 **Open & Local JSON Storage:** All data is saved locally on your computer
in human-readable JSON files.
- ⚡ **Zero Overhead:** Native C++ design using negligible RAM (< 5 MB) and 0%
CPU.
- 🔒 **100% Private:** No telemetry, no background network calls, completely
offline.

### ⚙️ Settings
- **Inactivity timeout (seconds):** How many seconds of no mouse/keyboard
activity before screen time pauses (default: 120s).
- **Auto-save interval (seconds):** How often daily statistics are persisted to
disk (default: 30s).
- **Show System Tray Icon:** Toggle whether the TimeTrack icon appears in your
notification area.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- idleThresholdSeconds: 120
  $name: Inactivity timeout (seconds)
  $description: Stop accumulating screen time when no keyboard or mouse activity
is detected for this duration.
- saveIntervalSeconds: 30
  $name: Auto-save interval (seconds)
  $description: Frequency in seconds to persist daily stats to disk.
- showTrayIcon: true
  $name: Show System Tray Icon
  $description: Display the TimeTrack tray icon in the Windows taskbar
notification area.
*/
// ==/WindhawkModSettings==

#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <windhawk_api.h>

// ---------------------------------------------------------------------------
// Constants & IDs
// ---------------------------------------------------------------------------
#define WM_TRAYICON (WM_USER + 101)
#define ID_TIMER_TICK 1001
#define ID_TRAY_TITLE 2001
#define ID_TRAY_TOTAL_TIME 2002
#define ID_TRAY_ACTIVE_APP 2003
#define ID_TRAY_SEPARATOR 2004
#define ID_TRAY_TOP_APP_START 2100
#define ID_TRAY_OPEN_FOLDER 2201
#define ID_TRAY_RESET_TODAY 2202

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------
struct ModSettings {
  int idleThresholdSeconds = 120;
  int saveIntervalSeconds = 30;
  bool showTrayIcon = true;
};

struct AppUsageRecord {
  std::wstring exeName;
  uint32_t activeSeconds = 0;
  uint32_t focusCount = 0;
};

struct DailyStats {
  std::wstring dateString; // YYYY-MM-DD
  uint32_t totalScreenTimeSeconds = 0;
  std::unordered_map<std::wstring, AppUsageRecord> apps;
};

// ---------------------------------------------------------------------------
// Global State
// ---------------------------------------------------------------------------
static ModSettings g_settings;
static std::mutex g_settingsMutex;

static DailyStats g_dailyStats;
static std::mutex g_statsMutex;

static std::wstring g_currentActiveApp = L"";
static bool g_isUserIdle = false;

static WCHAR g_storageDirectory[MAX_PATH] = {0};
static WCHAR g_todayJsonPath[MAX_PATH] = {0};
static WCHAR g_historyDirectory[MAX_PATH] = {0};

static std::atomic<bool> g_stopRequested(false);
static HANDLE g_hWorkerThread = NULL;
static HWND g_hTrayWnd = NULL;
static NOTIFYICONDATAW g_nid = {0};
static bool g_trayIconAdded = false;

// ---------------------------------------------------------------------------
// Helper Functions: Date & Time Formatting
// ---------------------------------------------------------------------------
static std::wstring GetCurrentDateString() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  WCHAR buffer[32];
  StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%04d-%02d-%02d", st.wYear,
                   st.wMonth, st.wDay);
  return std::wstring(buffer);
}

static std::wstring GetCurrentTimestampString() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  WCHAR buffer[64];
  StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%04d-%02d-%02d %02d:%02d:%02d",
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                   st.wSecond);
  return std::wstring(buffer);
}

static std::wstring FormatDuration(uint32_t totalSeconds,
                                   bool includeSeconds = false) {
  uint32_t hours = totalSeconds / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;

  WCHAR buffer[64];
  if (hours > 0) {
    if (includeSeconds) {
      StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%uh %um %us", hours,
                       minutes, seconds);
    } else {
      StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%uh %um", hours, minutes);
    }
  } else if (minutes > 0) {
    if (includeSeconds) {
      StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%um %us", minutes, seconds);
    } else {
      StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%um", minutes);
    }
  } else {
    StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%us", seconds);
  }
  return std::wstring(buffer);
}

// ---------------------------------------------------------------------------
// JSON Escape helper
// ---------------------------------------------------------------------------
static std::wstring EscapeJsonString(const std::wstring &input) {
  std::wstring out;
  out.reserve(input.size() + 8);
  for (WCHAR ch : input) {
    switch (ch) {
    case L'\"':
      out += L"\\\"";
      break;
    case L'\\':
      out += L"\\\\";
      break;
    case L'\b':
      out += L"\\b";
      break;
    case L'\f':
      out += L"\\f";
      break;
    case L'\n':
      out += L"\\n";
      break;
    case L'\r':
      out += L"\\r";
      break;
    case L'\t':
      out += L"\\t";
      break;
    default:
      if (ch < 32) {
        WCHAR hexBuf[8];
        StringCchPrintfW(hexBuf, ARRAYSIZE(hexBuf), L"\\u%04x", ch);
        out += hexBuf;
      } else {
        out += ch;
      }
      break;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Storage Management
// ---------------------------------------------------------------------------
static void InitializeStoragePaths() {
  WCHAR basePath[MAX_PATH] = {0};

  // Try Windhawk's official storage path first
  size_t chars = Wh_GetModStoragePath(basePath, ARRAYSIZE(basePath));
  if (chars == 0 || basePath[0] == L'\0') {
    // Fallback to %LocalAppData%\TimeTrack
    WCHAR localAppData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0,
                                   localAppData))) {
      PathCombineW(basePath, localAppData, L"TimeTrack");
    } else {
      StringCchCopyW(basePath, ARRAYSIZE(basePath), L"C:\\TimeTrack");
    }
  }

  StringCchCopyW(g_storageDirectory, ARRAYSIZE(g_storageDirectory), basePath);
  CreateDirectoryW(g_storageDirectory, NULL);

  PathCombineW(g_todayJsonPath, g_storageDirectory, L"timetrack_today.json");

  PathCombineW(g_historyDirectory, g_storageDirectory, L"history");
  CreateDirectoryW(g_historyDirectory, NULL);
}

// ---------------------------------------------------------------------------
// JSON Persistence: Save
// ---------------------------------------------------------------------------
static void SaveDailyStatsToFile(const std::wstring &targetPath) {
  std::lock_guard<std::mutex> lock(g_statsMutex);

  std::vector<AppUsageRecord> sortedApps;
  sortedApps.reserve(g_dailyStats.apps.size());
  for (const auto &pair : g_dailyStats.apps) {
    sortedApps.push_back(pair.second);
  }

  std::sort(sortedApps.begin(), sortedApps.end(),
            [](const AppUsageRecord &a, const AppUsageRecord &b) {
              return a.activeSeconds > b.activeSeconds;
            });

  std::wostringstream ss;
  ss << L"{\n";
  ss << L"  \"date\": \"" << EscapeJsonString(g_dailyStats.dateString)
     << L"\",\n";
  ss << L"  \"totalScreenTimeSeconds\": " << g_dailyStats.totalScreenTimeSeconds
     << L",\n";
  ss << L"  \"totalScreenTimeFormatted\": \""
     << EscapeJsonString(
            FormatDuration(g_dailyStats.totalScreenTimeSeconds, true))
     << L"\",\n";
  ss << L"  \"lastUpdated\": \""
     << EscapeJsonString(GetCurrentTimestampString()) << L"\",\n";
  ss << L"  \"appCount\": " << sortedApps.size() << L",\n";
  ss << L"  \"apps\": [\n";

  for (size_t i = 0; i < sortedApps.size(); ++i) {
    const auto &app = sortedApps[i];
    ss << L"    {\n";
    ss << L"      \"exe\": \"" << EscapeJsonString(app.exeName) << L"\",\n";
    ss << L"      \"seconds\": " << app.activeSeconds << L",\n";
    ss << L"      \"formatted\": \""
       << EscapeJsonString(FormatDuration(app.activeSeconds)) << L"\",\n";
    ss << L"      \"focusCount\": " << app.focusCount << L"\n";
    ss << L"    }" << (i + 1 < sortedApps.size() ? L"," : L"") << L"\n";
  }

  ss << L"  ]\n";
  ss << L"}\n";

  std::wstring jsonContent = ss.str();

  // Write atomically using a temporary file
  WCHAR tempPath[MAX_PATH];
  StringCchPrintfW(tempPath, ARRAYSIZE(tempPath), L"%s.tmp",
                   targetPath.c_str());

  HANDLE hFile = CreateFileW(tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    // UTF-8 conversion
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, jsonContent.c_str(), -1,
                                         NULL, 0, NULL, NULL);
    if (utf8Length > 0) {
      std::vector<char> utf8Buffer(utf8Length);
      WideCharToMultiByte(CP_UTF8, 0, jsonContent.c_str(), -1,
                          utf8Buffer.data(), utf8Length, NULL, NULL);

      DWORD bytesWritten = 0;
      WriteFile(hFile, utf8Buffer.data(), (DWORD)utf8Length - 1, &bytesWritten,
                NULL);
    }
    CloseHandle(hFile);

    // Atomic replace
    MoveFileExW(tempPath, targetPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  }
}

// ---------------------------------------------------------------------------
// JSON Persistence: Load
// ---------------------------------------------------------------------------
static void LoadTodayStatsFromFile() {
  std::wstring todayStr = GetCurrentDateString();

  HANDLE hFile = CreateFileW(g_todayJsonPath, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_dailyStats.dateString = todayStr;
    g_dailyStats.totalScreenTimeSeconds = 0;
    g_dailyStats.apps.clear();
    return;
  }

  DWORD fileSize = GetFileSize(hFile, NULL);
  if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
    CloseHandle(hFile);
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_dailyStats.dateString = todayStr;
    return;
  }

  std::vector<char> buffer(fileSize + 1, 0);
  DWORD bytesRead = 0;
  ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
  CloseHandle(hFile);

  int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, NULL, 0);
  if (wideLen <= 0)
    return;

  std::vector<WCHAR> wideBuffer(wideLen);
  MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, wideBuffer.data(),
                      wideLen);
  std::wstring json(wideBuffer.data());

  // Basic, safe parser for our own schema
  std::lock_guard<std::mutex> lock(g_statsMutex);

  // Parse date
  size_t datePos = json.find(L"\"date\": \"");
  if (datePos != std::wstring::npos) {
    size_t start = datePos + 9;
    size_t end = json.find(L"\"", start);
    if (end != std::wstring::npos) {
      std::wstring fileDate = json.substr(start, end - start);
      if (fileDate != todayStr) {
        // Different day! Archive yesterday's file and start fresh
        WCHAR archivePath[MAX_PATH];
        PathCombineW(archivePath, g_historyDirectory,
                     (fileDate + L".json").c_str());
        MoveFileExW(g_todayJsonPath, archivePath, MOVEFILE_REPLACE_EXISTING);

        g_dailyStats.dateString = todayStr;
        g_dailyStats.totalScreenTimeSeconds = 0;
        g_dailyStats.apps.clear();
        return;
      }
    }
  }

  g_dailyStats.dateString = todayStr;

  // Parse totalScreenTimeSeconds
  size_t secPos = json.find(L"\"totalScreenTimeSeconds\": ");
  if (secPos != std::wstring::npos) {
    size_t start = secPos + 26;
    size_t end = json.find_first_of(L",\n\r", start);
    if (end != std::wstring::npos) {
      std::wstring secStr = json.substr(start, end - start);
      g_dailyStats.totalScreenTimeSeconds = (uint32_t)_wtoi(secStr.c_str());
    }
  }

  // Parse apps
  size_t appsArrayPos = json.find(L"\"apps\": [");
  if (appsArrayPos != std::wstring::npos) {
    size_t curPos = appsArrayPos;
    while (true) {
      size_t exePos = json.find(L"\"exe\": \"", curPos);
      if (exePos == std::wstring::npos)
        break;

      size_t exeStart = exePos + 8;
      size_t exeEnd = json.find(L"\"", exeStart);
      if (exeEnd == std::wstring::npos)
        break;
      std::wstring exeName = json.substr(exeStart, exeEnd - exeStart);

      uint32_t seconds = 0;
      size_t sPos = json.find(L"\"seconds\": ", exeEnd);
      if (sPos != std::wstring::npos) {
        size_t sStart = sPos + 11;
        size_t sEnd = json.find_first_of(L",\n\r", sStart);
        if (sEnd != std::wstring::npos) {
          seconds = (uint32_t)_wtoi(json.substr(sStart, sEnd - sStart).c_str());
        }
      }

      uint32_t focusCount = 1;
      size_t fcPos = json.find(L"\"focusCount\": ", exeEnd);
      if (fcPos != std::wstring::npos) {
        size_t fcStart = fcPos + 14;
        size_t fcEnd = json.find_first_of(L",\n\r}", fcStart);
        if (fcEnd != std::wstring::npos) {
          focusCount =
              (uint32_t)_wtoi(json.substr(fcStart, fcEnd - fcStart).c_str());
        }
      }

      AppUsageRecord record;
      record.exeName = exeName;
      record.activeSeconds = seconds;
      record.focusCount = focusCount;
      g_dailyStats.apps[exeName] = record;

      curPos = exeEnd;
    }
  }
}

// ---------------------------------------------------------------------------
// Active Window & Process Identification
// ---------------------------------------------------------------------------
static std::wstring GetActiveProcessExecutableName() {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd || !IsWindow(hwnd)) {
    return L"";
  }

  // Skip shell/desktop if desired or tag it cleanly
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0) {
    return L"";
  }

  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!hProcess) {
    return L"";
  }

  WCHAR fullPath[MAX_PATH];
  DWORD pathSize = ARRAYSIZE(fullPath);
  std::wstring exeName = L"";

  if (QueryFullProcessImageNameW(hProcess, 0, fullPath, &pathSize)) {
    PCWSTR fileName = PathFindFileNameW(fullPath);
    if (fileName && *fileName) {
      exeName = fileName;
    }
  }

  CloseHandle(hProcess);

  // If hosted inside UWP ApplicationFrameHost, resolve real child process
  // (e.g., Calculator, Settings)
  if (_wcsicmp(exeName.c_str(), L"ApplicationFrameHost.exe") == 0) {
    HWND hChild =
        FindWindowExW(hwnd, NULL, L"Windows.UI.Core.CoreWindow", NULL);
    if (hChild) {
      DWORD childPid = 0;
      GetWindowThreadProcessId(hChild, &childPid);
      if (childPid != 0 && childPid != pid) {
        HANDLE hChildProc =
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, childPid);
        if (hChildProc) {
          WCHAR childPath[MAX_PATH];
          DWORD childSize = ARRAYSIZE(childPath);
          if (QueryFullProcessImageNameW(hChildProc, 0, childPath,
                                         &childSize)) {
            PCWSTR childFileName = PathFindFileNameW(childPath);
            if (childFileName && *childFileName) {
              exeName = childFileName;
            }
          }
          CloseHandle(hChildProc);
        }
      }
    }
  }

  return exeName;
}

// ---------------------------------------------------------------------------
// User Inactivity Detection
// ---------------------------------------------------------------------------
static bool CheckIsUserIdle(int thresholdSeconds) {
  // 1. Check if desktop is locked or on secure desktop (Win + L)
  HDESK hDesk = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
  if (!hDesk) {
    return true; // Workstation locked or secure desktop active
  }
  CloseDesktop(hDesk);

  // 2. Check mouse/keyboard inactivity
  LASTINPUTINFO lii = {sizeof(LASTINPUTINFO)};
  if (!GetLastInputInfo(&lii)) {
    return false;
  }

  DWORD currentTick = GetTickCount();
  DWORD idleMs = currentTick - lii.dwTime;
  return idleMs >= (DWORD)(thresholdSeconds * 1000);
}

// ---------------------------------------------------------------------------
// Tracking Tick (Executed every second)
// ---------------------------------------------------------------------------
static void OnTrackingTick() {
  ModSettings settings;
  {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    settings = g_settings;
  }

  // 1. Day rollover check
  std::wstring currentDate = GetCurrentDateString();
  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    if (g_dailyStats.dateString.empty()) {
      g_dailyStats.dateString = currentDate;
    } else if (g_dailyStats.dateString != currentDate) {
      // Archive previous day
      WCHAR archivePath[MAX_PATH];
      PathCombineW(archivePath, g_historyDirectory,
                   (g_dailyStats.dateString + L".json").c_str());
      SaveDailyStatsToFile(archivePath);

      // Reset for today
      g_dailyStats.dateString = currentDate;
      g_dailyStats.totalScreenTimeSeconds = 0;
      g_dailyStats.apps.clear();
    }
  }

  // 2. Idle check
  bool idle = CheckIsUserIdle(settings.idleThresholdSeconds);
  g_isUserIdle = idle;

  if (idle) {
    return; // User is away: do not count towards screen time
  }

  // 3. Active application tracking
  std::wstring activeExe = GetActiveProcessExecutableName();
  if (activeExe.empty()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_dailyStats.totalScreenTimeSeconds++;

    auto &record = g_dailyStats.apps[activeExe];
    if (record.exeName.empty()) {
      record.exeName = activeExe;
      record.activeSeconds = 0;
      record.focusCount = 1;
    }

    if (activeExe != g_currentActiveApp) {
      record.focusCount++;
      g_currentActiveApp = activeExe;
    }

    record.activeSeconds++;
  }
}

// ---------------------------------------------------------------------------
// Tray Notification & Menu
// ---------------------------------------------------------------------------
static void UpdateTrayTooltip() {
  if (!g_trayIconAdded || !g_hTrayWnd)
    return;

  uint32_t totalSeconds = 0;
  std::wstring activeApp = L"None";
  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    totalSeconds = g_dailyStats.totalScreenTimeSeconds;
    if (!g_currentActiveApp.empty()) {
      activeApp = g_currentActiveApp;
    }
  }

  std::wstring durationStr = FormatDuration(totalSeconds);

  WCHAR tip[128];
  if (g_isUserIdle) {
    StringCchPrintfW(tip, ARRAYSIZE(tip), L"TimeTrack: %s (Idle)\nActive: %s",
                     durationStr.c_str(), activeApp.c_str());
  } else {
    StringCchPrintfW(tip, ARRAYSIZE(tip), L"TimeTrack: %s\nActive: %s",
                     durationStr.c_str(), activeApp.c_str());
  }

  StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip);
  Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void ShowTrayContextMenu(HWND hWnd) {
  POINT pt;
  GetCursorPos(&pt);

  HMENU hMenu = CreatePopupMenu();
  if (!hMenu)
    return;

  uint32_t totalSeconds = 0;
  std::wstring activeApp = L"None";
  std::vector<AppUsageRecord> sortedApps;

  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    totalSeconds = g_dailyStats.totalScreenTimeSeconds;
    activeApp = g_currentActiveApp.empty() ? L"None" : g_currentActiveApp;

    for (const auto &pair : g_dailyStats.apps) {
      sortedApps.push_back(pair.second);
    }
  }

  std::sort(sortedApps.begin(), sortedApps.end(),
            [](const AppUsageRecord &a, const AppUsageRecord &b) {
              return a.activeSeconds > b.activeSeconds;
            });

  // 1. Title Item (Bold Header)
  AppendMenuW(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_TITLE,
              L"⏱️ TimeTrack - Daily Screen Time");

  // 2. Total Screen Time
  WCHAR totalBuf[128];
  StringCchPrintfW(totalBuf, ARRAYSIZE(totalBuf), L"  Today: %s",
                   FormatDuration(totalSeconds, true).c_str());
  AppendMenuW(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_TOTAL_TIME, totalBuf);

  // 3. Active App
  WCHAR activeBuf[128];
  StringCchPrintfW(activeBuf, ARRAYSIZE(activeBuf), L"  Active: %s",
                   activeApp.c_str());
  AppendMenuW(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_ACTIVE_APP, activeBuf);

  AppendMenuW(hMenu, MF_SEPARATOR, ID_TRAY_SEPARATOR, NULL);

  // 4. Top Apps List
  if (!sortedApps.empty()) {
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED, 0, L"📊 Top Apps Today:");
    size_t displayCount = (std::min)((size_t)5, sortedApps.size());
    for (size_t i = 0; i < displayCount; ++i) {
      WCHAR appBuf[128];
      StringCchPrintfW(appBuf, ARRAYSIZE(appBuf), L"   %zu. %s (%s)", i + 1,
                       sortedApps[i].exeName.c_str(),
                       FormatDuration(sortedApps[i].activeSeconds).c_str());
      AppendMenuW(hMenu, MF_STRING | MF_DISABLED,
                  (UINT_PTR)(ID_TRAY_TOP_APP_START + i), appBuf);
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
  }

  // 5. Actions
  AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_FOLDER, L"📁 Open Data Folder");
  AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESET_TODAY, L"🔄 Reset Today's Stats");

  SetForegroundWindow(hWnd);
  TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd,
                 NULL);
  PostMessageW(hWnd, WM_NULL, 0, 0);
  DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// Tray Window Procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (uMsg) {
  case WM_TIMER:
    if (wParam == ID_TIMER_TICK) {
      OnTrackingTick();

      static int saveCounter = 0;
      static int tooltipCounter = 0;

      tooltipCounter++;
      if (tooltipCounter >= 3) {
        tooltipCounter = 0;
        UpdateTrayTooltip();
      }

      saveCounter++;
      int saveInterval = 30;
      {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        saveInterval = g_settings.saveIntervalSeconds;
      }
      if (saveCounter >= saveInterval) {
        saveCounter = 0;
        SaveDailyStatsToFile(g_todayJsonPath);
      }
    }
    return 0;

  case WM_TRAYICON:
    if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
      ShowTrayContextMenu(hWnd);
    }
    return 0;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_TRAY_OPEN_FOLDER:
      ShellExecuteW(NULL, L"open", g_storageDirectory, NULL, NULL,
                    SW_SHOWNORMAL);
      break;

    case ID_TRAY_RESET_TODAY:
      if (MessageBoxW(
              hWnd,
              L"Are you sure you want to reset today's screen time statistics?",
              L"TimeTrack - Confirm Reset",
              MB_YESNO | MB_ICONQUESTION) == IDYES) {
        {
          std::lock_guard<std::mutex> lock(g_statsMutex);
          g_dailyStats.totalScreenTimeSeconds = 0;
          g_dailyStats.apps.clear();
        }
        SaveDailyStatsToFile(g_todayJsonPath);
        UpdateTrayTooltip();
      }
      break;
    }
    return 0;

  case WM_DESTROY:
    if (g_trayIconAdded) {
      Shell_NotifyIconW(NIM_DELETE, &g_nid);
      g_trayIconAdded = false;
    }
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Worker Thread
// ---------------------------------------------------------------------------
static DWORD WINAPI TimeTrackWorkerThread(LPVOID) {
  WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
  wc.lpfnWndProc = TrayWndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = L"TimeTrackTrayWndClass";
  RegisterClassExW(&wc);

  g_hTrayWnd =
      CreateWindowExW(0, wc.lpszClassName, L"TimeTrackTrayWnd", WS_POPUP, 0, 0,
                      0, 0, NULL, NULL, wc.hInstance, NULL);

  if (!g_hTrayWnd) {
    Wh_Log(L"Failed to create TimeTrack message window");
    return 1;
  }

  // Add tray icon
  bool showIcon = true;
  {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    showIcon = g_settings.showTrayIcon;
  }

  if (showIcon) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hTrayWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   L"TimeTrack: Starting...");

    if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
      g_trayIconAdded = true;
    }
  }

  // Set 1-second interval timer
  SetTimer(g_hTrayWnd, ID_TIMER_TICK, 1000, NULL);

  // Initial tooltip update
  UpdateTrayTooltip();

  // Message loop
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  KillTimer(g_hTrayWnd, ID_TIMER_TICK);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}

// ---------------------------------------------------------------------------
// Settings Loader
// ---------------------------------------------------------------------------
static void LoadModSettings() {
  std::lock_guard<std::mutex> lock(g_settingsMutex);

  g_settings.idleThresholdSeconds = Wh_GetIntSetting(L"idleThresholdSeconds");
  if (g_settings.idleThresholdSeconds <= 0) {
    g_settings.idleThresholdSeconds = 120;
  }

  g_settings.saveIntervalSeconds = Wh_GetIntSetting(L"saveIntervalSeconds");
  if (g_settings.saveIntervalSeconds <= 0) {
    g_settings.saveIntervalSeconds = 30;
  }

  g_settings.showTrayIcon = Wh_GetIntSetting(L"showTrayIcon") != 0;
}

// ---------------------------------------------------------------------------
// Windhawk Lifecycle Callbacks
// ---------------------------------------------------------------------------
BOOL Wh_ModInit() {
  Wh_Log(L"TimeTrack initializing...");

  LoadModSettings();
  InitializeStoragePaths();
  LoadTodayStatsFromFile();

  g_stopRequested = false;
  g_hWorkerThread = CreateThread(NULL, 0, TimeTrackWorkerThread, NULL, 0, NULL);
  if (!g_hWorkerThread) {
    Wh_Log(L"Failed to launch TimeTrack background worker thread");
    return FALSE;
  }

  Wh_Log(L"TimeTrack initialized successfully. Tracking active screen time.");
  return TRUE;
}

void Wh_ModUninit() {
  Wh_Log(L"TimeTrack uninitializing...");

  g_stopRequested = true;

  if (g_hTrayWnd) {
    PostMessageW(g_hTrayWnd, WM_CLOSE, 0, 0);
  }

  if (g_hWorkerThread) {
    WaitForSingleObject(g_hWorkerThread, 3000);
    CloseHandle(g_hWorkerThread);
    g_hWorkerThread = NULL;
  }

  // Final flush to disk
  SaveDailyStatsToFile(g_todayJsonPath);

  Wh_Log(L"TimeTrack uninitialized.");
}

BOOL Wh_ModSettingsChanged(BOOL *bReload) {
  Wh_Log(L"TimeTrack settings changed.");
  *bReload = TRUE;
  return TRUE;
}
