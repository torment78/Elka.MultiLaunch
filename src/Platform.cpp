#include "Platform.h"
#include <tlhelp32.h>
#include <shlobj.h>
#include <shellapi.h>
#include <sddl.h>
#include <taskschd.h>
#include <wbemidl.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace elka {
using Microsoft::WRL::ComPtr;
struct Handle {
    HANDLE value=nullptr;
    explicit Handle(HANDLE v=nullptr):value(v){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
    Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete;
};
struct BStr { BSTR value; explicit BStr(const wchar_t* s):value(SysAllocString(s)){} explicit BStr(const std::wstring& s):BStr(s.c_str()){} ~BStr(){SysFreeString(value);} operator BSTR() const{return value;} };
std::wstring ErrorText(DWORD e) {
    wchar_t* b=nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,e,0,reinterpret_cast<LPWSTR>(&b),0,nullptr);
    std::wstring s=b?b:L"Windows error "+std::to_wstring(e); if(b)LocalFree(b);
    while(!s.empty() && (s.back()==L'\r'||s.back()==L'\n'))s.pop_back(); return s;
}
std::wstring ExePath(){std::wstring s(32768,L'\0');auto n=GetModuleFileNameW(nullptr,s.data(),static_cast<DWORD>(s.size()));s.resize(n);return s;}
std::wstring DataDirectory(){PWSTR p=nullptr; std::wstring r; if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p))){r=p;CoTaskMemFree(p);} return r+L"\\ElkaSoft\\MultiLaunch";}
bool IsAdmin(){BOOL yes=FALSE; SID_IDENTIFIER_AUTHORITY a=SECURITY_NT_AUTHORITY; PSID sid=nullptr; if(AllocateAndInitializeSid(&a,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){CheckTokenMembership(nullptr,sid,&yes);FreeSid(sid);}return yes!=FALSE;}
bool IsExe(const std::wstring& p){auto k=Key(p);auto attr=GetFileAttributesW(p.c_str());return k.size()>4 && k.substr(k.size()-4)==L".exe" && attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY);}
bool LoadSettings(const std::wstring& dir,Settings& s,std::wstring& error){
    const std::filesystem::path p=std::filesystem::path(dir)/L"settings.elka";
    std::error_code ec; if(!std::filesystem::exists(p,ec) && !ec)return true;
    auto size=std::filesystem::file_size(p,ec); if(ec || size>1024*1024){error=L"Cannot read settings.elka, or it is larger than 1 MB.";return false;}
    std::ifstream f(p,std::ios::binary);if(!f){error=L"Cannot open settings.elka.";return false;}
    std::string text(static_cast<size_t>(size),'\0'); f.read(text.data(),static_cast<std::streamsize>(size));
    if(!f && size){error=L"Could not read the complete settings file.";return false;} return Deserialize(text,s,error);
}
bool SaveSettings(const std::wstring& dir,const Settings& s,std::wstring& error){
    error=Validate(s);if(!error.empty())return false;
    std::error_code ec;std::filesystem::create_directories(dir,ec);if(ec){error=L"Cannot create the settings folder.";return false;}
    auto path=dir+L"\\settings.elka", temp=path+L".tmp", backup=path+L".bak";
    auto bytes=Serialize(s);
    { Handle h(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
      if(h.value==INVALID_HANDLE_VALUE){error=ErrorText(GetLastError());return false;}
      DWORD written=0; if(!WriteFile(h.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)||written!=bytes.size()||!FlushFileBuffers(h.value)){error=L"Could not write the settings file.";return false;} }
    if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES && !CopyFileW(path.c_str(),backup.c_str(),FALSE)){error=L"Could not back up the previous settings: "+ErrorText(GetLastError());return false;}
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=ErrorText(GetLastError());return false;}
    return true;
}
static constexpr wchar_t RunKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr wchar_t StartupName[]=L"Elka Multi-Launch";
static std::wstring UserSid();
static std::wstring TaskName(){return L"MultiLaunch-"+UserSid();}
static HRESULT TaskFolder(ComPtr<ITaskFolder>& folder,bool create){
    ComPtr<ITaskService> service;HRESULT hr=CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&service));if(FAILED(hr))return hr;
    VARIANT empty{};hr=service->Connect(empty,empty,empty,empty);if(FAILED(hr))return hr;
    hr=service->GetFolder(BStr(L"\\ElkaSoft"),&folder);
    if(FAILED(hr)&&create){ComPtr<ITaskFolder> root;hr=service->GetFolder(BStr(L"\\"),&root);if(SUCCEEDED(hr))hr=root->CreateFolder(BStr(L"ElkaSoft"),empty,&folder);}
    return hr;
}
bool StartupEnabled(){
    DWORD size=0;if(RegGetValueW(HKEY_CURRENT_USER,RunKey,StartupName,RRF_RT_REG_SZ,nullptr,nullptr,&size)==ERROR_SUCCESS && size>2)return true;
    ComPtr<ITaskFolder> folder;ComPtr<IRegisteredTask> task;VARIANT_BOOL enabled=VARIANT_FALSE;
    return SUCCEEDED(TaskFolder(folder,false)) && SUCCEEDED(folder->GetTask(BStr(TaskName()),&task)) && SUCCEEDED(task->get_Enabled(&enabled)) && enabled!=VARIANT_FALSE;
}
static std::wstring UserSid(){
    HANDLE raw=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw))return {};Handle token(raw);
    DWORD n=0;GetTokenInformation(raw,TokenUser,nullptr,0,&n);std::vector<BYTE>b(n);
    if(!GetTokenInformation(raw,TokenUser,b.data(),n,&n))return {};LPWSTR sid=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid,&sid))return {};std::wstring s=sid;LocalFree(sid);return s;
}
bool SetStartup(bool enabled,bool runAsAdmin,std::wstring& error){
    // Elevated tasks use an interactive logon token: no stored password, service,
    // or SYSTEM process, and launches remain in the user's desktop session.
    auto fail=[&](HRESULT hr){error=L"Could not update Windows startup: "+ErrorText(static_cast<DWORD>(hr));return false;};
    if(enabled&&runAsAdmin){
        if(!IsAdmin()){error=L"Restart as administrator to configure elevated Windows startup.";return false;}
        ComPtr<ITaskService> service;HRESULT hr=CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&service));if(FAILED(hr))return fail(hr);
        VARIANT empty{};if(FAILED(hr=service->Connect(empty,empty,empty,empty)))return fail(hr);
        ComPtr<ITaskFolder> folder;if(FAILED(hr=TaskFolder(folder,true)))return fail(hr);
        ComPtr<ITaskDefinition> task;if(FAILED(hr=service->NewTask(0,&task)))return fail(hr);
        auto sid=UserSid();if(sid.empty())return fail(E_ACCESSDENIED);
        ComPtr<IPrincipal> principal;ComPtr<ITaskSettings> options;ComPtr<ITriggerCollection> triggers;ComPtr<ITrigger> trigger;ComPtr<ILogonTrigger> logon;ComPtr<IActionCollection> actions;ComPtr<IAction> action;ComPtr<IExecAction> exec;
#define CHECK_HR(x) if(FAILED(hr=(x)))return fail(hr)
        CHECK_HR(task->get_Principal(&principal));CHECK_HR(principal->put_UserId(BStr(sid)));CHECK_HR(principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN));CHECK_HR(principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST));
        CHECK_HR(task->get_Settings(&options));CHECK_HR(options->put_DisallowStartIfOnBatteries(VARIANT_FALSE));CHECK_HR(options->put_StopIfGoingOnBatteries(VARIANT_FALSE));CHECK_HR(options->put_ExecutionTimeLimit(BStr(L"PT0S")));CHECK_HR(options->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW));CHECK_HR(options->put_StartWhenAvailable(VARIANT_TRUE));
        CHECK_HR(task->get_Triggers(&triggers));CHECK_HR(triggers->Create(TASK_TRIGGER_LOGON,&trigger));CHECK_HR(trigger.As(&logon));CHECK_HR(logon->put_UserId(BStr(sid)));CHECK_HR(logon->put_Delay(BStr(L"PT3S")));
        CHECK_HR(task->get_Actions(&actions));CHECK_HR(actions->Create(TASK_ACTION_EXEC,&action));CHECK_HR(action.As(&exec));CHECK_HR(exec->put_Path(BStr(ExePath())));CHECK_HR(exec->put_Arguments(BStr(L"--tray")));CHECK_HR(exec->put_WorkingDirectory(BStr(Parent(ExePath()))));
        VARIANT user{};user.vt=VT_BSTR;user.bstrVal=SysAllocString(sid.c_str());ComPtr<IRegisteredTask> registered;
        hr=folder->RegisterTaskDefinition(BStr(TaskName()),task.Get(),TASK_CREATE_OR_UPDATE,user,empty,TASK_LOGON_INTERACTIVE_TOKEN,empty,&registered);VariantClear(&user);if(FAILED(hr))return fail(hr);
#undef CHECK_HR
        LSTATUS result=RegDeleteKeyValueW(HKEY_CURRENT_USER,RunKey,StartupName);
        if(result!=ERROR_SUCCESS&&result!=ERROR_FILE_NOT_FOUND)return fail(HRESULT_FROM_WIN32(result));
        return true;
    }
    ComPtr<ITaskFolder> folder;ComPtr<IRegisteredTask> existing;
    HRESULT hr=TaskFolder(folder,false);
    if(SUCCEEDED(hr) && SUCCEEDED(folder->GetTask(BStr(TaskName()),&existing))){hr=folder->DeleteTask(BStr(TaskName()),0);if(FAILED(hr))return fail(hr);}
    if(enabled){
        auto command=L"\""+ExePath()+L"\" --tray";if(command.size()>259){error=L"Move the application to a shorter path to enable Windows startup.";return false;}
        HKEY key=nullptr;LSTATUS r=RegCreateKeyExW(HKEY_CURRENT_USER,RunKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
        if(r!=ERROR_SUCCESS)return fail(HRESULT_FROM_WIN32(r));r=RegSetValueExW(key,StartupName,0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));RegCloseKey(key);if(r!=ERROR_SUCCESS)return fail(HRESULT_FROM_WIN32(r));
    }else{auto r=RegDeleteKeyValueW(HKEY_CURRENT_USER,RunKey,StartupName);if(r!=ERROR_SUCCESS&&r!=ERROR_FILE_NOT_FOUND)return fail(HRESULT_FROM_WIN32(r));}
    return true;
}
Process InspectProcess(DWORD pid){
    Process p;p.pid=pid;DWORD session=0;ProcessIdToSessionId(pid,&session);p.session=session;Handle h(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if(h.value){std::wstring s(32768,L'\0');DWORD n=static_cast<DWORD>(s.size());if(QueryFullProcessImageNameW(h.value,0,s.data(),&n)){s.resize(n);p.path=s;p.name=FileName(s);}FILETIME c{},e{},k{},u{};if(GetProcessTimes(h.value,&c,&e,&k,&u))p.born=(static_cast<uint64_t>(c.dwHighDateTime)<<32)|c.dwLowDateTime;}return p;
}
std::vector<Process> Processes(bool allPaths,const Settings* filter,bool* success){
    if(success)*success=false;std::vector<Process> result;std::set<std::wstring> names;
    if(filter)for(const auto& p:filter->profiles){if(p.enabled)names.insert(Key(FileName(p.trigger)));for(const auto& t:p.targets)names.insert(Key(FileName(t.path)));}
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));if(snap.value==INVALID_HANDLE_VALUE)return result;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(snap.value,&entry))return result;
    do {
        if(!allPaths&&!names.count(Key(entry.szExeFile)))continue;
        Process p=InspectProcess(entry.th32ProcessID);p.name=entry.szExeFile;
        result.push_back(std::move(p));
    }while(Process32NextW(snap.value,&entry));
    if(success)*success=true;
    if(allPaths)std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return Key(a.name)<Key(b.name);});return result;
}
bool Launch(const Target& t,DWORD& pid,std::wstring& error){
    pid=0;bool protocol=IsGameLaunchUri(t.path);if(protocol&&(!t.arguments.empty()||!t.directory.empty())){error=L"Game launch links do not accept extra arguments or a working folder.";return false;}if(!protocol&&!IsExe(t.path)){error=L"EXE not found: "+t.path;return false;}
    if(protocol){auto scheme=t.path.substr(0,t.path.find(L':'));HKEY key=nullptr;if(RegOpenKeyExW(HKEY_CLASSES_ROOT,scheme.c_str(),0,KEY_READ,&key)!=ERROR_SUCCESS){error=IsUbisoftLaunchUri(t.path)?L"Install and open Ubisoft Connect once, then try this game again.":L"Install and open Epic Games Launcher once, then try this game again.";return false;}RegCloseKey(key);}
    auto dir=protocol?L"":t.directory.empty()?Parent(t.path):t.directory;
    SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;info.lpVerb=L"open";info.lpFile=t.path.c_str();info.lpParameters=t.arguments.empty()?nullptr:t.arguments.c_str();info.lpDirectory=dir.empty()?nullptr:dir.c_str();info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info)){error=ErrorText(GetLastError());return false;}
    if(info.hProcess){pid=GetProcessId(info.hProcess);CloseHandle(info.hProcess);}return true;
}
struct Event {bool start;DWORD pid;uint64_t time;Process process;};
struct Events {
    std::mutex mutex;std::vector<Event> queue;HANDLE wake;std::atomic<HRESULT> failure{S_OK};
    std::set<DWORD> tracked;bool filterReady=false;
    explicit Events(HANDLE h):wake(h){}
};
class Sink final:public IWbemObjectSink {
    std::atomic<ULONG> refs_{1};std::shared_ptr<Events> events_;bool start_;
public:
    Sink(std::shared_ptr<Events> e,bool start):events_(std::move(e)),start_(start){}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{ULONG n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IWbemObjectSink){*out=static_cast<IWbemObjectSink*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE Indicate(LONG count,IWbemClassObject** objects) override{
        bool queued=false;
        for(LONG i=0;i<count;++i){VARIANT pid{},time{};if(FAILED(objects[i]->Get(L"ProcessID",0,&pid,nullptr,nullptr)))continue;objects[i]->Get(L"TIME_CREATED",0,&time,nullptr,nullptr);
            Event event{start_,static_cast<DWORD>(pid.uintVal),0,{}};if(time.vt==VT_BSTR)event.time=_wcstoui64(time.bstrVal,nullptr,10);
            VariantClear(&pid);VariantClear(&time);if(start_)event.process=InspectProcess(event.pid);
            {std::lock_guard lock(events_->mutex);
                if(start_){if(event.process.path.empty())continue;events_->tracked.insert(event.pid);}
                else if(events_->filterReady&&!events_->tracked.count(event.pid))continue;
                if(events_->queue.size()<4096)events_->queue.push_back(std::move(event));else events_->failure=WBEM_E_QUOTA_VIOLATION;
                queued=true;
            }
        }if(queued)SetEvent(events_->wake);return WBEM_S_NO_ERROR;
    }
    HRESULT STDMETHODCALLTYPE SetStatus(LONG flags,HRESULT hr,BSTR,IWbemClassObject*) override{
        if(flags==WBEM_STATUS_COMPLETE&&FAILED(hr)&&hr!=WBEM_E_CALL_CANCELLED){events_->failure=hr;SetEvent(events_->wake);}return S_OK;
    }
};
struct Monitor::Impl {
    Log log;std::function<void(const std::wstring&)> status;Handle wake{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    std::mutex mutex;Settings pending;bool pendingPause=false;uint64_t generation=0;std::atomic<bool> stop{false};std::thread thread;
    Impl(Log l,std::function<void(const std::wstring&)> s):log(std::move(l)),status(std::move(s)),thread([this]{Run();}){}
    void Run(){
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);Rules rules;DWORD session=0;ProcessIdToSessionId(GetCurrentProcessId(),&session);
        uint64_t current=~uint64_t(0);Settings settings;bool paused=false,ready=false;std::vector<Process> running;
        ComPtr<IWbemServices> service;ComPtr<Sink> starts,stops;std::shared_ptr<Events> events;
        auto disconnect=[&]{if(service){if(starts)service->CancelAsyncCall(starts.Get());if(stops)service->CancelAsyncCall(stops.Get());}starts.Reset();stops.Reset();service.Reset();events.reset();};
        while(!stop){
            bool changed=false;{std::lock_guard lock(mutex);if(generation!=current){settings=pending;paused=pendingPause;current=generation;changed=true;}}
            if(changed){
                rules.Configure(settings);
                disconnect();ready=false;running.clear();
                if(paused){status(L"Paused");}
                else if(settings.profiles.empty() || std::none_of(settings.profiles.begin(),settings.profiles.end(),[](const auto& p){return p.enabled;})){status(L"Ready · add an enabled profile");}
                else if((settings.passive||settings.runAsAdmin)&&!IsAdmin()){status(L"Administrator mode needs elevation");log(L"Monitoring is waiting. Use Restart as administrator; no polling is running.");}
                else {
                    HRESULT hr=S_OK;
                    if(settings.passive){
                        ComPtr<IWbemLocator> locator;hr=CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&locator));
                        if(SUCCEEDED(hr))hr=locator->ConnectServer(BStr(L"ROOT\\CIMV2"),nullptr,nullptr,nullptr,0,nullptr,nullptr,&service);
                        if(SUCCEEDED(hr))hr=CoSetProxyBlanket(service.Get(),RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
                        events=std::make_shared<Events>(wake.value);starts.Attach(new Sink(events,true));stops.Attach(new Sink(events,false));
                        std::set<std::wstring> names;
                        for(const auto& profile:settings.profiles)if(profile.enabled){names.insert(FileName(profile.trigger));for(const auto& target:profile.targets)names.insert(FileName(target.path));}
                        std::wstring nameFilter;
                        for(const auto& name:names){std::wstring escaped;for(wchar_t c:name){if(c==L'\\'||c==L'\'')escaped+=L'\\';escaped+=c;}if(!nameFilter.empty())nameFilter+=L" OR ";nameFilter+=L"ProcessName = '"+escaped+L"'";}
                        const auto startQuery=L"SELECT * FROM Win32_ProcessStartTrace WHERE SessionID = "+std::to_wstring(session)+L" AND ("+nameFilter+L")";
                        // Filtered stop subscriptions were not delivered on the
                        // tested Windows setup. Discard unrelated PIDs in Sink.
                        const auto stopQuery=L"SELECT * FROM Win32_ProcessStopTrace";
                        if(SUCCEEDED(hr))hr=service->ExecNotificationQueryAsync(BStr(L"WQL"),BStr(startQuery),WBEM_FLAG_SEND_STATUS,nullptr,starts.Get());
                        if(SUCCEEDED(hr))hr=service->ExecNotificationQueryAsync(BStr(L"WQL"),BStr(stopQuery),WBEM_FLAG_SEND_STATUS,nullptr,stops.Get());
                    }
                    if(FAILED(hr)){status(L"Passive mode unavailable · see history");log(L"Windows event subscription failed: "+ErrorText(static_cast<DWORD>(hr))+L". Choose regular mode to use process checks.");disconnect();}
                    else {running=Processes(false,&settings,&ready);if(ready){if(events){std::lock_guard lock(events->mutex);for(const auto& p:running)if(p.session==session&&!p.path.empty())events->tracked.insert(p.pid);events->filterReady=true;}status(settings.passive?L"Listening · passive Windows events":L"Watching · 1 second process checks");}else{status(L"Process snapshot failed · see history");log(L"Could not read the initial process list. Save settings to retry.");}}
                }
            }
            if(ready&&!paused){
                if(settings.passive&&events){
                    if(FAILED(events->failure.load())){log(L"Windows event delivery stopped: "+ErrorText(static_cast<DWORD>(events->failure.load())));status(L"Event delivery stopped · save to reconnect");disconnect();ready=false;continue;}
                    std::vector<Event> queue;{std::lock_guard lock(events->mutex);queue.swap(events->queue);}
                    std::stable_sort(queue.begin(),queue.end(),[](const auto& a,const auto& b){return a.time<b.time;});
                    for(auto& e:queue){
                        if(e.start){
                            if(e.process.path.empty())continue;
                            // Start and stop callbacks can arrive out of order.
                            // Verify older matching instances only on a start
                            // event, so a late stop cannot suppress a restart.
                            auto key=Key(e.process.path);
                            running.erase(std::remove_if(running.begin(),running.end(),[&](const auto& p){return p.pid!=e.pid&&Key(p.path)==key&&!Alive(p);}),running.end());
                            auto found=std::find_if(running.begin(),running.end(),[&](const auto& p){return p.pid==e.pid;});
                            if(found!=running.end()&&found->born>e.process.born)continue;
                            running.erase(std::remove_if(running.begin(),running.end(),[&](const auto& p){return p.pid==e.pid;}),running.end());running.push_back(std::move(e.process));
                        }else {
                            auto found=std::find_if(running.begin(),running.end(),[&](const auto& p){return p.pid==e.pid;});
                            if(found!=running.end()&&Alive(*found))continue;
                            running.erase(std::remove_if(running.begin(),running.end(),[&](const auto& p){return p.pid==e.pid;}),running.end());
                            std::lock_guard lock(events->mutex);events->tracked.erase(e.pid);
                        }
                        Evaluate(settings,running,rules,session);
                    }
                }
                Evaluate(settings,running,rules,session);
            }
            DWORD wait=WaitForSingleObject(wake.value,ready&&!settings.passive&&!paused?1000:INFINITE);
            if(wait==WAIT_TIMEOUT){bool ok=false;auto snapshot=Processes(false,&settings,&ok);if(ok)running=std::move(snapshot);}
        }
        disconnect();CoUninitialize();
    }
    static bool Alive(const Process& p){
        Handle h(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.pid));
        if(!h.value||WaitForSingleObject(h.value,0)!=WAIT_TIMEOUT)return false;
        FILETIME c{},e{},k{},u{};return GetProcessTimes(h.value,&c,&e,&k,&u)&&(((static_cast<uint64_t>(c.dwHighDateTime)<<32)|c.dwLowDateTime)==p.born);
    }
    void Evaluate(const Settings& settings,std::vector<Process>& running,Rules& rules,DWORD session){
        std::vector<Process> local;for(const auto& p:running)if(p.session==session)local.push_back(p);
        auto fire=rules.Evaluate(settings,local,false);
        std::set<std::wstring> dispatched;
        for(size_t index:fire){const auto& p=settings.profiles[index];log(L"Triggered: "+p.name);
            for(const auto& target:p.targets){if(stop)return;auto key=Key(target.path);
                // A short-lived target can exit before WMI delivers its start.
                // Recheck only that target when deciding whether to skip it.
                auto stale=[&](const auto& proc){return !proc.path.empty()&&Key(proc.path)==key&&!Alive(proc);};
                local.erase(std::remove_if(local.begin(),local.end(),stale),local.end());
                running.erase(std::remove_if(running.begin(),running.end(),stale),running.end());
                bool already=dispatched.count(key)!=0||std::any_of(local.begin(),local.end(),[&](const auto& proc){return !proc.path.empty()&&Key(proc.path)==key;});
                if(target.skipRunning&&already){log(L"Already running · "+FileName(target.path));continue;}
                DWORD pid=0;std::wstring error;
                if(Launch(target,pid,error)){log(L"Launched · "+FileName(target.path));dispatched.insert(key);if(pid){auto proc=InspectProcess(pid);if(!proc.path.empty()){local.push_back(proc);running.push_back(std::move(proc));}}}
                else log(L"Launch failed · "+FileName(target.path)+L" · "+error);
            }
        }
    }
};
Monitor::Monitor(Log log,std::function<void(const std::wstring&)> status):impl_(std::make_unique<Impl>(std::move(log),std::move(status))){}
Monitor::~Monitor(){Stop();}
void Monitor::Configure(const Settings& settings,bool paused){std::lock_guard lock(impl_->mutex);impl_->pending=settings;impl_->pendingPause=paused;++impl_->generation;SetEvent(impl_->wake.value);}
void Monitor::Stop(){if(impl_&&impl_->thread.joinable()){impl_->stop=true;SetEvent(impl_->wake.value);impl_->thread.join();}}
}
