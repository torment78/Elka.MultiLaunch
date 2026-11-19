#include "../src/Games.h"
#include "../src/Json.h"
#include "../src/GameLauncher.h"
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <thread>
#include <objbase.h>

namespace {
namespace fs=std::filesystem;
using namespace elka;
using namespace elka::games;
std::string Quote(const std::wstring& value){std::string out="\"";for(char c:ToUtf8(value)){if(c=='\\'||c=='"')out+='\\';out+=c;}return out+'"';}
void Write(const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<text;if(!out)throw std::runtime_error("Discovery fixture write");}
}
int DiscoveryChecks(){
    int count=0;auto check=[&](bool good,const char* message){++count;if(!good)throw std::runtime_error(message);};
    auto root=fs::path(Parent(ExePath()))/L"discovery-tests"/NewId();auto game=root/L"Halló Game";Write(game/L"Game.exe","fixture");auto client=ExePath();
    Launchers entries{};entries[3]={L"Epic",client,LauncherKind::Epic,true};auto manifests=root/L"EpicGamesLauncher"/L"Data"/L"Manifests";
    auto manifest=[&](const std::string& extra,const std::wstring& folder){return "{\"AppName\":\"game_id\",\"DisplayName\":\"Hall\\u00f3 Game\",\"InstallLocation\":"+Quote(folder)+",\"LaunchExecutable\":\"Game.exe\",\"CatalogNamespace\":\"namespace\",\"CatalogItemId\":\"catalog\""+extra+"}";};
    auto scan=[&](const std::string& text){Write(manifests/L"test.item",text);Library lib;DiscoverLauncherGames(entries,root,{}, {},lib);return lib;};
    auto valid=manifest("",game.wstring());auto lib=scan(valid);check(lib.games.size()==1,"Epic installed manifest imports one game");auto epic=lib.games.at(0);
    check(epic.name==L"Halló Game"&&epic.id==L"epic:game_id"&&epic.folder==game.wstring(),"Epic Unicode names and stable identity survive local discovery");
    check(epic.target.path==L"com.epicgames.launcher://apps/namespace%3Acatalog%3Agame_id?action=launch&silent=true"&&!epic.target.skipRunning,"Epic launch target retains the complete store identity");
    check(scan("\xef\xbb\xbf"+valid).games.size()==1,"Epic UTF8 BOM is accepted");
    for(const auto& extra:{",\"bIsIncompleteInstall\":true",",\"bIsApplication\":false",",\"bIsExecutable\":false",",\"AppCategories\":[\"addons\"]",",\"AppCategories\":[\"plugins/engine\"]",",\"CompatibleApps\":[\"UE_5.4\"]",",\"MainGameAppName\":\"other_game\"",",\"TechnicalType\":\"plugins/engine\""})check(scan(manifest(extra,game.wstring())).games.empty(),"Epic incomplete installations and non-playable content are excluded");
    check(scan(manifest(",\"AppCategories\":[\"addons\",\"addons/launchable\"]",game.wstring())).games.size()==1,"Epic independently launchable addons remain playable");
    check(scan(manifest("",(root/L"removed").wstring())).games.empty(),"Epic missing installation folders are excluded");
    auto unsafe=valid;auto pos=unsafe.find("Game.exe");unsafe.replace(pos,8,"../Game.exe");check(scan(unsafe).games.empty(),"Epic executable paths cannot escape the installed folder");
    auto missing=valid;pos=missing.find("Game.exe");missing.replace(pos,8,"Missing.exe");check(scan(missing).games.empty(),"Epic missing executables are excluded");
    check(scan(manifest(",\"bIsIncompleteInstall\":\"false\"",game.wstring())).games.empty(),"Epic install flags must be booleans");
    check(scan(valid+"trailing").games.empty(),"Truncated or malformed manifest content is isolated");
    auto bad=scan(std::string(2*1024*1024+1,' '));check(bad.games.empty()&&!bad.status[3].empty(),"Oversized Epic records are bounded and reported");
    Write(manifests/L"duplicate.item",valid);lib=scan(valid);check(lib.games.size()==1,"Duplicate Epic manifests produce one game");
    Write(manifests/L"broken.item","{broken");lib=scan(valid);check(lib.games.size()==1&&lib.status[3].find(L"unreadable")!=std::wstring::npos,"One unreadable Epic manifest does not discard other games");
    auto moved=root/L"Moved game";Write(moved/L"Game.exe","fixture");Write(root/L"UnrealEngineLauncher"/L"LauncherInstalled.dat","{\"InstallationList\":[{\"AppName\":\"game_id\",\"InstallLocation\":"+Quote(moved.wstring())+"}]}");
    lib=scan(valid);check(lib.games.size()==1&&lib.games[0].folder==moved.wstring()&&lib.games[0].Key()==epic.Key(),"Epic moved installations prefer the installed-list location and keep favorites stable");
    entries[8]={L"Second Epic",client,LauncherKind::Epic,false};lib=scan(valid);check(lib.games.size()==2&&static_cast<int>(lib.games[1].store)==8&&lib.games[0].Key()!=lib.games[1].Key(),"Configured duplicate and hidden launchers receive stable slot-specific games");
    entries={};entries[4]={L"Ubisoft",client,LauncherKind::Ubisoft,true};entries[5]={L"Rockstar",client,LauncherKind::Rockstar,true};
    std::vector<InstalledApp> ubi{{L"1234",L"",game.wstring()},{L"1234",L"duplicate",game.wstring()+L"\\"},{L"5678",L"removed",(root/L"removed").wstring()},{L"bad_id",L"bad",game.wstring()}};
    std::vector<InstalledApp> programs{{L"Uplay Install 1234",L"Ubisoft display name",game.wstring(),L"",client,2},{L"Rockstar",L"Rockstar title",game.wstring(),L"\"C:\\Rockstar Games\\Launcher.exe\" -uninstall=rdr2",client,1}};
    lib={};DiscoverLauncherGames(entries,root,programs,ubi,lib);check(lib.games.size()==2,"Ubisoft and Rockstar local records import installed games only");
    auto u=lib.games.at(0),r=lib.games.at(1);check(u.id==L"ubisoft:1234"&&u.name==L"Ubisoft display name"&&u.icon==client&&u.iconIndex==2,"Ubisoft enriches numeric installation records with local name and icon");
    check(u.target.path==L"uplay://launch/1234"&&u.target.arguments.empty()&&!u.target.skipRunning,"Ubisoft discovery launches through the registered store protocol");
    check(r.id==L"rockstar:rdr2"&&r.target.path==client&&r.target.arguments==L"-launchTitleInFolder \""+game.wstring()+L"\""&&r.iconIndex==1,"Rockstar discovery constructs a launch command without running the uninstall record");
    for(const auto& command:{L"steam://uninstall/1234",L"uninstall.exe -uninstall=unknown",L"Launcher.exe -uninstall=rdr2 -other",L"other.exe -uninstall=rdr2"}){Library rejected;DiscoverLauncherGames(entries,root,{{L"",L"Wrong game",game.wstring(),command}},{},rejected);check(rejected.games.empty(),"Unrelated, unknown or ambiguous Rockstar records are excluded");}
    Library stale;DiscoverLauncherGames(entries,root,{{L"",L"Removed Rockstar",(root/L"gone").wstring(),L"Launcher.exe -uninstall=rdr2"}},{},stale);check(stale.games.empty(),"Rockstar removed installations are excluded");
    Library fallback;DiscoverLauncherGames(entries,root,{},{{L"77",L"",game.wstring()+L"\\"}},fallback);check(fallback.games.size()==1&&fallback.games[0].name==L"Halló Game","Ubisoft uses the installation folder name when metadata is missing");
    State saved;for(size_t i=BuiltinCount;i<StoreCount;++i)if(!entries[i].name.empty())saved.launchers[i]=entries[i];saved.favorites={u.Key(),r.Key()};saved.custom.push_back({Store::Other,L"manual",L"Manual game",L"",{client,L"--test",L"",false}});State decoded;std::wstring error;check(Decode(Encode(saved),decoded,error)&&decoded.favorites==saved.favorites&&decoded.custom.size()==1,"Discovered favorites coexist with unchanged manual entries and settings format");
    auto changed=entries;changed[4].visible=false;changed[4].name=L"My Ubisoft";check(!DiscoveryChanged(entries,changed),"Hiding or renaming a launcher adds no scan");changed[4].kind=LauncherKind::Epic;check(DiscoveryChanged(entries,changed),"Changing preset invalidates the discovered library");changed=entries;changed[4].executable=client+L".new";check(DiscoveryChanged(entries,changed),"Changing launcher client requests one refresh");changed=entries;changed[4].name.clear();check(DiscoveryChanged(entries,changed),"Removing a configured launcher invalidates its games");
    Library none;DiscoverLauncherGames({},root,programs,ubi,none);check(none.games.empty(),"Unconfigured providers do no discovery");
    for(const auto& json:{"{\"a\":1,\"a\":2}","{\"a\":01}","{\"a\":1e}","{\"a\":\"\\ud800\"}"}){bool rejected=false;try{detail::Reader(json).Parse();}catch(...){rejected=true;}check(rejected,"Shared JSON reader rejects ambiguous or malformed records");}
    return count;
}
void DiscoveryScan(){
    State state;for(int i=0;i<3;++i){auto kind=i==0?LauncherKind::Epic:i==1?LauncherKind::Ubisoft:LauncherKind::Rockstar;state.launchers[i+3]={LauncherKindName(kind),L"",kind,true};}
    auto start=std::chrono::steady_clock::now();auto lib=Discover(state.launchers);auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    for(int i=0;i<6;++i){size_t count=0;for(const auto& g:lib.games)if(static_cast<int>(g.store)==i)++count;std::wcout<<LauncherName(state,i)<<L": "<<count<<L" installed games\n";}
    std::wcout<<L"One-time library scan: "<<elapsed<<L" ms\n";
}
namespace {
thread_local unsigned refreshes=0,steps=0;
void CALLBACK RefreshStep(HWND,UINT,UINT_PTR,DWORD){
    HWND browser=nullptr;EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL{wchar_t title[80]{};GetWindowTextW(window,title,80);if(std::wstring(title)==L"Installed games & favorites")*reinterpret_cast<HWND*>(data)=window;return TRUE;},reinterpret_cast<LPARAM>(&browser));
    if(++steps>500){if(browser)SendMessageW(browser,WM_COMMAND,IDCANCEL,0);else PostQuitMessage(1);return;}
    if(browser&&steps==1)SendMessageW(browser,WM_COMMAND,23,0);else if(browser&&refreshes>=2)SendMessageW(browser,WM_COMMAND,IDCANCEL,0);
}
}
int DiscoveryNativeChecks(){
    int count=0;std::exception_ptr failure;std::thread worker([&]{auto desktop=CreateDesktopW((L"ElkaDiscoveryTest-"+std::to_wstring(GetCurrentProcessId())).c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);auto original=GetThreadDesktop(GetCurrentThreadId());bool initialized=false;
        try{auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};check(desktop&&SetThreadDesktop(desktop),"create an inactive discovery test desktop");CoInitializeEx(nullptr,COINIT_MULTITHREADED);initialized=true;
            auto directory=fs::path(Parent(ExePath()))/L"discovery-native-tests";State state;state.enabled=true;for(int i=0;i<3;++i){auto kind=i==0?LauncherKind::Epic:i==1?LauncherKind::Ubisoft:LauncherKind::Rockstar;state.launchers[i+3]={LauncherKindName(kind),L"",kind,true};}std::wstring error;check(Save(directory.wstring(),state,error),"save isolated discovery fixtures");
            auto pump=[](unsigned duration){auto end=GetTickCount64()+duration;do{MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(GetTickCount64()<end)MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}while(GetTickCount64()<end);};
            {refreshes=0;GameLauncher launcher(nullptr,directory.wstring(),[](const std::wstring& message){if(message.rfind(L"Game library refreshed",0)==0)++refreshes;});auto deadline=GetTickCount64()+10000;while(!refreshes&&GetTickCount64()<deadline)pump(20);check(refreshes==1,"configured launcher discovery finishes once on initial load");
                pump(1200);check(refreshes==1,"idle launcher does not repeat discovery");steps=0;auto timer=SetTimer(nullptr,0,20,RefreshStep);check(timer!=0,"create bounded refresh driver");launcher.Library(nullptr);KillTimer(nullptr,timer);check(refreshes==2,"Library Refresh performs one additional scan");pump(300);check(refreshes==2,"explicit refresh returns to idle with no repeat scan");}
        }catch(...){failure=std::current_exception();}if(initialized)CoUninitialize();SetThreadDesktop(original);if(desktop)CloseDesktop(desktop);
    });worker.join();if(failure)std::rethrow_exception(failure);return count;
}
