# TimeTrack

A lightweight, privacy-respecting digital wellbeing and screen time monitor for Windows, running natively inside Windows Explorer via [Windhawk](https://windhawk.net/).

---

## 🌟 Highlights

- **⏱️ Active Screen Time:** Measures genuine foreground screen time per application.
- **💤 Inactivity Detection:** Automatically detects when you step away from the keyboard and mouse and pauses the timer.
- **📊 App Breakdown & Focus Counts:** Records active duration and window switch counts for every app.
- **🔔 Live System Tray Tooltip & Menu:** Hover to see your daily stats at a glance; right-click for top apps and quick actions.
- **🔒 100% Offline & Private:** Zero internet access, zero telemetry. All stats stay strictly on your computer in clean JSON format.
- **⚡ High Performance:** Pure native C++ code compiled into Windows Explorer with negligible RAM footprint (< 5 MB) and 0% CPU impact.

---

## 🛠️ How to Install in Windhawk

1. Make sure [Windhawk](https://windhawk.net/) is installed on your Windows PC.
2. In Windhawk, navigate to the **Mods** tab.
3. Click the three dots menu (**⋮**) in the top right corner and choose **New mod**.
4. Copy and paste the entire contents of [`TimeTrack.wh.cpp`](./TimeTrack.wh.cpp) into the editor.
5. Click **Compile and run**.
6. The TimeTrack icon will appear in your system tray, actively tracking your screen time!

---

## ⚙️ Configuration & Settings

Inside Windhawk, you can customize the following settings under the mod's **Settings** tab:

| Setting | Default | Description |
| :--- | :--- | :--- |
| **Inactivity timeout (seconds)** | `120` | Pauses screen time counting when no keyboard or mouse activity is detected for this duration. |
| **Auto-save interval (seconds)** | `30` | Frequency in seconds to persist daily statistics to disk. |
| **Show System Tray Icon** | `true` | Toggles the visibility of the TimeTrack icon in the Windows notification area. |

---

## 📁 Data Storage

All data is stored in human-readable JSON format under:
`%LocalAppData%\TimeTrack\` (or Windhawk's mod data folder):

- **`timetrack_today.json`**: Current day's active duration and per-application breakdown.
- **`history/YYYY-MM-DD.json`**: Archived daily snapshots, automatically rotated at midnight.

### Sample JSON Format

```json
{
  "date": "2026-10-20",
  "totalScreenTimeSeconds": 8420,
  "totalScreenTimeFormatted": "2h 20m 20s",
  "lastUpdated": "2026-10-20 16:45:10",
  "appCount": 3,
  "apps": [
    {
      "exe": "chrome.exe",
      "seconds": 4200,
      "formatted": "1h 10m",
      "focusCount": 32
    },
    {
      "exe": "Code.exe",
      "seconds": 3600,
      "formatted": "1h 0m",
      "focusCount": 24
    },
    {
      "exe": "spotify.exe",
      "seconds": 620,
      "formatted": "10m",
      "focusCount": 5
    }
  ]
}
```

---

## 📄 License

MIT License. Free and open source for everyone.
