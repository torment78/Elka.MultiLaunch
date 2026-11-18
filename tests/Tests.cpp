#include "../src/Platform.h"
#include "../src/Games.h"
#include "../src/Updates.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <io.h>

static int checks=0;
static void Check(bool okay,const char* message){++checks;if(!okay){std::cerr<<"FAILED: "<<message<<'\n';throw std::runtime_error(message);}}
static elka::Settings Sample(){return {false,{{L"one",L"Game",L"C:\\Games\\game.exe",true,{{L"C:\\Tools\\helper.exe",L"--name \"Halló\"",L"",true}}}}};}
static void GameChecks(){
    using namespace elka::games;State state;state.enabled=true;state.direction=Direction::Up;state.groupDirections.fill(Direction::Up);state.groupDirections[0]=Direction::Left;state.screen=L"\\\\.\\DISPLAY2";state.x=124;state.y=88;state.favorites={L"0:730",L"1:EA:test"};state.custom.push_back({Store::Other,L"custom",L"Halló \"game\"",L"",{L"C:\\Games\\test.exe",L"--name \"one two\"",L"",false}});State decoded;std::wstring error;
    Check(Decode(Encode(state),decoded,error)&&Encode(decoded)==Encode(state),"launcher settings preserve favorites, Unicode, screen and directions");auto unchanged=Encode(decoded);Check(!Decode("ELKA-GAMES 1\n1 0 0 9",decoded,error)&&Encode(decoded)==unchanged,"malformed launcher settings cannot partly replace favorites");Check(!Decode(Encode(state)+"unexpected",decoded,error),"launcher settings reject trailing records");
    auto folder=std::filesystem::path(elka::Parent(elka::ExePath()))/L"game-library-tests";std::filesystem::create_directories(folder);Check(Save(folder.wstring(),state,error)&&Load(folder.wstring(),decoded,error)&&Encode(decoded)==Encode(state),"launcher settings persist atomically");
    Check(EaContentIds("<contentIDs><contentID>123</contentID><contentID>OFB-EAST:456</contentID><contentID>123</contentID><contentID>bad\"arg</contentID></contentIDs>")==std::vector<std::wstring>{L"123",L"OFB-EAST:456"},"EA IDs are deduplicated and command-unsafe values rejected");
    auto field=[](char id,const std::string& value){return std::string(1,static_cast<char>((id<<3)|2))+std::string(1,static_cast<char>(value.size()))+value;};auto product=field(1,"fen")+field(2,"Fen")+field(3,field(1,"D:/Games/Diablo"));auto battle=ReadBattleDatabase(field(1,product));Check(battle.size()==1&&battle[0].id==L"fen"&&battle[0].path==L"D:/Games/Diablo","Battle.net protobuf installed path extraction");bool rejected=false;try{ReadBattleDatabase(std::string("\x0a\x7f",2));}catch(...){rejected=true;}Check(rejected,"truncated Battle.net database is rejected");
    auto root=folder/L"Steam",second=folder/L"Second library";std::filesystem::create_directories(root/L"steamapps"/L"common"/L"Game One");std::filesystem::create_directories(second/L"steamapps"/L"common"/L"Game Two");std::filesystem::copy_file(elka::ExePath(),root/L"steam.exe",std::filesystem::copy_options::overwrite_existing);
    auto put=[](const std::filesystem::path& p,const std::string& text){std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<text;};
    put(root/L"steamapps"/L"libraryfolders.vdf","// current Steam library format\n\"libraryfolders\" { \"0\" { \"path\" \""+elka::ToUtf8(root.generic_wstring())+"\" } \"1\" { \"path\" \""+elka::ToUtf8(second.generic_wstring())+"\" \"apps\" { \"20\" \"100\" } } }");
    auto manifest=[](const char* id,const char* dir,const char* stateFlags){return std::string("\"AppState\" { \"appid\" \"")+id+"\" \"name\" \"Game \" \"installdir\" \""+dir+"\" \"StateFlags\" \""+stateFlags+"\" }";};
    put(root/L"steamapps"/L"appmanifest_10.acf",manifest("10","Game One","4"));put(second/L"steamapps"/L"appmanifest_20.acf",manifest("20","Game Two","6"));put(root/L"steamapps"/L"appmanifest_30.acf",manifest("30","Game One","2"));put(root/L"steamapps"/L"appmanifest_40.acf",manifest("40","Missing","4"));put(root/L"steamapps"/L"appmanifest_50.acf",manifest("50","../Game One","4"));put(root/L"steamapps"/L"appmanifest_60.acf","\"AppState\" {");
    Library library;DiscoverSteam(root,library);Check(library.games.size()==2,"Steam discovers multiple libraries and excludes missing, incomplete and unsafe manifests");auto steamGame=std::find_if(library.games.begin(),library.games.end(),[](const Game& g){return g.id==L"10";});Check(steamGame!=library.games.end()&&steamGame->target.arguments==L"-applaunch 10"&&!steamGame->target.skipRunning,"Steam target uses app ID and is not suppressed by an open client");Check(!library.notes.empty(),"malformed Steam manifest does not prevent other games from being discovered");
    RECT work{-1920,0,0,1080},dock{-1500,300,-1062,438},tile{-1488,350,-1356,424};for(int d=0;d<4;++d)for(int s=0;s<4;++s){auto p=LayoutDrawer(dock,tile,work,100,static_cast<Direction>(d),static_cast<Direction>(s),96);Check(p.bounds.left>=work.left&&p.bounds.top>=work.top&&p.bounds.right<=work.right&&p.bounds.bottom<=work.bottom,"drawer stays on chosen negative-coordinate monitor");Check(p.capacity>0&&p.cells.size()<=static_cast<size_t>(p.capacity),"long favorite groups paginate within available screen area");}
    auto p=LayoutDrawer({700,500,1138,638},{712,550,844,624},{0,0,800,600},200,Direction::Down,Direction::Right,144);Check(p.bounds.left>=0&&p.bounds.top>=0&&p.bounds.right<=800&&p.bounds.bottom<=600,"DPI-scaled drawer remains reachable when dock has no space on its chosen side");
    auto releases=R"([{"draft":false,"tag_name":"v0.2.0-dev.10","html_url":"https://github.com/torment78/Elka.MultiLaunch/releases/tag/v0.2.0-dev.10","body":"quoted \"notes\" \u2605"},{"draft":true,"tag_name":"v9.0.0","html_url":"https://github.com/torment78/Elka.MultiLaunch/releases/tag/v9.0.0"}])";
    auto update=elka::ParseReleases(releases,L"0.2.0-dev.2");Check(update.available&&update.version==L"v0.2.0-dev.10"&&update.error.empty(),"updater compares numeric dev versions and ignores draft releases");Check(!elka::ParseReleases(releases,L"0.2.0").available,"stable release ranks above development builds of the same version");Check(!elka::ParseReleases(R"([{"tag_name":"v9.0.0","html_url":"https://example.com/other.exe"}])",L"0.2.0-dev.1").available,"updater rejects download links outside the project release page");Check(!elka::ParseReleases("{invalid",L"0.2.0-dev.1").error.empty(),"updater reports malformed responses instead of claiming no update");
}
static void Unit(){
    auto s=Sample();std::wstring error;elka::Settings roundtrip;
    Check(elka::Deserialize(elka::Serialize(s),roundtrip,error),"settings round trip");
    Check(elka::Serialize(s)==elka::Serialize(roundtrip),"lossless Unicode and quoted arguments");
    std::string crlf;for(char c:elka::Serialize(s)){if(c=='\n')crlf+='\r';crlf+=c;}Check(elka::Deserialize(crlf,roundtrip,error)&&elka::Serialize(s)==elka::Serialize(roundtrip),"Windows line endings");
    s.profiles[0].targets[0].arguments=L"\"quoted\"\tline\nnext\\tail\r";s.passive=true;s.runAsAdmin=true;
    Check(elka::Deserialize(elka::Serialize(s),roundtrip,error)&&elka::Serialize(s)==elka::Serialize(roundtrip),"escaped settings round trip");
    auto before=elka::Serialize(roundtrip);
    Check(elka::Deserialize("ELKA-MULTILAUNCH\t1\nmode\tpassive\n",roundtrip,error)&&roundtrip.runAsAdmin,"legacy passive settings migrate to administrator mode");
    before=elka::Serialize(roundtrip);
    for(const auto& bad:{"", "ELKA-MULTILAUNCH\t2\n", "ELKA-MULTILAUNCH\t1\nmode\tother\n", "ELKA-MULTILAUNCH\t1\nmode\tpolling\ntarget\tx\tx\tx\t1\n", "ELKA-MULTILAUNCH\t1\nmode\tpolling\nprofile\tid\tn\\q\tx\t1\n"})Check(!elka::Deserialize(bad,roundtrip,error)&&elka::Serialize(roundtrip)==before,"malformed input cannot partially change settings");
    s=Sample();elka::Rules rules;std::vector<elka::Process> running{{10,1,100,L"game.exe",L"c:/games/GAME.exe"}};
    Check(rules.Evaluate(s,{},false).empty(),"no trigger, no launch");
    Check(rules.Evaluate(s,running,false).size()==1,"detect full-path case-insensitive start");
    Check(rules.Evaluate(s,running,false).empty(),"no repeated launch while running");
    running.push_back({11,1,101,L"game.exe",L"C:\\Games\\game.exe"});
    Check(rules.Evaluate(s,running,false).empty(),"second concurrent instance does not retrigger");
    running.erase(running.begin());Check(rules.Evaluate(s,running,false).empty(),"remaining instance keeps session active");
    running.clear();Check(rules.Evaluate(s,running,false).empty(),"stop rearms");
    running.push_back({11,1,102,L"game.exe",L"C:\\Games\\game.exe"});Check(rules.Evaluate(s,running,false).size()==1,"restart fires again");
    running[0].born=103;Check(rules.Evaluate(s,running,false).size()==1,"PID reuse with new creation time fires");
    s.profiles[0].name=L"Renamed";Check(rules.Evaluate(s,running,false).empty(),"rename does not relaunch");
    s.profiles[0].enabled=false;Check(rules.Evaluate(s,running,false).empty(),"disabled profile");
    s.profiles[0].enabled=true;Check(rules.Evaluate(s,running,false).size()==1,"explicit re-enable rearms");
    s.profiles[0].enabled=false;rules.Configure(s);s.profiles[0].enabled=true;rules.Configure(s);Check(rules.Evaluate(s,running,false).size()==1,"disable during dormant monitor rearms");
    rules.Configure(s);Check(rules.Evaluate(s,running,false).empty(),"saving unchanged settings does not relaunch");
    elka::Rules other;Check(other.Evaluate(s,{{20,1,200,L"game.exe",L"C:\\Other\\game.exe"}},false).empty(),"same basename at different path does not match");
    Check(other.Evaluate(s,{{20,1,200,L"game.exe",L""}},false).empty(),"unreadable path is not guessed");
    elka::Rules paused;Check(paused.Evaluate(s,running,true).empty(),"paused rules do not fire");Check(paused.Evaluate(s,running,false).empty(),"same session is not replayed after pause");
    Check(elka::Validate(Sample()).empty(),"valid profile");s=Sample();s.profiles[0].targets[0].path=s.profiles[0].trigger;Check(!elka::Validate(s).empty(),"self-launch rejected");
    s=Sample();s.passive=true;Check(!elka::Validate(s).empty(),"passive requires administrator option");s.runAsAdmin=true;Check(elka::Validate(s).empty(),"passive with administrator option");
    s=Sample();s.profiles.push_back({L"two",L"Loop",s.profiles[0].targets[0].path,true,{{s.profiles[0].trigger,L"",L"",true}}});Check(!elka::Validate(s).empty(),"cross-profile cycle rejected");
    s=Sample();s.profiles[0].targets.clear();Check(!elka::Validate(s).empty(),"enabled profile requires target");s.profiles[0].enabled=false;Check(elka::Validate(s).empty(),"disabled draft may be incomplete");
    s=Sample();while(s.profiles[0].targets.size()<10)s.profiles[0].targets.push_back(s.profiles[0].targets[0]);Check(elka::Validate(s).empty(),"ten targets supported");s.profiles[0].targets.push_back(s.profiles[0].targets[0]);Check(!elka::Validate(s).empty(),"eleventh target rejected");
    s=Sample();for(int i=1;i<10;++i){auto p=s.profiles[0];p.id=std::to_wstring(i);s.profiles.push_back(p);}Check(elka::Validate(s).empty(),"ten profiles supported");Check(elka::Deserialize(elka::Serialize(s),roundtrip,error),"ten profile persistence");s.profiles.push_back(s.profiles[0]);Check(!elka::Validate(s).empty(),"eleventh profile rejected");
    auto folder=std::filesystem::path(elka::Parent(elka::ExePath()))/L"settings-test";
    Check(elka::SaveSettings(folder.wstring(),Sample(),error),"atomic settings save");elka::Settings loaded;Check(elka::LoadSettings(folder.wstring(),loaded,error)&&elka::Serialize(loaded)==elka::Serialize(Sample()),"settings load from disk");loaded.passive=true;loaded.runAsAdmin=true;Check(elka::SaveSettings(folder.wstring(),loaded,error)&&std::filesystem::exists(folder/L"settings.elka.bak"),"save preserves backup");
    bool okay=false;auto processes=elka::Processes(true,nullptr,&okay);Check(okay&&!processes.empty(),"native process snapshot");auto own=elka::InspectProcess(GetCurrentProcessId());Check(elka::Key(own.path)==elka::Key(elka::ExePath())&&own.born!=0,"full path and creation time inspection");
}
struct Child {HANDLE process=nullptr;~Child(){Stop();}void Stop(){if(process){TerminateProcess(process,0);WaitForSingleObject(process,3000);CloseHandle(process);process=nullptr;}}};
static void Start(const std::wstring& exe,const std::wstring& args,Child& child){std::wstring cmd=L"\""+exe+L"\" "+args;STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};Check(CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=FALSE,"start integration fixture");CloseHandle(pi.hThread);child.process=pi.hProcess;}
static void Integration(bool passive){
    if(passive)Check(elka::IsAdmin(),"passive integration requires elevation");
    auto dir=std::filesystem::path(elka::Parent(elka::ExePath()))/(passive?L"passive-fixtures":L"polling-fixtures");std::filesystem::create_directories(dir);
    auto trigger=(dir/L"Elka.TestTrigger.exe").wstring(),target=(dir/L"Elka.TestTarget.exe").wstring();std::filesystem::copy_file(elka::ExePath(),trigger,std::filesystem::copy_options::overwrite_existing);std::filesystem::copy_file(elka::ExePath(),target,std::filesystem::copy_options::overwrite_existing);
    struct Cleanup {std::wstring path;~Cleanup(){for(const auto& p:elka::Processes(true))if(elka::Key(p.path)==elka::Key(path)){HANDLE h=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,p.pid);if(h){TerminateProcess(h,0);WaitForSingleObject(h,3000);CloseHandle(h);}}}} cleanup{target};
    elka::Settings s{passive,{{L"integration",L"Integration test",trigger,true,{{target,L"--helper",L"",true}}}}};
    s.runAsAdmin=passive;
    std::mutex mutex;std::condition_variable cv;int launched=0,skipped=0;bool ready=false,failed=false;
    elka::Monitor monitor([&](const auto& text){std::lock_guard lock(mutex);std::wcout<<text<<L'\n';if(text.find(L"Launched")!=text.npos)++launched;if(text.find(L"Already running")!=text.npos)++skipped;if(text.find(L"failed")!=text.npos)failed=true;cv.notify_all();},[&](const auto& status){std::lock_guard lock(mutex);std::wcout<<status<<L'\n';ready=status.find(L"Listening")!=status.npos||status.find(L"Watching")!=status.npos;if(status.find(L"unavailable")!=status.npos)failed=true;cv.notify_all();});
    monitor.Configure(s,false);
    {std::unique_lock lock(mutex);Check(cv.wait_for(lock,std::chrono::seconds(12),[&]{return ready||failed;})&&ready&&!failed,"monitor reaches ready state");}
    Child first;Start(trigger,L"--helper",first);
    {std::unique_lock lock(mutex);Check(cv.wait_for(lock,std::chrono::seconds(8),[&]{return launched==1||failed;})&&launched==1&&!failed,"real trigger launches real target");}
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));{std::lock_guard lock(mutex);Check(launched==1,"no duplicate launch across multiple checks");}
    first.Stop();std::this_thread::sleep_for(std::chrono::milliseconds(1800));Child second;Start(trigger,L"--helper",second);
    {std::unique_lock lock(mutex);Check(cv.wait_for(lock,std::chrono::seconds(8),[&]{return skipped==1||failed;})&&skipped==1&&launched==1&&!failed,"restart rearms but skips already running target");}
    second.Stop();monitor.Stop();
}
int wmain(int argc,wchar_t** argv){
    if(argc>1&&std::wstring(argv[1])==L"--helper"){Sleep(30000);return 0;}
    for(int i=1;i+1<argc;++i)if(std::wstring(argv[i])==L"--report"){
        FILE* file=nullptr;if(_wfreopen_s(&file,argv[i+1],L"w",stdout)!=0)return 2;
        _dup2(_fileno(stdout),_fileno(stderr));setvbuf(stdout,nullptr,_IONBF,0);
    }
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
    try{if(argc>1){auto mode=std::wstring(argv[1]);if(mode==L"--library-scan"){auto lib=elka::games::Discover();for(const auto& g:lib.games)std::cout<<elka::ToUtf8(std::wstring(elka::games::StoreName(g.store))+L" | "+g.name+L" | "+g.id+L" | "+g.target.arguments)<<'\n';for(const auto& note:lib.notes)std::wcout<<L"Note: "<<note<<L'\n';std::wcout<<L"Installed games: "<<lib.games.size()<<L'\n';}else if(mode==L"--updates"){auto r=elka::CheckUpdates();Check(r.error.empty(),"live GitHub release query");std::wcout<<L"Newer release: "<<(r.available?r.version:L"none")<<L'\n';}else Integration(mode==L"--passive-integration");}else{Unit();GameChecks();}std::cout<<"PASS: "<<checks<<" checks\n";CoUninitialize();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';CoUninitialize();return 1;}
}
