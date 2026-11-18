# Elka Multi-Launch 0.2.0-dev.1 — validation

Validated on the development Windows machine on 18 November 2026. This is an unsigned development prerelease.

## Automated checks

- Release x64 builds with Visual Studio Build Tools 2026 / MSVC v145 and Windows SDK 10.0.26100.0, with warnings treated as errors.
- **91 core and feature checks passed**. The original 44 checks still cover process-trigger settings, limits, cycles, persistence, and rearming. New checks cover launcher settings and favorites, Unicode, malformed input, EA content IDs, Battle.net database fields, Steam multi-library discovery and installation filtering, drawer geometry on negative-coordinate monitors, DPI scaling, pagination, and GitHub release comparisons and response validation.
- **6 regular-mode integration checks passed**: real fixture trigger starts a real companion, does not launch repeatedly, rearms after exit, and suppresses an already-running companion.
- **1 live GitHub update check passed** using the app's WinHTTP client and release parser.
- Passive WMI implementation was not changed in this version. Its seven elevated integration checks passed in 0.1.0-dev.1; they were not repeated for this version.

## Local game discovery

- A read-only scan of this machine found **22 Steam titles and 6 EA titles** across installed local libraries.
- The generated Steam commands contain the manifest AppIDs and use `-applaunch`. EA requests contain the installation's content IDs and use the installed EA launcher.
- The local Battle.net client was detected; no installed Battle.net game was available for a live discovery/launch test. Its database parsing was checked with fixtures.
- No game installation, store account, store setting, or Windows startup entry was changed by these checks. Full real-game launches, sign-in flows, and anti-cheat behavior remain user acceptance items.

## Interface and layout

- The game launcher is native Win32/GDI and dark-only. It has store tiles, a filtered library/favorites dialog, game tiles, monitor/direction settings, position locking, and a manual GitHub update button.
- Layout checks exercise all 16 primary/secondary direction combinations, pagination, a monitor to the left of the primary screen, and a 150% DPI edge case.
- Visual acceptance of the new launcher is pending. Real multi-monitor dragging, keyboard navigation, animation appearance, favorites interaction, and the update dialog still need a walkthrough.
- The existing dark profile interface and dark installer were visually checked in 0.1.0-dev.1. That does not constitute visual acceptance of the new game UI.

## Resource behavior

- Installed-game discovery is a bounded background job started on demand. There is no recurring library scan or network polling.
- Drawer animation runs only while opening. Update requests happen only when the user clicks Check updates.
- This version has not yet had a new idle CPU/memory measurement; the 0.1.0 measurement should not be treated as a measurement of this build.

## Packages and remaining acceptance

- Standalone EXE with statically linked C++ runtime, portable ZIP, dark per-user installer, and SHA-256 checksums.
- Game launcher data is kept in a separate versioned file; existing trigger settings retain their original format. Uninstall preserves settings/history.
- Installer installation/uninstallation and actual elevated startup after sign-in have not been exercised in this version.
- Store launch commands may depend on client versions and a signed-in account. Unknown Battle.net product codes require manual setup.
- Games started from an elevated Multi-Launch may inherit its elevation through the store client. The existing administrator-mode notice remains in Settings.