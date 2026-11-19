# Elka Multi-Launch

A small, dark-only Windows tray application with optional floating game-launcher tiles. One application starts; up to ten companion applications launch automatically. Native C++17 and Win32, with no .NET, WebView, Electron, downloaded runtime, service, or driver. Game discovery works locally; GitHub is contacted only when you choose **Check updates**.

**Development release:** [v0.2.0-dev.5 downloads](https://github.com/torment78/Elka.MultiLaunch/releases/tag/v0.2.0-dev.5). Choose the dark installer or the portable ZIP. Development builds are prereleases; a stable release is not available yet. Current source is on the `dev` branch.

## Use

1. Run `Elka.MultiLaunch.exe` and select **New profile**.
2. Name the profile. Choose its trigger from **Processes** or **Browse EXE**.
3. Add up to ten launch targets using either picker. Each target supports optional arguments, a working folder, and **Skip if already running** (on by default).
4. Select **Save changes**. You can save up to ten independent profiles.
5. Closing the window hides it in the tray. Double-click the tray icon to open it; right-click for Open, Pause/Resume, or Exit.

The trigger is the application you start yourself. Multi-Launch watches for it; it does not start the trigger. A trigger already running when a profile is first enabled also fires the profile. The profile fires once per continuous running session, including multiple simultaneous instances. It rearms after all instances stop. Profile names and target edits do not replay a running trigger; disabling and re-enabling a profile rearms it. Launch loops are rejected when saving. Companion applications are left running when the trigger closes.

Changes take effect on **Save changes**. Closing to tray keeps unsaved edits in the window but the monitor continues using the last saved settings. Pause is temporary and is not saved across restarts.

## Game launcher

1. Open **Settings**, select **Use as game launcher**, and **Save changes**.
2. Select **Games** at the top, or **Library** on the floating dock. Choose Steam, EA app, or Battle.net to browse installed games. Use the filter to find a title.
3. Select a game and choose **Favorite** (or double-click its library row). Favorite changes are saved immediately. Click the store's floating tile to unfold its favorites; click a game tile to send its launch request.
4. Use **Lock** and **Always on top** directly on the dock's button row. Both apply and save immediately, and active buttons are highlighted. **Options** manages visible launcher tiles, names, presets, colors and expansion direction. Drag the slim top grip onto any screen to move the unlocked dock; its position is remembered.
5. The drawer's **+** opens the game library. Its arrow cycles the store's own flow direction; the opposite of the dock's main expansion direction is excluded. Drawers wrap on the chosen side, stay inside the monitor's work area, and paginate large favorite collections. Windows' animation preference is respected.

Right-click a toolbar button or store tile for **Hide button row**. This collapses only the controls and keeps the store tiles available. Right-click a tile again for **Show button row**, **Lock position**, **Always on top**, **Library**, or **Layout options**. The visibility preference is remembered. An unlocked compact dock can be dragged by its slim top grip. **Hide launcher** in this menu (or the row's **Hide** button) hides the whole dock.

The unfolded game nodes have transparent gaps: only the nodes and any visible controls cover the desktop. Right-click a game node for **Hide button row** to hide its header, X and page controls, independently for each store. The same menu restores the row and offers Library, flow direction, previous/next page and Close favorites. Clicking the store tile again also closes the group. An empty group keeps a small right-clickable hint.

Choose **Options → select a launcher → Edit launcher** to set separate **Accent** and **Background** colors using a dark shade-square picker. Drag the marker to select saturation/brightness, use the rainbow strip for hue, or type `#RRGGBB`. The presets remain available, and **Save this color** stores up to eight custom swatches; once full, the oldest slot is replaced. The background applies to that launcher and its game nodes; Favorites/Play text follows the accent. Artwork is centered and enlarged to fill the rounded node, cropping at its edges, with shadowed labels over it. Choose **Use launcher**, then **Save layout**; Saved swatches are shared between launcher color pickers and are written with **Use color → Use launcher → Save layout**. Cancel leaves saved settings unchanged.

To remove unwanted launcher tiles from the dock, right-click the tile and choose **Hide this launcher**. To restore it, open **Options**, select the launcher, use **Show / hide**, then **Save layout**. The dock width follows the visible tiles, even with the buttons showing. With one tile, the buttons stack above it; with more tiles, they wrap as needed. Hiding or showing the buttons keeps the tile position when the monitor has enough space. Hidden launchers keep all games and favorites. This only controls the visible tiles; it does not start or stop store clients. If every tile is hidden, an **Add launchers** button keeps setup accessible.

Select **Other → Edit launcher** to rename it, choose its launcher EXE and set its colors. **Add launcher** creates another independent tile; up to ten launcher tiles can be configured. Presets include **Custom EXE**, **Epic Games** and **GOG Galaxy**. Presets can find an installed launcher; Browse lets you set the EXE yourself. Tiles wrap to fit the monitor. These tiles hold individual game shortcuts; clicking a tile unfolds its games and does not launch them together.

In **Library**, choose the launcher from the selector, then **Add manually**:

- **Custom EXE:** choose a game/launcher EXE and optional arguments.
- **Epic Games:** enter its game ID, `namespace:item:artifact`, or full `com.epicgames.launcher://apps/…?action=launch&silent=true` link. Windows sends the link to the installed Epic launcher. See [Epic's protocol documentation](https://dev.epicgames.com/docs/epic-games-store/protocol-activation).
- **GOG Galaxy:** choose `GalaxyClient.exe`, enter the numeric game ID, and optionally provide its game folder. The preset creates `/command=runGame /gameId=… /path="…"`.

The optional game folder also helps locate its own icon; **From game EXE** fills that folder. Epic and GOG presets are manual setup helpers; this version does not automatically import their libraries. Changing a launcher's name or preset preserves existing saved game commands. New entries use the selected preset. Hidden launchers remain accessible in Library.

Store tiles use large centered translucent logos obtained from the installed launcher EXEs at 256 pixels, scaled for the display using Windows imaging. The images are cached; no icon downloads, web requests, or extra runtime are needed. Missing artwork leaves the store name and controls usable. Game icons in the library and favorite nodes come from local installation icon records or the game EXE. They are loaded only when displayed; a cache holds at most 32 game icons at 128 pixels. A missing or stale record checks the game folder's first 128 top-level entries, excluding known utility and anti-cheat EXEs. If needed, it checks fixed Unreal shipping-binary folders beneath up to 32 project folders, with a shared limit of 256 entries. This supplies the game artwork for titles such as Gray Zone Warfare without using the anti-cheat bootstrap icon. No icon downloads, folder watchers or recurring scans are used.

The dock and favorite nodes are windows within the same Multi-Launch process and share its single system-tray icon. They do not create separate tray icons or background processes.

Steam discovery reads `steamapps/libraryfolders.vdf` and installed app manifests across all registered local libraries, including the current nested library format. It obtains the AppID automatically and launches `steam.exe -applaunch <AppID>`. Steam being open never suppresses a game request. Incomplete or missing installations are excluded.

EA discovery reads installed-program records and `__Installer/installerdata.xml`, then passes an `origin2://game/launch?offerIds=...` request to the installed EA launcher. Battle.net discovery reads installed-program records and `Agent/product.db` and uses known product launch codes. If those records are missing, it also checks local `.build.info` markers plus matching game EXEs under Program Files and common game roots (`Games`, `Blizzard_games`, `Blizzard Games`, and `Battle.net Games`) on fixed drives. This is a bounded one-level check during the existing initial scan or **Refresh**, not a full-drive scan. Files found outside Battle.net's registered installations may need **Locate the game** in Battle.net before its launch command can work; their presence does not confirm that they are updated or ready to play. Unknown Battle.net products can be added manually with their launcher EXE and arguments. No account tokens, credentials, owned-game web APIs, or SteamCMD login are needed. The store client still handles sign-in, licensing, updates, and launching the game.

**Add manually** supports other EXE-based launchers and custom commands. For a manually added Steam game, entering only its numeric AppID in Arguments fills in `-applaunch` automatically. Remove custom entries with **Remove custom**. An unavailable favorite is retained in settings and appears again when the game is detected on a later refresh.

Use **+ Game** in an existing trigger profile to choose a discovered game as an automatic launch target. The launcher EXE and arguments are filled in, with client-process suppression turned off. To use a game as the trigger, continue selecting the actual game process/EXE.

Discovery runs once when needed and again on **Refresh**. There is no repeating game-library scan or background animation timer. **Hide** hides the dock; **Settings → Show launcher** brings it back. Disable the launcher option and save to return to the process-trigger interface alone.

## GitHub updates

The **Check updates** button checks this repository's published releases, including development prereleases. Versions are compared numerically. A newer release offers **Open downloads** to open its GitHub page; nothing is downloaded or installed automatically. Offline and rate-limit failures are shown explicitly. There are no automatic update checks.

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

Game favorites, manual entries, launcher enablement, last dragged-to monitor, position, launcher visibility/names/presets, accent/background colors, saved color swatches, and button-row preferences are stored separately in `launcher.elka`, with `launcher.elka.bak` retained on save. Existing process profiles are not migrated or replaced by enabling the game launcher.

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
