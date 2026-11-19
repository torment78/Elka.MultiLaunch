#include "../src/Games.h"
#include "../src/Ui.h"
#include <shellapi.h>
#include <objbase.h>
#include <stdexcept>
#include <thread>

int LauncherChecks(){
    using namespace elka;using namespace elka::games;int count=0;auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};Target target;std::wstring error;
    for(auto id:{L"1234",L"uplay://launch/1234",L"uplay://launch/1234/0"})check(ManualTarget(LauncherKind::Ubisoft,L"",id,L"",target,error)&&IsUbisoftLaunchUri(target.path)&&IsGameLaunchUri(target.path)&&target.arguments.empty()&&!target.skipRunning,"Ubisoft IDs and shortcut links generate native launch targets");auto ubisoft=target;
    for(auto bad:{L"",L"-1",L"12&action=uninstall",L"uplay://install/1234",L"uplay://launch/1234/1",L"uplay://launch/1234/0/0",L"uplay://launch/1234?other=1",L"https://example.com/1234",L"uplay://launch/%31",L"123456789012345678901"})check(!ManualTarget(LauncherKind::Ubisoft,L"",bad,L"",target,error)&&target.path==ubisoft.path,"malformed Ubisoft input cannot replace a saved target or select another action");
    check(ManualTarget(LauncherKind::Epic,L"",L"",L"C:\\Games\\Halló & Co#1",target,error)&&target.path==L"com.epicgames.launcher://apps/C%3A%2FGames%2FHall%C3%B3%20%26%20Co%231?action=launch&silent=true"&&IsEpicLaunchUri(target.path),"Epic folder launch encodes Unicode and reserved characters within the app path");auto epic=target;
    for(auto bad:{L"relative\\game",L"C:game",L"C:\\Game\"bad",L"C:\\Game\nBad"})check(!ManualTarget(LauncherKind::Epic,L"",L"",bad,target,error)&&target.path==epic.path,"Epic folder launch rejects relative and invalid paths without replacing the target");
    check(ManualTarget(LauncherKind::Rockstar,L"C:\\Rockstar Games\\Launcher.exe",L"",L"D:\\Games\\Halló World\\",target,error)&&target.path==L"C:\\Rockstar Games\\Launcher.exe"&&!target.skipRunning,"Rockstar uses its client and preserves the game installation folder");
    int arguments=0;auto parsed=CommandLineToArgvW((L"Launcher.exe "+target.arguments).c_str(),&arguments);check(parsed&&arguments==3&&std::wstring(parsed[1])==L"-launchTitleInFolder"&&std::wstring(parsed[2])==L"D:\\Games\\Halló World\\","Rockstar folder is exactly one argument including spaces, Unicode and trailing slash");if(parsed)LocalFree(parsed);auto rockstar=target;
    for(auto bad:{L"",L"relative",L"C:\\Game\" -uninstall=test",L"C:\\Game\rBad"})check(!ManualTarget(LauncherKind::Rockstar,L"client.exe",L"",bad,target,error)&&target.arguments==rockstar.arguments,"Rockstar rejects missing or invalid folders without changing an existing target");
    check(!ManualTarget(LauncherKind::Rockstar,L"client.exe",L"-uninstall=test",L"C:\\Game",target,error),"Rockstar folder preset rejects unrelated extra commands");
    check(!ManualTarget(static_cast<LauncherKind>(LauncherKindCount),L"client.exe",L"",L"",target,error),"unknown launcher presets are rejected");
    State state;state.launchers[4]={L"Ubisoft Connect",L"",LauncherKind::Ubisoft,true};state.launchers[8]={L"Rockstar Games",L"",LauncherKind::Rockstar,true};state.colors[4]=RGB(120,40,60);state.palette.count=1;state.palette.colors[0]=RGB(10,20,30);state.groupDirections[4]=Direction::Left;state.custom.push_back({static_cast<Store>(4),L"ubi",L"Ubisoft fixture",L"",ubisoft});state.custom.push_back({static_cast<Store>(8),L"rgl",L"Rockstar fixture",L"D:\\Games\\Halló World\\",rockstar});state.favorites={state.custom[0].Key(),state.custom[1].Key()};State decoded;
    check(Decode(Encode(state),decoded,error)&&Encode(decoded)==Encode(state),"new presets and game commands persist without requiring clients to be installed");
    auto v5=Encode(State{});v5[11]='5';check(Decode(v5,decoded,error)&&Encode(decoded)==Encode(State{}),"version five launcher settings migrate with existing defaults intact");
    auto invalid=state;invalid.launchers[8].kind=static_cast<LauncherKind>(LauncherKindCount);auto unchanged=Encode(decoded);check(!Decode(Encode(invalid),decoded,error)&&Encode(decoded)==unchanged,"unknown preset does not partially overwrite settings");
    auto wrongVersion=Encode(state);wrongVersion[11]='5';check(!Decode(wrongVersion,decoded,error)&&Encode(decoded)==unchanged,"extended presets require the new settings version");
    Settings settings;settings.profiles.push_back({L"fixture",L"Games",L"C:\\Trigger.exe",true,{ubisoft,rockstar,epic}});Settings loaded;check(Deserialize(Serialize(settings),loaded,error)&&Serialize(loaded)==Serialize(settings),"all new launcher targets fit the existing process profile format");
    DWORD pid=42;auto extra=ubisoft;extra.arguments=L"unexpected";check(!Launch(extra,pid,error)&&pid==0,"Ubisoft protocol dispatch rejects extra arguments before starting anything");extra=ubisoft;extra.directory=L"C:\\Game";check(!Launch(extra,pid,error)&&pid==0,"Ubisoft protocol dispatch rejects a separate working folder");
    check(!IsGameLaunchUri(L"uplay://install/1234")&&!IsGameLaunchUri(L"file:///C:/test.exe")&&!IsGameLaunchUri(L"https://example.com"),"protocol support remains restricted to game-launch actions");
    Game game{Store::Other,L"uri",L"Fixture",L"",ubisoft};check(FindGameIcon(game).empty(),"a launch URI is not treated as an icon filename");
    return count;
}
namespace {
thread_local std::wstring folderText;
thread_local unsigned attempts=0;
thread_local int pickerMode=0;
thread_local bool invalidKeptOpen=false;
void CALLBACK PickerStep(HWND,UINT,UINT_PTR,DWORD){
    HWND picker=nullptr;EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM result)->BOOL{wchar_t title[80]{};GetWindowTextW(window,title,80);if(std::wstring(title)==L"Select a game folder")*reinterpret_cast<HWND*>(result)=window;return TRUE;},reinterpret_cast<LPARAM>(&picker));
    ++attempts;if(!picker){if(attempts>80)PostQuitMessage(1);return;}if(attempts>80||pickerMode==1){SendMessageW(picker,WM_COMMAND,IDCANCEL,0);return;}
    if(pickerMode==2&&attempts==1){SetDlgItemTextW(picker,11,(folderText+L"\\not-present").c_str());SendMessageW(picker,WM_COMMAND,IDOK,0);return;}
    if(pickerMode==2)invalidKeptOpen=true;SetDlgItemTextW(picker,11,(L"\""+folderText+L"\"").c_str());SendMessageW(picker,WM_COMMAND,IDOK,0);
}
}
int LauncherPickerChecks(){
    int count=0;std::exception_ptr failure;std::thread worker([&]{auto desktop=CreateDesktopW((L"ElkaLauncherTest-"+std::to_wstring(GetCurrentProcessId())).c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);auto original=GetThreadDesktop(GetCurrentThreadId());bool initialized=false;
        try{auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};check(desktop&&SetThreadDesktop(desktop),"create inactive folder-picker test desktop");CoInitializeEx(nullptr,COINIT_MULTITHREADED);initialized=true;
            folderText=(std::filesystem::path(elka::Parent(elka::ExePath()))/L"launcher-picker-tests"/L"Halló Game").wstring();std::filesystem::create_directories(folderText);
            for(pickerMode=0;pickerMode<3;++pickerMode){attempts=0;auto timer=SetTimer(nullptr,0,30,PickerStep);check(timer!=0,"create bounded native picker driver");auto selected=ui::PickFolder(nullptr,folderText);KillTimer(nullptr,timer);check(pickerMode==1?selected.empty():elka::Key(selected)==elka::Key(folderText),"native dark folder picker accepts a quoted Unicode path and respects Cancel");}
            check(invalidKeptOpen,"invalid folder leaves the picker open instead of accepting the previous folder");
        }catch(...){failure=std::current_exception();}if(initialized)CoUninitialize();SetThreadDesktop(original);if(desktop)CloseDesktop(desktop);
    });worker.join();if(failure)std::rethrow_exception(failure);return count;
}
