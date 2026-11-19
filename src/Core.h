#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstdint>

namespace elka {
constexpr size_t MaxProfiles = 10, MaxTargets = 10;
struct Target {
    std::wstring path, arguments, directory;
    bool skipRunning = true;
};
struct Profile {
    std::wstring id, name, trigger;
    bool enabled = true;
    std::vector<Target> targets;
};
struct Settings {
    bool passive = false;
    std::vector<Profile> profiles;
    bool runAsAdmin = false;
};
struct Process {
    uint32_t pid = 0, session = 0;
    uint64_t born = 0;
    std::wstring name, path;
};
std::wstring Key(std::wstring value);
std::wstring FileName(const std::wstring& path);
std::wstring Parent(const std::wstring& path);
std::wstring NewId();
std::string ToUtf8(const std::wstring& value);
std::wstring FromUtf8(const std::string& value);
std::string Serialize(const Settings& settings);
bool Deserialize(const std::string& text, Settings& settings, std::wstring& error);
std::wstring Validate(const Settings& settings);
bool IsEpicLaunchUri(const std::wstring& value);
bool IsUbisoftLaunchUri(const std::wstring& value);
bool IsGameLaunchUri(const std::wstring& value);

// A profile fires once per continuous running session. Keep state across edits
// and pause/resume; a new trigger or a disable/enable explicitly rearms it.
class Rules {
    struct State { std::wstring trigger; bool enabled = false; std::set<std::pair<uint32_t,uint64_t>> ids; };
    std::map<std::wstring, State> states_;
public:
    void Configure(const Settings& settings);
    std::vector<size_t> Evaluate(const Settings& settings, const std::vector<Process>& processes, bool paused);
};
}
