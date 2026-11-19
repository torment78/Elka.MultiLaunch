#include "Core.h"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace elka {
std::wstring Key(std::wstring s) {
    std::replace(s.begin(), s.end(), L'/', L'\\');
    if (s.rfind(L"\\\\?\\UNC\\", 0) == 0) s = L"\\\\" + s.substr(8);
    else if (s.rfind(L"\\\\?\\", 0) == 0) s.erase(0, 4);
    if (!s.empty()) CharLowerBuffW(s.data(), static_cast<DWORD>(s.size()));
    return s;
}
std::wstring FileName(const std::wstring& s) { auto p = s.find_last_of(L"\\/"); return p == s.npos ? s : s.substr(p + 1); }
std::wstring Parent(const std::wstring& s) { auto p = s.find_last_of(L"\\/"); return p == s.npos ? L"" : s.substr(0, p + 1); }
std::wstring NewId() { GUID g{}; CoCreateGuid(&g); wchar_t b[40]{}; StringFromGUID2(g,b,40); return b; }
std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Invalid Unicode");
    std::string r(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n, nullptr, nullptr); return r;
}
std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring r(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n); return r;
}
static std::string Escape(const std::wstring& w) {
    std::string r;
    for (char c : ToUtf8(w)) switch(c) {
    case '\\': r += "\\\\"; break; case '\t': r += "\\t"; break;
    case '\n': r += "\\n"; break; case '\r': r += "\\r"; break;
    default: r += c;
    }
    return r;
}
static std::wstring Unescape(const std::string& s) {
    std::string r;
    for (size_t i=0; i<s.size(); ++i) {
        if (s[i] != '\\') { if (!s[i]) throw std::runtime_error("NUL"); r += s[i]; continue; }
        if (++i == s.size()) throw std::runtime_error("Escape");
        switch(s[i]) { case '\\': r += '\\'; break; case 't': r += '\t'; break; case 'n': r += '\n'; break; case 'r': r += '\r'; break; default: throw std::runtime_error("Escape"); }
    }
    return FromUtf8(r);
}
std::string Serialize(const Settings& s) {
    std::string r = "ELKA-MULTILAUNCH\t2\nmode\t" + std::string(s.passive ? "passive" : "polling") + "\nadmin\t" + (s.runAsAdmin ? "1" : "0") + "\n";
    for (const auto& p : s.profiles) {
        r += "profile\t" + Escape(p.id) + "\t" + Escape(p.name) + "\t" + Escape(p.trigger) + "\t" + (p.enabled ? "1" : "0") + "\n";
        for (const auto& t : p.targets) r += "target\t" + Escape(t.path) + "\t" + Escape(t.arguments) + "\t" + Escape(t.directory) + "\t" + (t.skipRunning ? "1" : "0") + "\n";
    }
    return r;
}
bool IsEpicLaunchUri(const std::wstring& value){
    const std::wstring prefix=L"com.epicgames.launcher://apps/",suffix=L"?action=launch&silent=true";
    if(value.size()>4096||value.size()<=prefix.size()+suffix.size()||value.compare(0,prefix.size(),prefix)!=0||value.compare(value.size()-suffix.size(),suffix.size(),suffix)!=0)return false;
    auto id=value.substr(prefix.size(),value.size()-prefix.size()-suffix.size());
    for(size_t i=0;i<id.size();++i){auto c=id[i];if(c==L'%'){if(i+2>=id.size()||!iswxdigit(id[i+1])||!iswxdigit(id[i+2]))return false;i+=2;}else if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'-'||c==L'_'||c==L'.'||c==L':'))return false;}
    return true;
}
bool IsUbisoftLaunchUri(const std::wstring& value){
    const std::wstring prefix=L"uplay://launch/";if(value.compare(0,prefix.size(),prefix)!=0)return false;auto id=value.substr(prefix.size());
    if(id.size()>2&&id.compare(id.size()-2,2,L"/0")==0)id.resize(id.size()-2);
    return !id.empty()&&id.size()<=20&&id.find_first_not_of(L"0123456789")==id.npos;
}
bool IsGameLaunchUri(const std::wstring& value){return IsEpicLaunchUri(value)||IsUbisoftLaunchUri(value);}
std::wstring Validate(const Settings& s) {
    if(s.passive&&!s.runAsAdmin)return L"Passive mode requires Run as administrator.";
    if (s.profiles.size() > MaxProfiles) return L"You can save up to ten profiles.";
    std::set<std::wstring> ids;
    for (const auto& p : s.profiles) {
        if (p.id.empty() || !ids.insert(p.id).second) return L"Invalid profile identifier.";
        if (p.name.empty() || p.name.size()>120) return L"Give each profile a name of 1 to 120 characters.";
        if (p.enabled && (p.trigger.empty() || p.targets.empty())) return L"Choose a trigger and at least one target for '" + p.name + L"', or disable the profile.";
        if (p.targets.size() > MaxTargets) return L"A profile can launch up to ten applications.";
        for (const auto& t : p.targets) {
            if (t.path.empty()) return L"A target application is missing.";
            if (!p.trigger.empty() && Key(t.path) == Key(p.trigger)) return L"A profile cannot launch its own trigger.";
            if (t.arguments.size()>16000 || t.path.size()>32000 || t.directory.size()>32000) return L"A target value is too long.";
        }
    }
    // Reject cycles so a chain cannot recursively launch itself.
    std::map<std::wstring, std::vector<std::wstring>> graph;
    for (const auto& p : s.profiles) if (p.enabled) for (const auto& t : p.targets) graph[Key(p.trigger)].push_back(Key(t.path));
    std::set<std::wstring> visiting, visited;
    auto visit = [&](auto&& self, const std::wstring& path) -> bool {
        if (visiting.count(path)) return false;
        if (visited.count(path)) return true;
        visiting.insert(path);
        auto it = graph.find(path);
        if (it != graph.end()) for (const auto& next : it->second) if (!self(self,next)) return false;
        visiting.erase(path); visited.insert(path); return true;
    };
    for (const auto& [path, _] : graph) if (!visit(visit,path)) return L"These profiles create a launch loop. Remove one of the links before saving.";
    return {};
}
bool Deserialize(const std::string& text, Settings& out, std::wstring& error) {
    error.clear();
    try {
        if (text.size()>1024*1024) throw std::runtime_error("Size");
        std::istringstream input(text); std::string line;
        auto nextLine=[&]{if(!std::getline(input,line))return false;if(!line.empty()&&line.back()=='\r')line.pop_back();return true;};
        if (!nextLine() || (line != "ELKA-MULTILAUNCH\t1" && line != "ELKA-MULTILAUNCH\t2")) throw std::runtime_error("Version");
        const bool version2=line=="ELKA-MULTILAUNCH\t2";
        if (!nextLine() || (line != "mode\tpassive" && line != "mode\tpolling")) throw std::runtime_error("Mode");
        Settings s; s.passive = line == "mode\tpassive";
        s.runAsAdmin=s.passive;
        if(version2){if(!nextLine()||(line!="admin\t0"&&line!="admin\t1"))throw std::runtime_error("Admin");s.runAsAdmin=line=="admin\t1";}
        while (nextLine()) {
            if (line.empty()) continue;
            std::vector<std::string> f; size_t pos=0;
            for (;;) { auto n=line.find('\t',pos); f.push_back(line.substr(pos,n-pos)); if(n==line.npos) break; pos=n+1; }
            if (f.size()!=5 || (f[4]!="0" && f[4]!="1")) throw std::runtime_error("Record");
            if (f[0]=="profile") {
                if (s.profiles.size()==MaxProfiles) throw std::runtime_error("Profiles");
                s.profiles.push_back({Unescape(f[1]),Unescape(f[2]),Unescape(f[3]),f[4]=="1",{}});
            } else if (f[0]=="target" && !s.profiles.empty() && s.profiles.back().targets.size()<MaxTargets) {
                s.profiles.back().targets.push_back({Unescape(f[1]),Unescape(f[2]),Unescape(f[3]),f[4]=="1"});
            } else throw std::runtime_error("Record");
        }
        error=Validate(s); if (!error.empty()) return false;
        out=std::move(s); return true;
    } catch (...) { error=L"Settings could not be read. The original file has been kept; check settings.elka and settings.elka.bak."; return false; }
}
void Rules::Configure(const Settings& settings) {
    for(auto it=states_.begin();it!=states_.end();) {
        auto p=std::find_if(settings.profiles.begin(),settings.profiles.end(),[&](const auto& item){return item.id==it->first;});
        if(p==settings.profiles.end() || !p->enabled || Key(p->trigger)!=it->second.trigger) it=states_.erase(it); else ++it;
    }
}
std::vector<size_t> Rules::Evaluate(const Settings& s, const std::vector<Process>& processes, bool paused) {
    std::vector<size_t> fire;
    std::set<std::wstring> keep;
    for (size_t i=0; i<s.profiles.size(); ++i) {
        const auto& p=s.profiles[i]; keep.insert(p.id);
        auto& previous=states_[p.id]; const auto trigger=Key(p.trigger);
        std::set<std::pair<uint32_t,uint64_t>> now;
        if (p.enabled) for (const auto& proc : processes) if (!proc.path.empty() && Key(proc.path)==trigger) now.insert({proc.pid,proc.born});
        bool overlap=false;
        if (previous.enabled && previous.trigger==trigger) for(const auto& id:now) if(previous.ids.count(id)) {overlap=true;break;}
        if (!paused && !now.empty() && !overlap) fire.push_back(i);
        previous={trigger,p.enabled,std::move(now)};
    }
    for(auto it=states_.begin();it!=states_.end();) if(!keep.count(it->first)) it=states_.erase(it); else ++it;
    return fire;
}
}
