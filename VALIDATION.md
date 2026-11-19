# Elka Multi-Launch 0.2.0-dev.4 — validation

Validated on the development Windows machine on 19 November 2026. This is an unsigned development prerelease.

## Automated checks

- Release x64 builds with MSVC v145 and Windows SDK 10.0.26100.0, with warnings treated as errors.
- **147 core and feature checks passed**, covering the original trigger rules/settings, discovery, update parsing, launcher visibility/names/presets, ten launcher slots, stable favorite identities, custom backgrounds, saved swatches, HSV color conversion, narrow/wrapped dock layout, and launcher-format migrations.
- **6 regular-mode integration checks passed** with real local trigger/companion fixtures. Existing EXE launches, rearming and already-running suppression still work after adding narrow Epic launch-link support.
- **14 native rendering checks passed**, including store/game icons, stable GDI handle count during 200 draws, PNG output and the shared shade-square/hue-strip renderer. The exported image was inspected: artwork fills the rounded tiles, labels remain visible, backgrounds differ, and the shade square runs from white/selected hue to black.
- Passive WMI event monitoring was not changed. Its elevated integration checks were not repeated in this version.

## Launcher controls and settings

- Individual launcher tiles can be hidden immediately through their context menu or configured in Options. Hidden tiles keep games and favorites. Renaming Other retains the same game/favorite identities.
- Up to ten launcher tiles can be configured, including manual Custom EXE, Epic Games and GOG Galaxy presets. A visible fallback setup button keeps the dock reachable if every launcher tile is hidden.
- Tests cover dock widths with zero through ten visible launchers. Button rectangles remain inside the dock, toolbar visibility does not change its width, and a single launcher uses stacked buttons. Drag position persistence remains in use; toolbar toggling compensates its height to preserve tile position, subject to work-area clamping.
- Launcher file version 5 stores the shared eight-color palette. Versions 1–3 and the intermediate version 4 preview migrate without losing colors, hidden rows, launcher names, manual games or favorites. Invalid values do not partially replace the saved state.
- The color picker uses a cached native bitmap for the shade square and hue strip. Saturation/brightness changes move the marker without regenerating the bitmap; changing hue or DPI regenerates it. Closing the picker frees the bitmap.
- The user exercised the intermediate hide/restore and larger-artwork preview and requested the final narrower dock and shade picker. The final stacked toolbar, drag marker and saved-color interactions still need a live walkthrough. Renderer and layout checks do not substitute for that acceptance.

## Manual launcher presets

- Epic presets produce the documented `com.epicgames.launcher://apps/…?action=launch&silent=true` activation link. Windows dispatches it through its registered handler; no shell command is constructed. Unrelated URL schemes/actions, malformed escaping and extra launch arguments are rejected. The existing trigger-profile file format can preserve a generated Epic target.
- GOG presets construct GalaxyClient arguments from a numeric game ID and optional game folder. Windows argument-parser checks cover paths with spaces and trailing backslashes. Custom EXE presets retain manual EXE/argument setup.
- Presets are manual helpers. No Epic or GOG automatic library import or account access was added. Their actual client sign-in, installation and game launches have not been verified in this version.

## Discovery and runtime scope

- The current development cycle detected 22 Steam, 6 EA and 4 Battle.net titles. The Blizzard recovery scan uses local active build markers and matching EXEs in bounded common folders; recovered files may still need Locate the game in Battle.net before launching.
- Game artwork is loaded only when displayed and retained in a 32-entry cache with 128-pixel source icons. Visible store artwork uses 256-pixel source icons. No icon downloads, new recurring scans, timers, services or background processes were added.
- Dock and game nodes share the app's existing single tray icon. An isolated test copy is a separate app instance; it is not started alongside another launcher copy during preview updates.
- The final x64 EXE is 745,984 bytes (728.5 KiB). A new idle benchmark of the final picker build has not been completed; earlier-version memory measurements are not claimed for this build.
- No Windows startup configuration, installed-app profiles, store installation or account settings were changed by validation. The isolated preview retains the user's chosen administrator/passive settings.

## Packages and remaining acceptance

- Standalone EXE, portable ZIP, dark per-user installer and SHA-256 checksums. C++ runtime is linked statically.
- Live update lookup and published-package download/hash verification are performed during release publication.
- Actual Epic/GOG game launches, the final GUI interaction walkthrough, installer installation/uninstallation and elevated sign-in startup remain unverified in this version.
- Games started from an elevated Multi-Launch may inherit elevation through a store client. The existing administrator-mode notice remains in Settings.
