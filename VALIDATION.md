# Elka Multi-Launch 0.1.0-dev.1 — validation

Validated on the development Windows machine on 17 November 2026. This is a development prerelease.

## Automated checks

- Release x64 builds with Visual Studio Build Tools 2026 / MSVC v145 and Windows SDK 10.0.26100.0. Compiler warnings are treated as errors.
- **44 core checks passed**: settings round trips, Unicode and escaped arguments, CRLF support, malformed input, legacy migration, administrator/passive dependency, ten-profile and ten-target limits, atomic persistence and backup, full-path matching, duplicate suppression, concurrent trigger instances, restart/rearming, PID reuse, enabled/disabled state, and launch-loop rejection.
- **6 regular-mode integration checks passed** using locally built fixture processes.
- **7 administrator passive-mode integration checks passed** using the same kind of isolated fixture processes.
- The integration checks actually start a trigger, observe a companion launch, confirm no repeated launch while the trigger remains running, restart the trigger, and confirm an already-running companion is skipped.
- Passive testing exposed a stop-notification filtering problem on this Windows installation. The final implementation uses an unfiltered stop subscription, discards unrelated PIDs inside the sink, and verifies relevant process identities when events arrive. Both integration modes passed after this correction and the process-enumeration optimization.

## Interface checks

- Dark empty state, profile editor, process picker, EXE picker, target editor, Settings, and History were inspected.
- The process picker displayed protected/system processes as well as ordinary applications; filtering by name was exercised.
- EXE folder navigation, selecting an EXE from the list, and entering a full EXE path were exercised.
- A profile was created through the interface, given a target with `--helper` arguments, saved, and used to launch a harmless local fixture. The saved file and visible History confirmed the correct target and arguments.
- Selecting passive mode also selects administrator mode. Turning administrator mode off clears passive mode.
- White corners around rounded buttons were removed. The profile target-count repaint was corrected after the final walkthrough.
- The installer was built with Inno Setup 6.6.1 and forced dark styling. Its destination, Start Menu, and task-selection pages were visually checked. Separate administrator and Windows-startup choices were visible and unchecked by default. The preview was closed before installation.

## Resource measurement

A 20-second regular-mode sample with **ten enabled profiles and 100 target entries**, after confirming that monitoring was active:

| Metric | Observed |
| --- | ---: |
| CPU time used over 20 seconds | 250 ms |
| Working set | 13.19 MiB |
| Private memory | 2.18 MiB |
| Open handles at start / end | 169 / 169 |

250 ms over 20 seconds is approximately **1.25% of one logical CPU**. It is not 1.25% of the entire computer's CPU capacity. This is a short local sample, not a long-term stability claim. Passive mode also uses the shared Windows WMI service; that service's resource use is not included in these app-process figures.

## Package contents

- Standalone `Elka.MultiLaunch.exe`, with its C++ runtime linked statically.
- Portable ZIP containing the EXE, README, and this validation record.
- Dark per-user installer with separate administrator and Windows-startup choices.
- Settings and history are outside the installation folder and are preserved by uninstall.

## Acceptance limits

- No actual game, anti-cheat-protected trigger, or third-party companion combination has been tested. Windows can withhold executable paths from protected processes.
- Installer installation/uninstallation and a real sign-out/sign-in cycle have not been executed on the development machine. Scheduled-task registration and cleanup are implemented and reviewed, but real sign-in acceptance remains to be checked after installation.
- Windows startup settings and existing scheduled tasks were not changed by the validation work.
- The tray menu is implemented using dark owner-drawn items. A complete user-driven tray reopen/exit test remains appropriate during first use.
- A very short-lived process can be missed by regular-mode sampling or can exit before passive-mode path verification.
- Release files are not code-signed in this preview.
