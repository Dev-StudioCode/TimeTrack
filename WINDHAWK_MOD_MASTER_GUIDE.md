# 🦅 Windhawk Mod Master Development & Troubleshooting Guide

> **A comprehensive reference manual and Standard Operating Procedure (SOP) for developing, testing, publishing, and updating Windhawk mods with 100% first-attempt success.**

---

## 📑 Table of Contents
1. [Overview & Philosophy](#-overview--philosophy)
2. [Part 1: The "Mistakes to Avoid" Hall of Fame (Errors & Solutions)](#part-1-the-mistakes-to-avoid-hall-of-fame-errors--solutions)
   - [Error 1: Multi-line `@description` in Metadata Block](#error-1-multi-line-description-in-metadata-block)
   - [Error 2: Malformed YAML in Settings Block](#error-2-malformed-yaml-in-settings-block)
   - [Error 3: Missing / Generic `@author` and `@github` Metadata](#error-3-missing--generic-author-and-github-metadata)
   - [Error 4: Missing `## Mod authorship` in Pull Request Description](#error-4-missing-mod-authorship-in-pull-request-description)
   - [Error 5: Multiple Mod Files in a Single Pull Request](#error-5-multiple-mod-files-in-a-single-pull-request)
   - [Error 6: Committing Directly to `main` Instead of a Feature Branch](#error-6-committing-directly-to-main-instead-of-a-feature-branch)
   - [Error 7: Upstream vs. Fork Confusion (Looking for "Sync fork" on Upstream)](#error-7-upstream-vs-fork-confusion-looking-for-sync-fork-on-upstream)
   - [Error 8: Editing the Bot's Comment Instead of Your PR Description](#error-8-editing-the-bots-comment-instead-of-your-pr-description)
   - [Error 9: Accidental Stray Keystrokes in Source Code](#error-9-accidental-stray-keystrokes-in-source-code)
   - [Error 10: Missing Linker Libraries in `@compilerOptions`](#error-10-missing-linker-libraries-in-compileroptions)
   - [Error 11: COM Threading & Endpoint Callback Crashes](#error-11-com-threading--endpoint-callback-crashes)
3. [Part 2: The Golden C++ Mod Template (Zero-Error Reference)](#part-2-the-golden-c-mod-template-zero-error-reference)
4. [Part 3: End-to-End Lifecycle: Deploying a New Mod (Zero to Marketplace)](#part-3-end-to-end-lifecycle-deploying-a-new-mod-zero-to-marketplace)
   - [Phase 1: Local Workspace & Naming Convention](#phase-1-local-workspace--naming-convention)
   - [Phase 2: Developing & Testing Locally in Windhawk Developer Mode](#phase-2-developing--testing-locally-in-windhawk-developer-mode)
   - [Phase 3: Creating Your Standalone GitHub Showcase Repository](#phase-3-creating-your-standalone-github-showcase-repository)
   - [Phase 4: Fork Management & Dedicated Branching](#phase-4-fork-management--dedicated-branching)
   - [Phase 5: Submitting the Pull Request & Running Reviews](#phase-5-submitting-the-pull-request--running-reviews)
5. [Part 4: Future Updates Guide (How to Release v1.1.0, v1.2.0, etc.)](#part-4-future-updates-guide-how-to-release-v110-v120-etc)
6. [Part 5: Project Consistency & Registry Mapping Table](#part-5-project-consistency--registry-mapping-table)

---

## 🎯 Overview & Philosophy

Windhawk uses an automated CI (Continuous Integration) pipeline based on Python (`compile_mod.py` and `pr_validation.py`) to validate, compile, and publish mods. 

Because the validation is automated:
* **The parser is completely unforgiving**: A single wrapped line or missing space will reject the entire mod.
* **The repository enforces isolation**: Exactly **one mod file per PR**.
* **Understanding Git layers is essential**: Your Local PC, your Standalone Showcase Repo, your Fork Branch, and the Upstream Marketplace are distinct stages in the pipeline.

This guide documents every mistake encountered in previous projects and provides the exact rules to achieve a **clean, first-pass approval**.

---

## Part 1: The "Mistakes to Avoid" Hall of Fame (Errors & Solutions)

### Error 1: Multi-line `@description` in Metadata Block

#### 🔴 The Error
```text
Compiling mods/remove-shortcut-arrows.wh.cpp
RuntimeError: Invalid metadata line format: // Explorer icons cleanly in memory without touching the Windows registry.
Error: Process completed with exit code 1.
```

#### 🔍 Why It Happened
In `remove-shortcut-arrows.wh.cpp`, the description was split across two lines for visual neatness:
```cpp
// ==WindhawkMod==
// @id              remove-shortcut-arrows
// @name            Remove Shortcut Arrow Overlays
// @description     Removes shortcut arrow overlays from desktop and File
// Explorer icons cleanly in memory without touching the Windows registry.
// ==/WindhawkMod==
```
Windhawk's Python script regex matches `^//[ \t]+@([a-zA-Z]+)...`. The second line (`// Explorer icons...`) did not start with `// @`, so the parser crashed.

#### 🟢 The Prevention Rule
* **Every single line** between `// ==WindhawkMod==` and `// ==/WindhawkMod==` **MUST** begin with `// @<property>`.
* Property values (such as `@description`) must **NEVER** wrap across multiple lines. Always keep them on a single line, no matter how long.

```cpp
// 🟢 CORRECT:
// @description     Removes shortcut arrow overlays from desktop and File Explorer icons cleanly in memory.
```

---

### Error 2: Malformed YAML in Settings Block

#### 🔴 The Error
```text
Error: USAGE: Failed to parse settings: simple key expect ':' at byte 220 line 5 column 1 (in 'mods/remove-shortcut-arrows.wh.cpp')
```

#### 🔍 Why It Happened
Inside `// ==WindhawkModSettings==`, the YAML `$description` was wrapped without standard YAML indentation:
```yaml
- custom_icon_path: ""
  $name: Custom Overlay Icon Path (Optional)
  $description: Leave this empty to use the built-in transparent icon
(recommended). If you want a custom overlay, specify the full path to a valid
.ico file (e.g. C:\icons\my_custom.ico).
```
Windhawk 2.0 uses a strict C++ YAML parser (`yaml-cpp`). Because line 4 and 5 started at column 1 without spaces or colons, the parser thought they were malformed dictionary keys and failed.

#### 🟢 The Prevention Rule
Keep all string values inside `==WindhawkModSettings==` on a **single line**:
```yaml
# 🟢 CORRECT:
- custom_icon_path: ""
  $name: Custom Overlay Icon Path (Optional)
  $description: Leave this empty to use the built-in transparent icon (recommended). If you want a custom overlay, specify the full path to a valid .ico file (e.g. C:\icons\my_custom.ico).
```

---

### Error 3: Missing / Generic `@author` and `@github` Metadata

#### 🔴 The Error
```text
Warning: Expected @github (https://github.com/) to match the pull request author (https://github.com/Dev-StudioCode).
```

#### 🔍 Why It Happened
In the initial draft, `@author` was set to `Community` and `@github` was set to `https://github.com/ramensoftware/windhawk-mods` or `https://github.com/`. 
Windhawk's CI checks whether `@github` points to the actual GitHub user who created the Pull Request.

#### 🟢 The Prevention Rule
Always set `@author` to your exact handle and `@github` to your GitHub profile URL:
```cpp
// 🟢 CORRECT:
// @author          Dev-StudioCode
// @github          https://github.com/Dev-StudioCode
```

---

### Error 4: Missing `## Mod authorship` in Pull Request Description

#### 🔴 The Error
```text
Warning: New mod submissions must keep the "## Mod authorship" section from the pull request template in the PR description, so reviewers know how the mod was authored. Please restore that section and fill it in.
Error: Process completed with exit code 1.
```

#### 🔍 Why It Happened
When creating a Pull Request on GitHub, the default PR template was cleared and replaced entirely by the project README. Windhawk's `pr_validation.py` script specifically scans the PR body text for the string `## Mod authorship`. If missing, the automated check fails.

#### 🟢 The Prevention Rule
Whenever creating a PR for a **new mod**, **ALWAYS** append this exact block at the bottom of your PR description:

```markdown
## Mod authorship

If this pull request introduces a new mod, please complete the section below.

This mod was created by:

- [ ] The submitter, without AI assistance
- [x] The submitter, with AI assistance
- [ ] Claude
- [ ] ChatGPT
- [ ] Gemini
- [ ] Another AI (please specify): 
- [ ] Other (please specify): 
```

---

### Error 5: Multiple Mod Files in a Single Pull Request

#### 🔴 The Error
```text
@Dev-StudioCode `/ai-review` can't be applied here: a pull request must add or update a single mod file under `mods/`, but it changes 2 files.
```

#### 🔍 Why It Happened
Both `mods/audio-hardware-volume-preset.wh.cpp` and `mods/remove-shortcut-arrows.wh.cpp` were placed on the fork's `main` branch. 
Windhawk requires strictly **1 mod file per Pull Request** because mods are versioned, cataloged, and reviewed independently.

#### 🟢 The Prevention Rule
* **NEVER work directly on the `main` branch of your fork.**
* Always create a dedicated branch for each mod (e.g., `remove-shortcut-arrows`). A PR created from that branch will only touch that single mod.

---

### Error 6: Committing Directly to `main` Instead of a Feature Branch

#### 🔴 The Problem
Over time, your fork's `main` branch becomes *ahead* by previous mod commits and *behind* the official repository (e.g., `Ahead by: 10 Behind by: 103`). Any new PR opened from `main` accidentally pulls in months of old commits.

#### 🟢 The Prevention Rule
* Keep `main` in your fork untouched and clean (always synced with `ramensoftware:main`).
* For every new mod or mod update, branch off from `main`:
  ```bash
  git checkout main
  git pull upstream main
  git checkout -b new-mod-name
  ```

---

### Error 7: Upstream vs. Fork Confusion (Looking for "Sync fork" on Upstream)

#### 🔴 The Confusion
Navigating to `https://github.com/ramensoftware/windhawk-mods` and looking for the "Sync fork" button. Clicking "Fork" produces:
> *"No available destinations to fork this repository. Dev-StudioCode/windhawk-mods"*

#### 🟢 The Clarification
* `ramensoftware/windhawk-mods` is the **Upstream Root Repository**. It cannot sync from itself.
* Your personal copy is **`https://github.com/Dev-StudioCode/windhawk-mods`**.
* The **Sync fork** button ONLY exists on your personal fork page.

---

### Error 8: Editing the Bot's Comment Instead of Your PR Description

#### 🔴 The Confusion
Clicking the `...` (three dots) on the second comment (`windhawk-reviewer bot`) and seeing only *Copy link*, *Quote reply*, but **no "Edit" option**.

#### 🟢 The Clarification
You cannot edit someone else's comment (or a bot's comment). The **PR Description** is the **very first comment box at the top of the conversation**. Look at the top-right corner of the *first* box (where your own username is) to find the **Edit** button.

---

### Error 9: Accidental Stray Keystrokes in Source Code

#### 🔴 The Error
Typing `q` or `qq` into the code while navigating files in VS Code, causing syntax errors:
```cpp
#include <windows.h>
qq
#include <string>
```

#### 🟢 The Prevention Rule
* Always run `git diff` or review unstaged changes before committing.
* In VS Code, check the Problems panel (`Ctrl + Shift + M`) to ensure 0 errors exist before staging files.

---

### Error 10: Missing Linker Libraries in `@compilerOptions`

#### 🔴 The Error
```text
undefined reference to `CreateDIBSection'
undefined reference to `SHGetImageList'
```

#### 🟢 The Prevention Rule
If your mod calls Windows Shell or GDI APIs, declare the appropriate libraries in `// @compilerOptions`:
* GDI drawing (`CreateDIBSection`, `CreateBitmap`): `-lgdi32`
* Shell controls & Image lists (`SHGetImageList`): `-lcomctl32 -lshlwapi`
* COM objects (`CoCreateInstance`, `CoInitializeEx`): `-lole32`
* Common Dialogs: `-lcomdlg32`
* Theme / DWM: `-luxtheme -ldwmapi`

Example:
```cpp
// @compilerOptions -lole32 -lshlwapi -lgdi32 -lcomctl32
```

---

### Error 11: COM Threading & Endpoint Callback Crashes

#### 🔴 Lessons from the Volume Mod (`audio-hardware-volume-preset.wh.cpp`)
* Windows Audio APIs (`IMMDeviceEnumerator`, `IAudioEndpointVolume`) require COM. Calling them from raw hooks or arbitrary threads without COM initialization causes access violations (`0x800401F0: CO_E_NOTINITIALIZED`).
* Always call `CoInitializeEx(NULL, COINIT_MULTITHREADED)` in worker threads and `CoUninitialize()` on termination.
* Never block Explorer's main UI thread with synchronous COM enumerations. Dispatch device checks onto a lightweight background thread synchronized with a Win32 Event (`CreateEvent`, `SetEvent`, `WaitForSingleObject`).

---

## Part 2: The Golden C++ Mod Template (Zero-Error Reference)

Use this exact skeleton when creating any new `.wh.cpp` file:

```cpp
// ==WindhawkMod==
// @id              your-mod-id-here
// @name            Your Mod Title Here
// @description     Single line description of what the mod does without any line breaks.
// @version         1.0.0
// @author          Dev-StudioCode
// @github          https://github.com/Dev-StudioCode
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -lshlwapi -lgdi32 -lcomctl32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Your Mod Title Here

Detailed description of the mod, why it exists, features, and benefits.

### Features
* Feature 1
* Feature 2
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- setting_name: "default_value"
  $name: Human Readable Setting Name
  $description: Single line description of this setting without breaking lines.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>
#include <windows.h>

// ---------------------------------------------------------------------------
// Settings & State
// ---------------------------------------------------------------------------

static void LoadSettings() {
    // Read your settings here
}

// ---------------------------------------------------------------------------
// Entry Points (Standard Windhawk Lifecycle)
// ---------------------------------------------------------------------------

BOOL Wh_ModInit() {
    Wh_Log(L"Initializing Your Mod");
    LoadSettings();
    // Setup hooks or memory modifications
    return TRUE;
}

void Wh_ModAfterInit() {
    // Actions performed after injection completes (e.g. refresh UI)
}

void Wh_ModUninit() {
    Wh_Log(L"Uninitializing Your Mod");
    // Clean up hooks, restore state, release COM objects
}

BOOL Wh_ModSettingsChanged(BOOL *bReload) {
    *bReload = TRUE;
    return TRUE;
}
```

---

## Part 3: End-to-End Lifecycle: Deploying a New Mod (Zero to Marketplace)

Follow this structured workflow for every mod from start to finish:

```
[Phase 1: Local PC]  ──>  [Phase 2: Standalone Repo]  ──>  [Phase 3: Fork Branch]  ──>  [Phase 4: Official Marketplace]
D:\#Anti Gravity\...       Dev-StudioCode/<Mod-Repo>       Dev-StudioCode/windhawk-mods       ramensoftware/windhawk-mods
```

### Phase 1: Local Workspace & Naming Convention
1. Choose a clean **Mod ID** (e.g., `remove-shortcut-arrows`).
2. Keep all names synchronized:
   * **Local Folder**: `D:\#Anti Gravity\<Descriptive Name>`
   * **Mod Source File**: `<mod-id>.wh.cpp`
   * **Mod ID inside code**: `@id <mod-id>`
   * **Git Branch**: `<mod-id>`

### Phase 2: Developing & Testing Locally in Windhawk Developer Mode
1. Open the **Windhawk** desktop application.
2. Click `...` (top right) > **Advanced** > toggle **Developer mode** ON.
3. Go to the **Mods** tab > click **Create New Mod**.
4. Set the Mod ID to your chosen `<mod-id>`.
5. Paste your code and click **Compile & Save**.
6. Check Windhawk's log window for your `Wh_Log` messages. Verify that injecting, unloading, and toggling settings works with 0 crashes.

### Phase 3: Creating Your Standalone GitHub Showcase Repository
1. In your local folder, initialize Git:
   ```bash
   git init
   git add .
   git commit -m "feat: initial commit"
   ```
2. Create a new public repository on your GitHub account (`https://github.com/Dev-StudioCode/<Mod-Repository-Name>`).
3. Add a professional `README.md`, `.gitignore`, and `LICENSE` (MIT).
4. Push to your repository:
   ```bash
   git remote add origin https://github.com/Dev-StudioCode/<Mod-Repository-Name>.git
   git push -u origin main
   ```

### Phase 4: Fork Management & Dedicated Branching
1. Go to your personal fork of Windhawk mods:
   `https://github.com/Dev-StudioCode/windhawk-mods`
2. **Sync the `main` branch first**:
   * Click **Sync fork**.
   * If it says *commits ahead*, click **Discard commits** to make it 100% clean and match the official repo.
3. **Create the dedicated feature branch**:
   * Click the branch dropdown (where it says `main`).
   * Type your mod ID (e.g., `remove-shortcut-arrows`).
   * Click **Create branch: <mod-id> from main**.
4. **Upload the mod file**:
   * Ensure your branch is selected.
   * Open the **`mods/`** directory.
   * Click **Add file** > **Upload files**.
   * Upload `<mod-id>.wh.cpp`.
   * Commit directly to your feature branch.

### Phase 5: Submitting the Pull Request & Running Reviews
1. Go to the official repository:
   `https://github.com/ramensoftware/windhawk-mods`
2. Click **Pull requests** > **New pull request**.
3. Set the compare branch:
   * Base repository: `ramensoftware/windhawk-mods` (base: `main`)
   * Head repository: `Dev-StudioCode/windhawk-mods` (compare: `<your-branch>`)
4. Verify the file diff: **Only 1 file changed** (`mods/<mod-id>.wh.cpp`).
5. In the PR Description box, include your summary and append the required template:
   ```markdown
   ## Mod authorship

   This mod was created by:
   - [ ] The submitter, without AI assistance
   - [x] The submitter, with AI assistance
   - [ ] Gemini
   ```
6. Click **Create pull request**.
7. In the conversation box, trigger the automated review by commenting:
   ```text
   /ai-review
   ```
8. The bot will compile and inspect the code. When all checks are green, comment:
   ```text
   /ready-for-reviewer
   ```
9. Ramen Software will review and merge it. Within minutes, it goes live on **windhawk.net**!

---

## Part 4: Future Updates Guide (How to Release v1.1.0, v1.2.0, etc.)

When you want to update an existing mod that is already in the marketplace:

1. **Update code locally**: Make your improvements in your local workspace.
2. **Bump the `@version` tag**:
   ```cpp
   // @version         1.1.0   // Increased from 1.0.0
   ```
3. **Sync your fork branch**:
   * Switch to your fork: `https://github.com/Dev-StudioCode/windhawk-mods`
   * Sync your branch or `main` so it has the latest commits.
4. **Upload the modified file**: Replace `mods/<mod-id>.wh.cpp` on your branch.
5. **Open an Update Pull Request**:
   * The PR template requires a `## Changelog` block for updates:
   ```markdown
   ## Changelog

   <!-- changelog:start -->
   * Added high-DPI scaling support.
   * Fixed overlay redraw delay when returning from sleep.
   <!-- changelog:end -->
   ```
6. Comment `/ai-review`, then `/ready-for-reviewer`.
7. Once merged, Windhawk notifies all users worldwide to update their installed mod!

---

## Part 5: Project Consistency & Registry Mapping Table

Keep all mod naming strictly aligned across all 5 layers:

| Layer | Volume Mod (Live) | Shortcut Arrows Mod (In Review) | Future Mod (Example) |
| :--- | :--- | :--- | :--- |
| **Mod ID** | `audio-hardware-volume-preset` | `remove-shortcut-arrows` | `my-cool-mod` |
| **C++ File** | `mods/audio-hardware-volume-preset.wh.cpp` | `mods/remove-shortcut-arrows.wh.cpp` | `mods/my-cool-mod.wh.cpp` |
| **Fork Branch** | `audio-hardware-volume-preset` | `remove-shortcut-arrows` | `my-cool-mod` |
| **Standalone Repo** | `Dev-StudioCode/Windows-Hardware-Volume-Set` | `Dev-StudioCode/Hide-Desktop-Shortcut-Icon` | `Dev-StudioCode/My-Cool-Mod` |
| **Local Folder** | `D:\#Anti Gravity\Windows Hardware Volume Set` | `D:\#Anti Gravity\Hide Desktop Shortcut Icon` | `D:\#Anti Gravity\My Cool Mod` |
| **Marketplace URL**| `windhawk.net/mods/audio-hardware-volume-preset` | `windhawk.net/mods/remove-shortcut-arrows` | `windhawk.net/mods/my-cool-mod` |

---
*Created and maintained as a central blueprint for Dev-StudioCode Windhawk mods.*
