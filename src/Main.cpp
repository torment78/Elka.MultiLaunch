#include "Ui.h"
#include "Platform.h"
#include <shellapi.h>
#include <commctrl.h>
#include <sddl.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <deque>
#include <memory>
#include <algorithm>

namespace {
constexpr UINT ActivityMessage=WM_APP+11,StatusMessage=WM_APP+12,TrayMessage=WM_APP+13;
constexpr int Profiles=100,History=101,SettingsPage=102,Hide=103,ProfileList=110,AddProfile=111,RemoveProfile=112,Name=120,Enabled=121,Trigger=122,TriggerProcess=123,TriggerExe=124,TargetList=130,AddProcess=131,AddExe=132,EditTarget=133,RemoveTarget=134,Save=140,Pause=141,Passive=150,Startup=151,Elevate=152,Admin=153,HistoryList=160;
class TargetWindow:public ui::Window {
public:
    elka::Target target;
    explicit TargetWindow(elka::Target t):target(std::move(t)){}
    void Created()override{Label(L"APPLICATION",10);Edit(target.path,11,true);Button(L"From processes",12);Button(L"Browse EXE",13);Label(L"Arguments (optional)",14);Edit(target.arguments,15);Label(L"Working folder (optional; defaults to the EXE folder)",16);Edit(target.directory,17);Check(L"Skip if this application is already running",18,target.skipRunning);Button(L"Cancel",IDCANCEL);Button(L"Apply target",IDOK,true);}
    void Layout(int w,int)override{Place(10,24,20,w-48,22);Place(11,24,50,w-48,30);Place(12,24,94,146,34);Place(13,182,94,126,34);Place(14,24,152,w-48,24);Place(15,24,180,w-48,30);Place(16,24,232,w-48,24);Place(17,24,260,w-48,30);Place(18,24,306,w-48,34);Place(IDCANCEL,w-268,370,112,34);Place(IDOK,w-144,370,120,34);}
    void Command(int id,int)override{if(id==IDCANCEL)End(0);else if(id==12||id==13){auto p=id==12?ui::PickProcess(hwnd):ui::PickExe(hwnd,Value(11));if(!p.empty())Value(11,p);}else if(id==IDOK){target={Value(11),Value(15),Value(17),Checked(18)};if(!elka::IsExe(target.path)){ui::Notice(hwnd,L"Choose an application",L"Choose an existing EXE file for this target.");return;}std::error_code ec;if(!target.directory.empty()&&!std::filesystem::is_directory(target.directory,ec)){ui::Notice(hwnd,L"Working folder",L"The working folder does not exist. Choose an existing folder or leave it blank.");return;}End(IDOK);}}
};
class MainWindow:public ui::Window {
    elka::Settings draft,saved;std::wstring directory,loadError,status=L"Starting…";std::unique_ptr<elka::Monitor> monitor;
    int selected=-1,page=Profiles;bool loading=false,dirty=false,paused=false,startup=false,trayAdded=false,isolated=false;UINT taskbarCreated=0;
    std::deque<std::wstring> history;
    const std::vector<int> profileControls{ProfileList,AddProfile,RemoveProfile,Name,Enabled,Trigger,TriggerProcess,TriggerExe,TargetList,AddProcess,AddExe,EditTarget,RemoveTarget,200,201,202,203,204};
    const std::vector<int> settingsControls{Passive,Startup,Elevate,Admin,210,211,212,213,214,215};
public:
    explicit MainWindow(std::wstring dir,bool custom):directory(std::move(dir)),isolated(custom){elka::LoadSettings(directory,draft,loadError);saved=draft;startup=isolated?false:elka::StartupEnabled();}
    void Created()override{
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
        Button(L"Profiles",Profiles);Button(L"History",History);Button(L"Settings",SettingsPage);Button(L"Hide to tray",Hide);
        List(ProfileList,58);Button(L"+ New profile",AddProfile);Button(L"Remove",RemoveProfile);
        Label(L"PROFILE NAME",200);Edit(L"",Name);Check(L"Enabled",Enabled,true);
        Label(L"01   WHEN THIS APPLICATION RUNS",201);Edit(L"",Trigger,true);Button(L"From processes",TriggerProcess);Button(L"Browse EXE",TriggerExe);
        Label(L"02   LAUNCH THESE APPLICATIONS",202);List(TargetList,56);Button(L"+ From processes",AddProcess);Button(L"+ Browse EXE",AddExe);Button(L"Edit",EditTarget);Button(L"Remove",RemoveTarget);
        Label(L"Choose one trigger, then add up to ten applications below.",203);Label(L"Targets launch once per trigger session. Closing the trigger rearms the profile.",204);
        Button(L"Save changes",Save,true);Button(L"Pause",Pause);
        List(HistoryList,50);
        Label(L"PROCESS DETECTION",210);Check(L"Passive mode · Windows process notifications",Passive,draft.passive);
        Label(L"Requires administrator rights. Windows notifies Multi-Launch when a process starts or stops.\nThere are no repeating process scans in this mode.",211);
        Label(L"With passive mode off, a native process check runs once per second.\nRegular mode works without administrator rights and may miss very short-lived processes.",212);
        Check(L"Run as administrator · remember for future launches",Admin,draft.runAsAdmin);
        Check(L"Start with Windows · open quietly in the system tray",Startup,startup);
        Label(L"Administrator + Windows startup uses Task Scheduler with highest privileges for your account.\nWithout administrator mode, startup uses the normal Windows startup entry.",213);
        Button(L"Restart as administrator",Elevate);
        Label(std::wstring(elka::IsAdmin()?L"This session is running as administrator. ":L"This session is running normally. ")+L"Elevation changes apply on the next launch.\nApplications launched by an elevated Multi-Launch also receive administrator rights.",214);
        Label(isolated?L"Isolated test data · Windows startup is disabled in this session.":L"Settings and recent history are kept in your Local AppData folder. No network connection is used.",215);
        if(isolated)EnableWindow(Get(Startup),FALSE);
        ReadHistory();for(const auto& line:history)SendMessageW(Get(HistoryList),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str()));
        ReloadProfiles();SelectProfile(draft.profiles.empty()?-1:0);SwitchPage(Profiles);
        AddTray();
        auto post=[this](UINT message,const std::wstring& text){auto p=new std::wstring(text);if(!PostMessageW(hwnd,message,0,reinterpret_cast<LPARAM>(p)))delete p;};
        monitor=std::make_unique<elka::Monitor>([post](const auto& text){post(ActivityMessage,text);},[post](const auto& text){post(StatusMessage,text);});
        monitor->Configure(saved,false);
        if(!loadError.empty()){Log(loadError);ui::Notice(hwnd,L"Settings need attention",loadError);}
    }
    void Layout(int w,int h)override{
        Place(Profiles,w-462,29,94,34);Place(History,w-358,29,90,34);Place(SettingsPage,w-258,29,94,34);Place(Hide,w-154,29,126,34);
        Place(ProfileList,24,126,224,h-254);Place(AddProfile,24,h-112,138,34);Place(RemoveProfile,172,h-112,76,34);
        int x=276,cw=w-x-28;
        Place(200,x,122,cw-132,20);Place(Name,x,150,cw-144,30);Place(Enabled,w-152,147,124,34);
        Place(203,x,192,cw,24);Place(201,x,236,cw,24);Place(Trigger,x,266,cw,30);Place(TriggerProcess,x,310,150,34);Place(TriggerExe,x+162,310,128,34);
        Place(202,x,374,cw,24);Place(TargetList,x,406,cw,h-566);
        Place(AddProcess,x,h-148,158,34);Place(AddExe,x+170,h-148,140,34);Place(EditTarget,w-202,h-148,76,34);Place(RemoveTarget,w-114,h-148,86,34);
        Place(204,x,h-100,cw,36);
        Place(Save,w-174,h-50,146,34);Place(Pause,w-278,h-50,92,34);
        Place(HistoryList,28,126,w-56,h-204);
        Place(210,36,128,w-72,24);Place(Passive,36,163,w-72,36);Place(211,66,212,w-100,52);Place(212,66,280,w-100,52);
        Place(Admin,36,330,w-72,36);Place(Startup,36,376,w-72,36);Place(213,66,426,w-100,54);Place(Elevate,36,496,224,36);Place(214,36,552,w-72,50);Place(215,36,620,w-72,44);
    }
    void Paint(HDC dc,RECT r)override{
        int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
        TextAt(dc,L"ELKA",{S(28),S(17),S(280),S(37)},ui::Accent,bold);
        TextAt(dc,L"Multi-Launch",{S(26),S(36),S(350),S(76)},ui::Text,title);
        Box(dc,{S(24),S(94),r.right-S(24),S(95)},ui::Edge);
        const auto heading=page==Profiles?L"YOUR PROFILES   "+std::to_wstring(draft.profiles.size())+L" / 10":page==History?L"RECENT ACTIVITY   ·   LAST 200 EVENTS":L"SETTINGS";
        TextAt(dc,heading,{S(28),S(100),r.right-S(28),S(122)},ui::Muted,bold);
        Box(dc,{S(24),S(h-64),r.right-S(24),S(h-63)},ui::Edge);
        TextAt(dc,(dirty?L"Unsaved changes   ·   ":L"")+status,{S(28),S(h-52),S(w-294),S(h-18)},dirty?ui::Danger:ui::Accent);
        if(page==Profiles&&selected<0){TextAt(dc,L"One app starts. The rest follow.",{S(292),S(235),S(w-50),S(280)},ui::Text,title);TextAt(dc,L"Create a profile on the left to choose a trigger\nand the applications it should launch.",{S(294),S(302),S(w-50),S(374)},ui::Muted,font,DT_LEFT|DT_WORDBREAK);}
        if(page==Profiles&&selected>=0&&draft.profiles[selected].targets.empty()){TextAt(dc,L"Add applications using the buttons below.",{S(294),S(426),S(w-44),S(472)},ui::Muted);}
    }
    void MarkDirty(){if(!loading){dirty=true;InvalidateRect(hwnd,nullptr,FALSE);}}
    void Capture(){if(selected>=0&&static_cast<size_t>(selected)<draft.profiles.size()){auto& p=draft.profiles[selected];p.name=Value(Name);p.enabled=Checked(Enabled);}draft.passive=Checked(Passive);draft.runAsAdmin=Checked(Admin);}
    void ReloadProfiles(){SendMessageW(Get(ProfileList),LB_RESETCONTENT,0,0);for(const auto& p:draft.profiles)SendMessageW(Get(ProfileList),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.name.c_str()));if(selected>=0)SendMessageW(Get(ProfileList),LB_SETCURSEL,selected,0);EnableWindow(Get(AddProfile),draft.profiles.size()<elka::MaxProfiles);}
    void ReloadTargets(){SendMessageW(Get(TargetList),LB_RESETCONTENT,0,0);if(selected>=0){for(const auto& t:draft.profiles[selected].targets)SendMessageW(Get(TargetList),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(t.path.c_str()));bool canAdd=draft.profiles[selected].targets.size()<elka::MaxTargets;EnableWindow(Get(AddProcess),canAdd);EnableWindow(Get(AddExe),canAdd);}InvalidateRect(Get(ProfileList),nullptr,TRUE);InvalidateRect(hwnd,nullptr,FALSE);}
    void SelectProfile(int index){loading=true;selected=index;if(index>=0){auto& p=draft.profiles[index];Value(Name,p.name);Checked(Enabled,p.enabled);Value(Trigger,p.trigger);SendMessageW(Get(ProfileList),LB_SETCURSEL,index,0);}else{Value(Name,L"");Value(Trigger,L"");}ReloadTargets();loading=false;SwitchPage(page);}
    void SwitchPage(int next){page=next;for(int id:profileControls)Show(id,next==Profiles&&(id==ProfileList||id==AddProfile||id==RemoveProfile||selected>=0));for(int id:settingsControls)Show(id,next==SettingsPage);Show(HistoryList,next==History);for(int id:{Profiles,History,SettingsPage}){SetPropW(Get(id),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(id==next)));InvalidateRect(Get(id),nullptr,TRUE);}InvalidateRect(hwnd,nullptr,TRUE);}
    bool SaveConfig(bool updateStartup){
        Capture();auto error=elka::Validate(draft);if(!error.empty()){ui::Notice(hwnd,L"Check this profile",error);return false;}
        for(const auto& p:draft.profiles)if(p.enabled){if(!elka::IsExe(p.trigger)){ui::Notice(hwnd,L"Trigger not found",L"The trigger EXE for '"+p.name+L"' is not available. Choose an existing file or disable the profile.");return false;}for(const auto& t:p.targets)if(!elka::IsExe(t.path)){ui::Notice(hwnd,L"Target not found",L"This target EXE is not available:\n"+t.path);return false;}}
        if(!elka::SaveSettings(directory,draft,error)){ui::Notice(hwnd,L"Could not save",error);return false;}
        bool startupOkay=true;
        if(updateStartup&&!isolated&&(Checked(Startup)!=startup||(Checked(Startup)&&draft.runAsAdmin!=saved.runAsAdmin))){startupOkay=elka::SetStartup(Checked(Startup),draft.runAsAdmin,error);if(!startupOkay){Log(error);ui::Notice(hwnd,L"Startup needs attention",L"Your profiles were saved, but Windows startup could not be updated.\n\n"+error);}startup=elka::StartupEnabled();if(startupOkay)Checked(Startup,startup);}
        saved=draft;dirty=!startupOkay;ReloadProfiles();monitor->Configure(saved,paused);Log(L"Settings saved · "+std::to_wstring(saved.profiles.size())+L" profiles");InvalidateRect(hwnd,nullptr,FALSE);return startupOkay;
    }
    void RestartAdmin(){
        if(elka::IsAdmin()){ui::Notice(hwnd,L"Administrator",L"Multi-Launch is already running as administrator.");return;}
        Checked(Admin,true);
        if(!SaveConfig(false))return;
        std::wstring args=L"--restart --data-dir \""+directory+L"\"";if(!isolated)args+=Checked(Startup)?L" --enable-startup":L" --disable-startup";else args+=L" --isolated";
        SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOASYNC;info.lpVerb=L"runas";auto exe=elka::ExePath();info.lpFile=exe.c_str();info.lpParameters=args.c_str();info.nShow=SW_SHOWNORMAL;
        if(ShellExecuteExW(&info))Exit();else if(GetLastError()!=ERROR_CANCELLED)ui::Notice(hwnd,L"Could not restart",elka::ErrorText(GetLastError()));
    }
    void Command(int id,int code)override{
        if(id==Profiles||id==History||id==SettingsPage){Capture();SwitchPage(id);return;}
        if(id==Name&&code==EN_CHANGE){MarkDirty();return;}
        if(id==ProfileList&&code==LBN_SELCHANGE){Capture();auto index=static_cast<int>(SendMessageW(Get(ProfileList),LB_GETCURSEL,0,0));ReloadProfiles();SelectProfile(index);return;}
        if(id==Enabled||id==Passive||id==Startup||id==Admin){if(id==Passive&&Checked(Passive))Checked(Admin,true);if(id==Admin&&!Checked(Admin))Checked(Passive,false);MarkDirty();return;}
        if(id==Hide){Close();return;}if(id==Save){SaveConfig(true);return;}if(id==Pause){paused=!paused;Value(Pause,paused?L"Resume":L"Pause");monitor->Configure(saved,paused);Log(paused?L"Monitoring paused.":L"Monitoring resumed.");UpdateTray();return;}if(id==Elevate){RestartAdmin();return;}
        if(id==AddProfile){if(draft.profiles.size()>=elka::MaxProfiles)return;Capture();draft.profiles.push_back({elka::NewId(),L"Profile "+std::to_wstring(draft.profiles.size()+1),L"",true,{}});ReloadProfiles();SelectProfile(static_cast<int>(draft.profiles.size()-1));MarkDirty();SetFocus(Get(Name));SendMessageW(Get(Name),EM_SETSEL,0,-1);return;}
        if(selected<0)return;auto& p=draft.profiles[selected];
        if(id==RemoveProfile){if(!ui::Notice(hwnd,L"Remove profile",L"Remove '"+p.name+L"' from your profiles? Save changes to apply the removal.",true))return;draft.profiles.erase(draft.profiles.begin()+selected);selected=draft.profiles.empty()?-1:(std::min)(selected,static_cast<int>(draft.profiles.size()-1));ReloadProfiles();SelectProfile(selected);MarkDirty();return;}
        if(id==TriggerProcess||id==TriggerExe){auto path=id==TriggerProcess?ui::PickProcess(hwnd):ui::PickExe(hwnd,p.trigger);if(!path.empty()){p.trigger=path;Value(Trigger,path);if(p.name.rfind(L"Profile ",0)==0){p.name=std::filesystem::path(path).stem().wstring();Value(Name,p.name);ReloadProfiles();}MarkDirty();}return;}
        if(id==AddProcess||id==AddExe){if(p.targets.size()>=elka::MaxTargets)return;auto path=id==AddProcess?ui::PickProcess(hwnd):ui::PickExe(hwnd);if(!path.empty()){TargetWindow editor({path,L"",L"",true});editor.Create(L"Add launch target",690,430,hwnd);if(editor.Modal(hwnd)==IDOK){p.targets.push_back(editor.target);ReloadTargets();MarkDirty();}}return;}
        int target=static_cast<int>(SendMessageW(Get(TargetList),LB_GETCURSEL,0,0));if(target<0||static_cast<size_t>(target)>=p.targets.size())return;
        if(id==EditTarget||(id==TargetList&&code==LBN_DBLCLK)){TargetWindow editor(p.targets[target]);editor.Create(L"Edit launch target",690,430,hwnd);if(editor.Modal(hwnd)==IDOK){p.targets[target]=editor.target;ReloadTargets();SendMessageW(Get(TargetList),LB_SETCURSEL,target,0);MarkDirty();}}else if(id==RemoveTarget){p.targets.erase(p.targets.begin()+target);ReloadTargets();MarkDirty();}
    }
    void Item(DRAWITEMSTRUCT* d)override{
        if(d->CtlType==ODT_MENU){Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);auto r=d->rcItem;r.left+=S(14);TextAt(d->hDC,reinterpret_cast<const wchar_t*>(d->itemData),r);return;}
        if(d->itemID==static_cast<UINT>(-1))return;Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);RECT r=d->rcItem;r.left+=S(12);r.right-=S(12);r.bottom=r.top+S(30);
        if(d->CtlID==ProfileList&&d->itemID<draft.profiles.size()){auto& p=draft.profiles[d->itemID];TextAt(d->hDC,p.name,r,ui::Text,bold);r.top+=S(28);r.bottom+=S(24);TextAt(d->hDC,(p.enabled?L"Enabled":L"Disabled")+std::wstring(L"   ·   ")+std::to_wstring(p.targets.size())+L" targets",r,ui::Muted);}
        else if(d->CtlID==TargetList&&selected>=0&&d->itemID<draft.profiles[selected].targets.size()){auto& t=draft.profiles[selected].targets[d->itemID];TextAt(d->hDC,std::to_wstring(d->itemID+1)+L"   "+elka::FileName(t.path)+(t.skipRunning?L"   ·   skip if running":L""),r,ui::Text,bold);r.top+=S(27);r.bottom+=S(22);TextAt(d->hDC,t.path,r,ui::Muted);}
        else if(d->CtlID==HistoryList&&d->itemID<history.size()){auto& text=history[d->itemID];r.bottom=d->rcItem.bottom-S(4);r.top+=S(5);TextAt(d->hDC,text,r,text.find(L"failed")!=text.npos||text.find(L"could not")!=text.npos?ui::Danger:ui::Text,font,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);}
    }
    void ReadHistory(){std::error_code ec;auto file=std::filesystem::path(directory)/L"history.log";if(!std::filesystem::exists(file,ec)||std::filesystem::file_size(file,ec)>512*1024)return;std::ifstream f(file,std::ios::binary);std::string line;while(std::getline(f,line)){try{history.push_back(elka::FromUtf8(line));if(history.size()>200)history.pop_front();}catch(...){}}}
    void Log(const std::wstring& text){SYSTEMTIME t{};GetLocalTime(&t);wchar_t stamp[40]{};swprintf_s(stamp,L"%04u-%02u-%02u  %02u:%02u:%02u   ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);auto line=stamp+text;std::replace(line.begin(),line.end(),L'\r',L' ');std::replace(line.begin(),line.end(),L'\n',L' ');history.push_back(line);if(history.size()>200){history.pop_front();SendMessageW(Get(HistoryList),LB_DELETESTRING,0,0);}SendMessageW(Get(HistoryList),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str()));SendMessageW(Get(HistoryList),LB_SETTOPINDEX,history.size()>9?history.size()-9:0,0);
        std::error_code ec;std::filesystem::create_directories(directory,ec);if(!ec){std::ofstream f(std::filesystem::path(directory)/L"history.log",std::ios::binary|std::ios::trunc);for(const auto& item:history)f<<elka::ToUtf8(item)<<'\n';}
    }
    void AddTray(){NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;n.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;n.uCallbackMessage=TrayMessage;n.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101));wcscpy_s(n.szTip,L"Elka Multi-Launch");trayAdded=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;if(trayAdded){n.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&n);}}
    void UpdateTray(){NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;n.uFlags=NIF_TIP;wcscpy_s(n.szTip,paused?L"Elka Multi-Launch · paused":L"Elka Multi-Launch · monitoring");Shell_NotifyIconW(NIM_MODIFY,&n);}
    void Reveal(){ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);}
    void Close()override{if(trayAdded)ShowWindow(hwnd,SW_HIDE);else{ui::Notice(hwnd,L"Tray unavailable",L"The Windows notification area is unavailable. Multi-Launch will stay visible so you can still control it.");}}
    void Exit(){if(dirty&&!ui::Notice(hwnd,L"Unsaved changes",L"Exit without saving your latest changes?",true))return;monitor->Stop();NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);End(0);PostQuitMessage(0);}
    LRESULT Message(UINT msg,WPARAM w,LPARAM l,bool& handled)override{
        handled=true;
        if(msg==ActivityMessage||msg==StatusMessage){std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(l));if(msg==ActivityMessage)Log(*text);else{status=*text;Log(status);InvalidateRect(hwnd,nullptr,FALSE);}return 0;}
        if(msg==taskbarCreated&&taskbarCreated){AddTray();if(!trayAdded)Reveal();return 0;}
        if(msg==TrayMessage){auto event=LOWORD(l);if(event==WM_LBUTTONDBLCLK||event==NIN_KEYSELECT)Reveal();else if(event==WM_CONTEXTMENU){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_OWNERDRAW,1,L"Open Multi-Launch");AppendMenuW(menu,MF_OWNERDRAW,2,paused?L"Resume monitoring":L"Pause monitoring");AppendMenuW(menu,MF_OWNERDRAW,3,L"Exit");auto brush=CreateSolidBrush(ui::Panel);MENUINFO appearance{sizeof(appearance)};appearance.fMask=MIM_BACKGROUND;appearance.hbrBack=brush;SetMenuInfo(menu,&appearance);POINT p{};GetCursorPos(&p);SetForegroundWindow(hwnd);auto item=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(menu);DeleteObject(brush);PostMessageW(hwnd,WM_NULL,0,0);if(item==1)Reveal();if(item==2)Command(Pause,BN_CLICKED);if(item==3)Exit();}return 0;}
        if(msg==WM_GETMINMAXINFO){auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={S(1020),S(770)};return 0;}
        if(msg==WM_QUERYENDSESSION)return TRUE;
        if(msg==WM_ENDSESSION&&w){if(monitor)monitor->Stop();return 0;}
        handled=false;return 0;
    }
};
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool tray=false,restart=false,custom=false,configureInstall=false,installAdmin=false,installStartup=false;int startRequest=0;auto directory=elka::DataDirectory();
    for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--tray")tray=true;else if(arg==L"--restart")restart=true;else if(arg==L"--data-dir"&&i+1<argc){directory=argv[++i];custom=true;}else if(arg==L"--enable-startup"){startRequest=1;custom=false;}else if(arg==L"--disable-startup"){startRequest=-1;custom=false;}else if(arg==L"--isolated")custom=true;else if(arg==L"--normal-data")custom=false;else if(arg==L"--configure-install")configureInstall=true;else if(arg==L"--admin")installAdmin=true;else if(arg==L"--startup")installStartup=true;}LocalFree(argv);
    // One instance per logon session and data directory. The restart path waits
    // for the old owner to exit before loading settings or creating its window.
    std::wstring mutexName=L"Local\\Elka.MultiLaunch."+std::to_wstring(std::hash<std::wstring>{}(elka::Key(directory)));
    PSECURITY_DESCRIPTOR security=nullptr;ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;OW)S:(ML;;NW;;;ME)",SDDL_REVISION_1,&security,nullptr);SECURITY_ATTRIBUTES sa{sizeof(sa),security,FALSE};
    HANDLE mutex=CreateMutexW(security?&sa:nullptr,FALSE,mutexName.c_str());if(security)LocalFree(security);
    if(!mutex){ui::Notice(nullptr,L"Elka Multi-Launch",L"Could not create the single-instance lock: "+elka::ErrorText(GetLastError()));CoUninitialize();return 1;}
    DWORD acquired=WaitForSingleObject(mutex,restart?10000:0);
    if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED){auto window=FindWindowW(L"Elka.MultiLaunch.Window",L"Elka Multi-Launch");if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}CloseHandle(mutex);CoUninitialize();return configureInstall?3:0;}
    std::wstring startupError;elka::Settings desired;std::wstring settingsError;elka::LoadSettings(directory,desired,settingsError);
    if(configureInstall){
        desired.runAsAdmin=installAdmin;if(!installAdmin)desired.passive=false;
        bool okay=settingsError.empty();
        if(okay)okay=elka::SaveSettings(directory,desired,settingsError);
        if(okay&&!custom)okay=elka::SetStartup(installStartup,installAdmin,settingsError);
        if(!okay)ui::Notice(nullptr,L"Installer settings",settingsError);
        ReleaseMutex(mutex);CloseHandle(mutex);CoUninitialize();return okay?0:2;
    }
    if(desired.runAsAdmin&&!elka::IsAdmin()){
        std::wstring args=L"--restart --data-dir \""+directory+L"\""+(custom?L" --isolated":L" --normal-data")+(tray?L" --tray":L"");
        SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOASYNC;info.lpVerb=L"runas";auto exe=elka::ExePath();info.lpFile=exe.c_str();info.lpParameters=args.c_str();info.nShow=tray?SW_HIDE:SW_SHOWNORMAL;
        if(ShellExecuteExW(&info)){ReleaseMutex(mutex);CloseHandle(mutex);CoUninitialize();return 0;}
        tray=false;startupError=L"Administrator launch was cancelled or unavailable. Monitoring will wait until you restart as administrator, or turn off administrator mode.";
    }
    if(startRequest){if(settingsError.empty())elka::SetStartup(startRequest>0,desired.runAsAdmin,startupError);else startupError=settingsError;}
    int result=0;
    {MainWindow main(directory,custom);if(main.Create(L"Elka Multi-Launch",1120,760,nullptr,true)){
        if(!startupError.empty())ui::Notice(main.hwnd,L"Startup needs attention",startupError);
        ShowWindow(main.hwnd,tray?SW_HIDE:SW_SHOW);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(main.hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}result=static_cast<int>(msg.wParam);
    }else result=1;}
    ReleaseMutex(mutex);CloseHandle(mutex);CoUninitialize();return result;
}
