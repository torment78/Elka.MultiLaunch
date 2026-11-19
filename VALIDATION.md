# Elka Multi-Launch 0.2.0-dev.8 — validation

Validated on the development Windows machine on 19 November 2026. This is an unsigned development prerelease.

## Automated checks

- Release x64 builds with MSVC v145 and Windows SDK 10.0.26100.0, with warnings treated as errors.
- **324 core, feature and rendering-regression checks passed**, including 43 new installed-game discovery/parser checks, the original trigger rules/settings, launcher presets/layout, icon fixes and drawer-animation checks.
- **7 native discovery lifecycle checks passed** on an inactive test desktop. The configured library scans once on initial load, remains idle, refreshes once through the actual Library button, then remains idle again. No desktop switch, user input or vendor game launch was used.
- **8 native dark folder-picker checks passed** on an inactive test desktop, covering a quoted Unicode folder, Cancel, and an invalid folder staying open without accepting the previous location. No desktop switch or user input was used.
- Animation checks cover all twelve supported Open/Build combinations with controls shown/hidden at 100% and 200% scaling, a negative-coordinate monitor, outward-first reveal, subsequent node order/direction, monotonic growth inside fixed artwork bounds, exact final placement, wrapping/pagination, transparent gaps and a maximum 1.5-second duration. The exported five-stage native preview was inspected for Open Up + Build Left.
- **7 native drawer lifecycle checks passed** on an inactive test desktop. The real drawer survives its initial show/layout messages, unfolds progressively, stops timer wakeups after finishing, cancels when closed midway, and becomes idle after rapid close/reopen. The test did not switch desktops, take focus or launch a game.
- The translated-drawing regression failed against the previous clipping code and passes after the fix. Pixel comparisons cover 100%, 150% and 200% display scaling, gray/blue/red backgrounds, viewport/window origin translation, preservation of an existing clip, and repeated drawing with stable GDI resource counts after initialization. A hidden native owner-drawn button also verifies the Windows animation-capture message path, including custom client-area painting.
- Icon-lookup fixtures cover anti-cheat bootstrap rejection, Unreal shipping executables, engine-folder exclusion, stale registry paths, registered artwork priority and missing game folders.
- **15 native artwork-preview checks passed**, including installed store/game icons, stable GDI handle count during 200 draws, PNG output and the shared shade-square/hue-strip renderer. The exported image was inspected and includes the correct Gray Zone Warfare artwork from its shipping executable alongside Battlefield V and Bodycam.
- **6 regular-mode integration checks passed** with local fixture executables after extending the launch-target dispatch to Ubisoft links. No vendor client or game was started.
- Passive WMI event monitoring was not changed. Its elevated integration checks were not repeated in this version.

## Launcher controls and settings

- Individual launcher tiles can be hidden immediately through their context menu or configured in Options. Hidden tiles keep games and favorites. Renaming Other retains the same game/favorite identities.
- Up to ten launcher tiles can be configured, including Custom EXE, Epic Games, GOG Galaxy, Ubisoft Connect and Rockstar Games presets. Epic, Ubisoft and Rockstar now discover installed games; Custom EXE and GOG retain manual entries. Templates can be saved without a client EXE. A visible fallback setup button keeps the dock reachable if every launcher tile is hidden.
- Tests cover dock widths with zero through ten visible launchers. Button rectangles remain inside the dock, toolbar visibility does not change its width, and a single launcher uses stacked buttons. Drag position persistence remains in use; toolbar toggling compensates its height to preserve tile position, subject to work-area clamping.
- Launcher file version 6 is unchanged. Versions 1–5 migrate without losing colors, saved swatches, hidden rows, launcher names, game commands or favorites. Stable discovered identifiers preserve favorites across refreshes and moved Epic installations. Existing manual entries remain unchanged.
- The color picker uses a cached native bitmap for the shade square and hue strip. Saturation/brightness changes move the marker without regenerating the bitmap; changing hue or DPI regenerates it. Closing the picker frees the bitmap.
- The prior icon-clipping fix and sequential node reveals are retained. Options exposes Open (global) and Build (selected launcher); the settings upgrade preserves existing saved directions. The live animation feel and direction-control walkthrough still need user acceptance.

## Manual launcher presets

- Epic presets produce the documented `com.epicgames.launcher://apps/…?action=launch&silent=true` activation link from an ID or an absolute game folder. Folder encoding checks cover Unicode, spaces and reserved characters. Windows dispatches the link through its registered handler; no shell command is constructed.
- Ubisoft presets accept numeric IDs and `uplay://launch/ID` links, including a trailing `/0`. Other actions, queries, malformed IDs, unrelated URL schemes and extra arguments/working folders are rejected. Epic and Ubisoft missing-handler cases return an install/open-client message before dispatch.
- Rockstar presets use the installed client with `-launchTitleInFolder` and exactly one quoted game-folder argument. Windows argument-parser checks cover Unicode, spaces and a trailing backslash. Missing/relative/invalid folders and unrelated extra commands are rejected.
- GOG presets construct GalaxyClient arguments from a numeric game ID and optional game folder. Windows argument-parser checks cover paths with spaces and trailing backslashes. Custom EXE presets retain manual EXE/argument setup.
- Automatic discovery uses Epic manifests/installed-list paths, Ubisoft installation registry entries and Rockstar uninstall-registration metadata. Uninstall commands are only inspected to identify Rockstar titles; they are never executed. Game launches still use the existing protocol/folder launch methods.
- Fixtures cover Unicode names, complete Epic launch identities, duplicate manifests, incomplete downloads, add-ons/plugins, moved or removed folders, executable path traversal, missing executables, invalid/oversized records, Ubisoft name/icon enrichment, Rockstar title filtering and safe launch construction, multiple/hidden launcher slots, stable favorites and unchanged manual entries. The existing small JSON reader is shared with the updater and rejects duplicate keys and malformed numbers/Unicode.
- Configured providers are scanned once per refresh, even if multiple tiles use the same preset. A settings snapshot is passed to the existing scan thread; a preset change while scanning queues one replacement scan and discards the stale result. Rename, color and visibility changes do not rescan. Unconfigured extra providers are not read. No account access, new recurring tasks or background workers were added.

## Discovery and runtime scope

- A read-only scan with all three new providers configured detected 22 Steam, 6 EA and 4 Battle.net titles, and zero Epic/Ubisoft/Rockstar installations. That complete one-time scan took 88 ms on this machine; this is an observation, not a performance guarantee for other libraries. Real installed-game import and game launching for the new stores remain unverified because those clients/games were not found locally. Fixture coverage is recorded separately above.
- Epic reads at most 4,096 directory entries, 2 MB per JSON file and 32 MB total, one file at a time. Ubisoft reads at most 4,096 installation records and uses the existing installed-program enumeration for names/icons. Rockstar reuses that enumeration and checks known title IDs. No recursive searches or icon downloads were added. Temporary metadata is released after scanning; the existing bounded artwork cache is retained.
- Game artwork is loaded only when displayed and retained in a 32-entry cache with 128-pixel source icons. Visible store artwork uses 256-pixel source icons. A 16-ms UI timer runs only during the bounded unfolding animation and is cancelled on completion, closing, replacement or destruction. Cached artwork is not resized on each frame. No icon downloads, new recurring scans, idle animation timers, services or background processes were added.
- Dock and game nodes share the app's existing single tray icon. An isolated test copy is a separate app instance; preview replacement waits for existing copies to exit.
- Native lifecycle checks confirm no animation timer wakeups after completion or cancellation. A full idle CPU/memory benchmark has not been completed for this version; earlier-version measurements are not claimed for this build.
- No Windows startup configuration, installed-app profiles, store installation or account settings were changed by validation. Isolated fixtures use their own data directories; the user's running app is left open.

## Packages and remaining acceptance

- Standalone EXE, portable ZIP, dark per-user installer and SHA-256 checksums. C++ runtime is linked statically.
- Live update lookup and published-package download/hash verification are performed during release publication.
- Actual Epic/Ubisoft/Rockstar installed-game import, Epic/GOG/Ubisoft/Rockstar game launches, installer installation/uninstallation and elevated sign-in startup remain unverified in this version.
- Games started from an elevated Multi-Launch may inherit elevation through a store client. The existing administrator-mode notice remains in Settings.
