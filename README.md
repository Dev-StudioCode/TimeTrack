# TimeTrack

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?logo=windows&logoColor=white)](https://microsoft.com)
[![Windhawk Mod](https://img.shields.io/badge/Windhawk-Mod-00A4EF?logo=visual-studio-code&logoColor=white)](https://windhawk.net)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![Architecture](https://img.shields.io/badge/Architecture-x86--64-informational)](https://github.com/Dev-StudioCode/TimeTrack)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Privacy](https://img.shields.io/badge/Privacy-100%25%20Offline%20%7C%20Local-success)](#privacy--security-guarantee)

> An ultra-lightweight, native digital wellbeing and active screen time monitor for Windows. Runs unobtrusively inside Windows Explorer via [Windhawk](https://windhawk.net/) with zero background telemetry, minimal memory usage, and no external runtime dependencies.

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Performance & Architecture](#performance--architecture)
- [Installation Guide](#installation-guide)
- [Configuration Reference](#configuration-reference)
- [Data Storage & Schema](#data-storage--schema)
- [Privacy & Security Guarantee](#privacy--security-guarantee)
- [Compiling from Source](#compiling-from-source)
- [Author & License](#author--license)

---

## Overview

Modern operating systems often obscure productivity data behind cloud sync services or cumbersome background suites. **TimeTrack** takes a minimalist, systems-first approach:

- Injected directly into `explorer.exe` using native Win32 APIs.
- Operates strictly on your local machine with zero network activity.
- Generates transparent, human-readable JSON logs for personal analytics or integration with third-party dashboards.

---

## Key Features

### ⏱️ High-Precision Foreground Tracking
Tracks the currently focused window using Win32 event queries. Session duration and switch counts are recorded per executable (e.g., `chrome.exe`, `Code.exe`).

### 💤 Smart Inactivity & Lock Detection
- **Input Inactivity:** Automatically pauses duration accumulation when no mouse or keyboard events are registered within the user-defined threshold (default: 120 seconds).
- **Session Locking (`Win + L`):** Accurately detects workstation lock states and secure desktop switches (`OpenInputDesktop`), ensuring locked screen hours are excluded from usage data.

### 🪟 Modern Application Resolution (UWP)
Inspects Windows 10/11 Universal Windows Platform (UWP) host containers (`ApplicationFrameHost.exe`) to resolve underlying application binaries (such as `SystemSettings.exe` or `CalculatorApp.exe`) rather than generic container names.

### 🔔 Native System Tray Integration
- **Real-Time Tooltip:** Hover over the tray icon to view current daily screen time, active application, and idle state.
- **Context Menu:** Right-click to inspect today's top 5 most used applications, open the storage folder in File Explorer, or reset daily statistics.
- **Dismissal Handling:** Implements standardized Win32 menu dismissal routines (`WS_POPUP` hierarchy with `PostMessage(WM_NULL)`) for seamless desktop interaction.

### 🔄 Automatic Daily Rollover & Atomic Persistence
- Automatically detects midnight day transitions and archives the previous day's metrics to `history/YYYY-MM-DD.json`.
- Persists current metrics periodically to disk via atomic file replacement (`.tmp` to `.json` rename), protecting against data corruption during abrupt shutdowns.

---

## Performance & Architecture

Unlike standalone tracking utilities built on Electron, Python, or the JVM, TimeTrack executes as a lightweight native C++ extension inside Windows Explorer.

| Metric | TimeTrack (C++ Windhawk Mod) | Typical Standalone Tracker |
| :--- | :--- | :--- |
| **RAM Consumption** | **< 4 MB** | 80 MB – 250 MB+ |
| **CPU Utilization** | **~0.0%** (1s tick polling) | 0.5% – 2.0% |
| **Network Requests** | **0 (Strictly Offline)** | Cloud analytics / telemetry |
| **Startup Overhead** | Native DLL Injection | Process bootstrap & window render |
| **Dependencies** | None (pure Win32 SDK) | Web runtimes / framework DLLs |

---

## Installation Guide

### Via Windhawk Developer Mode

1. Ensure [Windhawk](https://windhawk.net/) (v1.4 or later) is installed.
2. Open the Windhawk user interface.
3. Open the menu (**⋮**) in the upper right corner and navigate to **Advanced** > toggle **Developer mode** to ON.
4. In the **Mods** tab, select **Create New Mod**.
5. Set the mod ID to `timetrack`.
6. Replace the boilerplate code with the complete contents of [`timetrack.wh.cpp`](./timetrack.wh.cpp).
7. Click **Compile and run**.
8. The TimeTrack indicator will appear in your system tray immediately.

---

## Configuration Reference

Settings can be adjusted on the fly through the **Settings** tab in Windhawk. Changes are applied automatically via dynamic mod reloading.

| Setting Name | Key | Type | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Inactivity timeout** | `idleThresholdSeconds` | Integer | `120` | Duration (in seconds) of input inactivity required before screen time tracking pauses. |
| **Auto-save interval** | `saveIntervalSeconds` | Integer | `30` | Frequency (in seconds) at which active tracking metrics are flushed to local storage. |
| **Show System Tray Icon** | `showTrayIcon` | Boolean | `true` | Toggles the visibility of the TimeTrack icon in the taskbar notification area. |

---

## Data Storage & Schema

All metrics are stored locally under your dedicated Windhawk storage path or `%LocalAppData%\TimeTrack\`.

```
%LocalAppData%\TimeTrack\
├── timetrack_today.json       # Current active day's usage
└── history\
    ├── 2026-10-20.json        # Archived historical records
    ├── 2026-10-21.json
    └── ...
```

### JSON Schema Example

```json
{
  "date": "2026-10-23",
  "totalScreenTimeSeconds": 14520,
  "totalScreenTimeFormatted": "4h 2m 0s",
  "lastUpdated": "2026-10-23 16:30:00",
  "appCount": 3,
  "apps": [
    {
      "exe": "Code.exe",
      "seconds": 8400,
      "formatted": "2h 20m",
      "focusCount": 42
    },
    {
      "exe": "chrome.exe",
      "seconds": 5100,
      "formatted": "1h 25m",
      "focusCount": 65
    },
    {
      "exe": "WindowsTerminal.exe",
      "seconds": 1020,
      "formatted": "17m",
      "focusCount": 19
    }
  ]
}
```

---

## Privacy & Security Guarantee

- **Zero Network Activity:** TimeTrack contains no networking libraries and issues no socket connections, HTTP requests, or external telemetry pings.
- **Local Sovereignty:** All gathered metrics remain exclusively on your computer in standard JSON format.
- **Non-Invasive Process Inspection:** Window inspection utilizes restricted queries (`PROCESS_QUERY_LIMITED_INFORMATION`), reading only module filenames and top-level handles without memory scanning or keystroke recording.

---

## Compiling from Source

To verify or compile the mod directly using Windhawk's embedded Clang toolchain:

```powershell
& 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe' `
    -DWH_MOD `
    -DWH_EDITING `
    -target x86_64-w64-mingw32 `
    -I "C:\Program Files\Windhawk\Compiler\include" `
    -c timetrack.wh.cpp `
    -o timetrack.o
```

---

## Author & License

- **Author:** [Dev-StudioCode](https://github.com/Dev-StudioCode)
- **License:** Licensed under the [MIT License](LICENSE). Free for personal and commercial use.
