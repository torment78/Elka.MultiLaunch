# Elka Multi-Launch

A small, dark-only Windows tray application. One application starts; up to ten companion applications launch automatically. Native C++17 and Win32, with no .NET, WebView, Electron, downloaded runtime, service, driver, or network dependency.

**Development release:** [v0.1.0-dev.1 downloads](https://github.com/torment78/Elka.MultiLaunch/releases/tag/v0.1.0-dev.1). Choose the dark installer or the portable ZIP. Development builds are prereleases; a stable release is not available yet. Current source is on the `dev` branch.

## Use

1. Run `Elka.MultiLaunch.exe` and select **New profile**.
2. Name the profile. Choose its trigger from **Processes** or **Browse EXE**.
3. Add up to ten launch targets using either picker. Each target supports optional arguments, a working folder, and **Skip if already running** (on by default).
4. Select **Save changes**. You can save up to ten independent profiles.
5. Closing the window hides it in the tray. Double-click the tray icon to open it; right-click for Open, Pause/Resume, or Exit.

The trigger is the application you start yourself. Multi-Launch watches for it; it does not start the trigger. A trigger already running when a profile is first enabled also fires the profile. The profile fires once per continuous running session, including multiple simultaneous instances. It rearms after all instances stop. Profile names and target edits do not replay a running trigger; disabling and re-enabling a profile rearms it. Launch loops are rejected when saving. Companion applications are left running when the trigger closes.

Changes take effect on **Save changes**. Closing to tray keeps unsaved edits in the window but the monitor continues using the last saved settings. Pause is temporary and is not saved across restarts.

## Detection and administrator mode

- **Regular mode (default):** one native process snapshot per second while an enabled profile is active. No WMI subscription. No admin requirement. Processes that start and exit between checks can be missed.
- **Passive mode:** subscribes to the Windows `Win32_ProcessStartTrace` and `Win32_ProcessStopTrace` notifications. Requires administrator rights in this app. It takes one initial snapshot when connecting or applying settings, then waits for notifications without periodic scans. It never silently falls back to polling. Subscription failure appears in History and the status line; Save reconnects.

**Run as administrator** is a saved setting, independent of the detection mode. Enabling passive mode also selects it. Use **Settings → Restart as administrator** to apply it now. Future manual launches request elevation when selected; Windows controls whether any UAC prompt appears. If elevation is declined, monitoring waits rather than running targets without the requested rights. Turning the option off takes effect after exiting and launching normally. Apps launched from an elevated Multi-Launch inherit elevation. The app uses native Windows event delivery through WMI; WMI's own shared service costs are separate from this process's resource use.

Both modes match the full EXE path, case-insensitively, in your current Windows session. Same-named EXEs in other folders do not match. The process picker lists system and protected processes too, but Windows may hide their paths. An unreadable path cannot be selected; browse to the EXE instead. Protected triggers whose paths Windows will not reveal cannot be verified even if their names match. A very short-lived process can exit before its notification's path is read.

## Windows startup

Put the portable EXE in its permanent location, or use the installer, before enabling **Start with Windows** and saving.

- With **Run as administrator** off, startup uses `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- With **Run as administrator** on, startup uses the current user's interactive logon task `\ElkaSoft\MultiLaunch-<user SID>`, with highest privileges, in either detection mode. Configure or remove it while Multi-Launch is elevated. No password is stored and no service is installed.

Startup opens the app quietly in the tray. Windows may delay startup. Disable startup and save before moving the portable EXE. The dark installer offers separate **Run as administrator** and **Start with Windows** choices; both start unchecked on a fresh installation. Selecting both configures the elevated task. The uninstaller removes startup entries pointing to its own installed EXE and preserves profiles and history. Windows may request elevation to remove an elevated task.

## Data and history

Settings: `%LOCALAPPDATA%\ElkaSoft\MultiLaunch\settings.elka`. This is a versioned UTF-8 text format with escaped fields. Saves replace the file atomically and retain the previous copy as `settings.elka.bak`. Invalid settings are not partially loaded.

Recent history: `history.log` in the same folder. It contains at most 200 activity records and shows launches, skipped targets, failures, and settings changes. Arguments are not logged. History uses the computer's local time.

## Build in Visual Studio

Open `Elka.MultiLaunch.sln` with Visual Studio 2022 or newer and the **Desktop development with C++** workload, then build **Release / x64**. The project defaults to toolset v143; retarget to your installed toolset if needed (v145 for Visual Studio 2026).

Alternatively, from PowerShell:

```powershell
.\scripts\Build.ps1 -Package
.\build\Release\Elka.MultiLaunch.Tests.exe --integration
# Run in an elevated terminal for the event-delivery integration test:
.\build\Release\Elka.MultiLaunch.Tests.exe --passive-integration
```

The build script finds the local Visual Studio toolchain and runs the core tests. `-Package` creates a standalone EXE, portable ZIP, and, if Inno Setup 6.6 or newer is installed, a dark per-user installer in `artifacts`. The C++ runtime is linked statically. Windows 10/11 x64 is the target; development validation is recorded in `VALIDATION.md`.

For isolated UI testing: `Elka.MultiLaunch.exe --data-dir "C:\path\to\test-data"`. That session cannot modify Windows startup. Test fixture executables are built locally from the test program and exit automatically after 30 seconds.

Microsoft references: [process snapshots](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot), [process-start notifications](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/krnlprov/win32-processstarttrace), [notification area](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area), and [Windows Run keys](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys).
