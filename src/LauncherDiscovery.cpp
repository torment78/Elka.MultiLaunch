#include "Games.h"
#include "Json.h"
#include <algorithm>
#include <fstream>
#include <map>

namespace elka::games {
namespace fs=std::filesystem;
namespace {
constexpr size_t MaxRecords=4096,MaxFile=2*1024*1024,MaxBytes=32*1024*1024;
std::wstring Folder(std::wstring value,bool checkExists=true){
    if(value.size()>=2&&value.front()==L'"'&&value.back()==L'"')value=value.substr(1,value.size()-2);
    if(value.empty()||value.size()>32000||value.find_first_of(L"\"\r\n")!=value.npos||value.find(L'\0')!=value.npos)return {};
    std::replace(value.begin(),value.end(),L'/',L'\\');fs::path path(value);std::error_code ec;
    if(!path.is_absolute()||(checkExists&&!fs::is_directory(path,ec)))return {};
    path=path.lexically_normal();while(path!=path.root_path()&&path.filename().empty())path=path.parent_path();return path.wstring();
}
bool Id(const std::wstring& value){return !value.empty()&&value.size()<160&&value.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==value.npos;}
std::string Read(const fs::path& file,size_t& budget){
    std::error_code ec;auto size=fs::file_size(file,ec);if(ec||size>MaxFile||size>budget)throw std::runtime_error("Installation record size");budget-=static_cast<size_t>(size);
    std::ifstream input(file,std::ios::binary);std::string text(static_cast<size_t>(size),'\0');if(!input.read(text.data(),static_cast<std::streamsize>(size)))throw std::runtime_error("Installation record read");return text;
}
const detail::Json& Field(const detail::Json& value,const char* key){static const detail::Json empty;auto i=value.object.find(key);return i==value.object.end()?empty:i->second;}
std::wstring Text(const detail::Json& value,const char* key){return FromUtf8(Field(value,key).text);}
bool Flag(const detail::Json& value,const char* key,bool fallback){auto i=value.object.find(key);if(i==value.object.end())return fallback;if(i->second.type!=detail::Json::Type::Boolean)throw std::runtime_error("Installation flag");return i->second.boolean;}
bool Contains(const detail::Json& value,const char* key,const char* item){for(const auto& v:Field(value,key).array)if(v.text==item)return true;return false;}
void Add(Library& lib,Game game){
    if(game.name.empty()||game.name.size()>512||game.name.find_first_of(L"\r\n")!=game.name.npos||game.name.find(L'\0')!=game.name.npos)return;
    for(const auto& old:lib.games)if(old.Key()==game.Key())return;game.target.skipRunning=false;lib.games.push_back(std::move(game));
}
void Epic(const fs::path& root,Library& lib){
    size_t budget=MaxBytes;std::map<std::wstring,std::wstring> locations;std::error_code ec;bool skipped=false;
    auto installed=root/L"UnrealEngineLauncher"/L"LauncherInstalled.dat";
    if(fs::is_regular_file(installed,ec))try{auto text=Read(installed,budget);auto doc=detail::Reader(text).Parse();size_t count=0;for(const auto& app:Field(doc,"InstallationList").array){if(++count>MaxRecords)break;auto id=Text(app,"AppName");if(Id(id))locations[elka::Key(id)]=Text(app,"InstallLocation");}}catch(...){skipped=true;}
    fs::directory_iterator it(root/L"EpicGamesLauncher"/L"Data"/L"Manifests",ec),end;size_t visited=0;if(ec&&ec!=std::errc::no_such_file_or_directory)skipped=true;
    for(;!ec&&it!=end&&visited<MaxRecords&&budget;it.increment(ec),++visited){auto file=it->path();if(elka::Key(file.extension().wstring())!=L".item")continue;
        try{
            auto text=Read(file,budget);auto doc=detail::Reader(text).Parse();auto id=Text(doc,"AppName");
            if(!Id(id)||id.rfind(L"UE_",0)==0||Flag(doc,"bIsIncompleteInstall",false))continue;
            if(!Flag(doc,"bIsApplication",true)||!Flag(doc,"bIsExecutable",true))continue;
            if(Contains(doc,"AppCategories","plugins")||Contains(doc,"AppCategories","plugins/engine")||Text(doc,"TechnicalType").find(L"plugins/engine")!=std::wstring::npos)continue;
            bool plugin=false;for(const auto& v:Field(doc,"CompatibleApps").array)if(v.text.rfind("UE_",0)==0)plugin=true;if(plugin)continue;
            if((Contains(doc,"AppCategories","addons")||(!Text(doc,"MainGameAppName").empty()&&Text(doc,"MainGameAppName")!=id))&&!Contains(doc,"AppCategories","addons/launchable"))continue;
            auto folder=Folder(Text(doc,"InstallLocation"));auto found=locations.find(elka::Key(id));if(found!=locations.end()){auto known=Folder(found->second);if(!known.empty())folder=known;}if(folder.empty())continue;
            fs::path relative(Text(doc,"LaunchExecutable"));if(relative.empty()||relative.has_root_path())continue;bool parent=false;for(const auto& part:relative)if(part==L"..")parent=true;if(parent||!IsExe((fs::path(folder)/relative).wstring()))continue;
            auto name=Text(doc,"DisplayName");if(name.empty())name=fs::path(folder).filename().wstring();auto ns=Text(doc,"CatalogNamespace"),item=Text(doc,"CatalogItemId");
            std::wstring launchId=id;if(!ns.empty()||!item.empty()){if(!Id(ns)||!Id(item))continue;launchId=ns+L":"+item+L":"+id;}
            Target target;std::wstring error;if(!ManualTarget(LauncherKind::Epic,L"",launchId,folder,target,error))continue;
            // The shared icon lookup rejects utility/anti-cheat executables and falls back to the game folder.
            Add(lib,{Store::Other,L"epic:"+id,name,folder,target,(fs::path(folder)/relative).wstring()});
        }catch(...){skipped=true;}
    }
    if(skipped||visited==MaxRecords||!budget)lib.notes.push_back(L"Some Epic installation records were unreadable or exceeded the scan limit.");
}
void Ubisoft(const std::vector<InstalledApp>& installs,const std::vector<InstalledApp>& programs,Library& lib){
    std::map<std::wstring,const InstalledApp*> metadata;for(const auto& program:programs){auto path=Folder(program.folder,false);if(!path.empty())metadata.emplace(elka::Key(path),&program);}
    size_t count=0;for(const auto& record:installs){if(++count>MaxRecords)break;Target target;std::wstring error;if(!ManualTarget(LauncherKind::Ubisoft,L"",record.id,L"",target,error)||record.id.find_first_not_of(L"0123456789")!=record.id.npos)continue;
        auto folder=Folder(record.folder);if(folder.empty())continue;Game game{Store::Other,L"ubisoft:"+record.id,record.name.empty()?fs::path(folder).filename().wstring():record.name,folder,target};
        auto found=metadata.find(elka::Key(folder));if(found!=metadata.end()){const auto& program=*found->second;if(!program.name.empty())game.name=program.name;game.icon=program.icon;game.iconIndex=program.iconIndex;}Add(lib,std::move(game));
    }
}
void Rockstar(const std::vector<InstalledApp>& programs,const std::wstring& client,Library& lib){
    if(!IsExe(client))return;
    const wchar_t* ids[]{L"gta5",L"gta5_gen9",L"rdr",L"rdr2",L"lanoire",L"mp3",L"lanoirevr",L"gtasa",L"gta3",L"gtavc",L"bully",L"gta4",L"gta3unreal",L"gtavcunreal",L"gtasaunreal"};
    size_t count=0;for(const auto& record:programs){if(++count>20000)break;auto command=elka::Key(record.uninstall);auto at=command.find(L"-uninstall=");if(at==command.npos)continue;
        auto prefix=command.substr(0,at);if(prefix.find(L"launcher.exe")==prefix.npos&&prefix.find(L"uninstall.exe")==prefix.npos)continue;
        auto id=command.substr(at+11);while(!id.empty()&&iswspace(id.back()))id.pop_back();if(id.size()>=2&&id.front()==L'"'&&id.back()==L'"')id=id.substr(1,id.size()-2);
        if(std::find_if(std::begin(ids),std::end(ids),[&](const auto* known){return id==known;})==std::end(ids))continue;
        auto folder=Folder(record.folder);if(folder.empty())continue;Target target;std::wstring error;if(!ManualTarget(LauncherKind::Rockstar,client,L"",folder,target,error))continue;
        Add(lib,{Store::Other,L"rockstar:"+id,record.name.empty()?fs::path(folder).filename().wstring():record.name,folder,target,record.icon,record.iconIndex});
    }
}
}
bool HasDiscovery(LauncherKind kind){return kind==LauncherKind::Epic||kind==LauncherKind::Ubisoft||kind==LauncherKind::Rockstar;}
bool DiscoveryChanged(const Launchers& before,const Launchers& after){for(size_t i=BuiltinCount;i<StoreCount;++i)if((HasDiscovery(before[i].kind)||HasDiscovery(after[i].kind))&&(before[i].kind!=after[i].kind||before[i].name.empty()!=after[i].name.empty()||before[i].executable!=after[i].executable))return true;return false;}
void DiscoverLauncherGames(const Launchers& entries,const fs::path& epicRoot,const std::vector<InstalledApp>& programs,const std::vector<InstalledApp>& ubisoft,Library& lib){
    for(auto kind:{LauncherKind::Epic,LauncherKind::Ubisoft,LauncherKind::Rockstar}){
        std::vector<size_t> slots;for(size_t i=BuiltinCount;i<StoreCount;++i)if(entries[i].kind==kind&&!entries[i].name.empty())slots.push_back(i);if(slots.empty())continue;
        auto client=std::wstring{};for(auto i:slots)if(IsExe(entries[i].executable)){client=entries[i].executable;break;}if(client.empty())client=FindLauncher(kind);
        Library found;if(kind==LauncherKind::Epic)Epic(epicRoot,found);if(kind==LauncherKind::Ubisoft)Ubisoft(ubisoft,programs,found);if(kind==LauncherKind::Rockstar)Rockstar(programs,client,found);
        for(auto i:slots){lib.clients[i]=IsExe(entries[i].executable)?entries[i].executable:client;for(auto game:found.games){game.store=static_cast<Store>(i);if(kind==LauncherKind::Rockstar)game.target.path=lib.clients[i];Add(lib,std::move(game));}
            lib.status[i]=found.games.empty()?L"No installed games found in this launcher's local records. Use Refresh after installing, or Add manually.":L"Installed games read locally. Favorite a game to add it to the floating drawer.";
            if(kind==LauncherKind::Rockstar&&client.empty())lib.status[i]=L"Rockstar client not found. Use Edit launcher to locate its EXE, then Refresh.";
            if(!found.notes.empty())lib.status[i]=found.notes.front();
        }
    }
}
}
