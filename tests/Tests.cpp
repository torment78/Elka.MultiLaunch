#include "../src/Platform.h"
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
    try{if(argc>1){auto mode=std::wstring(argv[1]);Integration(mode==L"--passive-integration");}else Unit();std::cout<<"PASS: "<<checks<<" checks\n";CoUninitialize();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';CoUninitialize();return 1;}
}
