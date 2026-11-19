# Elka Multi-Launch 0.2.0-dev.3 — validation

Validated on the development Windows machine on 19 November 2026. This is an unsigned development prerelease.

## Automated checks

- Release x64 builds with Visual Studio Build Tools 2026 / MSVC v145 and Windows SDK 10.0.26100.0, with warnings treated as errors.
- **107 core and feature checks passed**. The original 44 checks still cover process-trigger settings, limits, cycles, persistence, and rearming. New checks cover launcher settings and favorites, Unicode, malformed input, EA content IDs, Battle.net database fields, Steam multi-library discovery and installation filtering, drawer geometry on negative-coordinate monitors, DPI scaling, pagination, and GitHub release comparisons and response validation.
- The unchanged process engine passed **6 regular-mode integration checks** in 0.2.0-dev.2: real fixture trigger starts a real companion, does not launch repeatedly, rearms after exit, and suppresses an already-running companion. Those integrations were not repeated for this UI/discovery update.
- **1 live GitHub update check passed** using the app's WinHTTP client and release parser before publication.
- Passive WMI implementation was not changed in this version. Its seven elevated integration checks passed in 0.1.0-dev.1; they were not repeated for this version.

## Store artwork and toolbar refinement

- The actual native tile renderer was exercised against the installed Steam, EA and Battle.net client icons, with 256-pixel source artwork rendered at 200% DPI. The exported preview was visually inspected: large translucent logos, readable labels and dark rounded tiles.
- **13 artwork/rendering checks passed**, including all three client icons, PNG output, and stable GDI handle count across 200 repeated draws.
- Launcher settings version 3 saves per-store colors and drawer control-row visibility. Migration checks cover versions 1 and 2, retaining favorites, lock, on-top and compact dock preferences; malformed colors/visibility do not partially replace saved settings.
- The native window region includes individual rounded nodes and visible control rows; a geometry check confirms desktop gaps are outside that region. Hiding the drawer row removes header/footer spacing. The native renderer preview includes Battlefield V and Bodycam with their own local icons and visible gaps.
- The user confirmed the previous dock Lock, Always on top and hide/restore controls worked. The new transparent drawer, per-store control rows and dark color picker have a separate preview available; live interaction acceptance of these new controls is still pending.
- The screen picker was removed. Existing drag-to-monitor position persistence remains in use.

## Local game discovery

- A read-only scan of this machine found **22 Steam titles, 6 EA titles and 4 Battle.net titles** across installed local libraries.
- The generated Steam commands contain the manifest AppIDs and use `-applaunch`. EA requests contain the installation's content IDs and use the installed EA launcher.
- Battle.net had no registered game installations or product database on this machine. A bounded common-folder scan recovered Call of Duty, Diablo IV, StarCraft and StarCraft II from active build markers and matching EXEs. Incomplete leftover folders without a matching game EXE are excluded. Fixture checks cover older manifests with empty Product fields, active/inactive entries, duplicates and correct launch codes. Actual Battle.net launches remain untested; its client may need Locate the game for these unregistered files.
- No game installation, store account, store setting, or Windows startup entry was changed by these checks. Full real-game launches, sign-in flows, and anti-cheat behavior remain user acceptance items.

## Interface and layout

- The game launcher is native Win32/GDI and dark-only. It has store tiles, a filtered library/favorites dialog, game tiles, store colors and direction settings, position locking, and a manual GitHub update button.
- Layout checks exercise all 16 primary/secondary direction combinations, pagination, a monitor to the left of the primary screen, and a 150% DPI edge case.
- The existing floating launcher received user feedback and its requested refinements are implemented. The newest transparency, drawer context menus, color picker and game icons still need a live walkthrough; automated renderer checks do not substitute for it.
- The existing dark profile interface and dark installer were visually checked in 0.1.0-dev.1. That does not constitute visual acceptance of the new game UI.

## Resource behavior

- Installed-game discovery is a bounded background job started on demand. There is no recurring library scan or network polling.
- Drawer animation runs only while opening. Update requests happen only when the user clicks Check updates.
- With the dock visible after discovering 32 games, no active process profiles, and no open drawer/library, a 37.9-second sample added **0.000 seconds of measured CPU time**. Working set was **16.69 MiB**, private bytes **3.60 MiB**, and handles remained at **184**. This is one idle sample, not a worst-case bound or a measurement with all game icons loaded.
- Game artwork is loaded on first display and kept in a 32-entry least-recently-used cache, with 128-pixel source icons. Missing icons are also cached. No new timers, services, background processes or network requests were added for artwork or transparency.
- Exactly one Multi-Launch process was running during the preview sample. Floating dock and drawer windows do not register tray icons; the main window owns the existing single tray icon.
- The unsigned x64 EXE is **706,560 bytes (690 KiB)**, up 49,664 bytes from dev.2. The native runtime remains statically linked.

## Packages and remaining acceptance

- Standalone EXE with statically linked C++ runtime, portable ZIP, dark per-user installer, and SHA-256 checksums.
- Game launcher data is kept in a separate versioned file; existing trigger settings retain their original format. Uninstall preserves settings/history.
- Installer installation/uninstallation and actual elevated startup after sign-in have not been exercised in this version.
- Store launch commands may depend on client versions and a signed-in account. Unknown Battle.net product codes require manual setup.
- Games started from an elevated Multi-Launch may inherit its elevation through the store client. The existing administrator-mode notice remains in Settings.