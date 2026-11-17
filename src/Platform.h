#pragma once
#include "Core.h"
#include <windows.h>
#include <functional>
#include <memory>

namespace elka {
std::wstring ErrorText(DWORD code);
std::wstring ExePath();
std::wstring DataDirectory();
bool IsAdmin();
bool IsExe(const std::wstring& path);
bool LoadSettings(const std::wstring& dir, Settings& settings, std::wstring& error);
bool SaveSettings(const std::wstring& dir, const Settings& settings, std::wstring& error);
bool StartupEnabled();
bool SetStartup(bool enabled, bool runAsAdmin, std::wstring& error);
std::vector<Process> Processes(bool allPaths, const Settings* filter = nullptr, bool* success = nullptr);
Process InspectProcess(DWORD pid);
bool Launch(const Target& target, DWORD& pid, std::wstring& error);
using Log = std::function<void(const std::wstring&)>;
class Monitor {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Monitor(Log log, std::function<void(const std::wstring&)> status);
    ~Monitor();
    void Configure(const Settings& settings, bool paused);
    void Stop();
};
}
