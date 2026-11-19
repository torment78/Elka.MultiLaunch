#include "Games.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <shellapi.h>
#include <shlobj.h>

namespace elka::games {
namespace fs = std::filesystem;
std::wstring Game::Key() const { return std::to_wstring(static_cast<int>(store)) + L":" + elka::Key(id); }
const wchar_t* StoreName(Store s) { static const wchar_t* names[]{L"Steam",L"EA app",L"Battle.net",L"Other"}; return names[static_cast<size_t>(s)]; }
const wchar_t* DirectionName(Direction d) { static const wchar_t* names[]{L"Down",L"Up",L"Left",L"Right"}; return names[static_cast<size_t>(d)]; }
bool Allowed(Direction p, Direction s) { return static_cast<int>(p)>=0 && static_cast<int>(p)<4 && static_cast<int>(s)>=0 && static_cast<int>(s)<4 && (static_cast<int>(p)^1)!=static_cast<int>(s); }
static bool Directory(const fs::path& p) { std::error_code ec; return fs::is_directory(p,ec); }
static std::string Read(const fs::path& p) {
    std::error_code ec; auto size=fs::file_size(p,ec); if(ec||size>8*1024*1024) return {};
    std::ifstream in(p,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}};
}
static std::wstring Reg(HKEY base,const std::wstring& key,const wchar_t* value,REGSAM view=0) {
    HKEY h=nullptr; if(RegOpenKeyExW(base,key.c_str(),0,KEY_READ|view,&h)!=ERROR_SUCCESS)return {};
    DWORD type=0,bytes=0; std::wstring result;
    if(RegQueryValueExW(h,value,nullptr,&type,nullptr,&bytes)==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&bytes<=65536){
        result.resize(bytes/sizeof(wchar_t)+1); if(RegQueryValueExW(h,value,nullptr,&type,reinterpret_cast<BYTE*>(result.data()),&bytes)==ERROR_SUCCESS)result.resize(wcsnlen_s(result.c_str(),result.size()));else result.clear();
    } RegCloseKey(h); return result;
}
static std::vector<std::wstring> Keys(HKEY base,const std::wstring& key,REGSAM view) {
    std::vector<std::wstring> result; HKEY h=nullptr; if(RegOpenKeyExW(base,key.c_str(),0,KEY_READ|view,&h)!=ERROR_SUCCESS)return result;
    for(DWORD i=0;i<20000;++i){wchar_t name[1024]{};DWORD n=1024;auto code=RegEnumKeyExW(h,i,name,&n,nullptr,nullptr,nullptr,nullptr);if(code==ERROR_NO_MORE_ITEMS)break;if(code==ERROR_SUCCESS)result.emplace_back(name,n);}RegCloseKey(h);return result;
}
static std::wstring Known(REFKNOWNFOLDERID id){PWSTR p=nullptr;std::wstring r;if(SUCCEEDED(SHGetKnownFolderPath(id,0,nullptr,&p))){r=p;CoTaskMemFree(p);}return r;}
static std::wstring ProtocolClient(const wchar_t* scheme,const wchar_t* exeName) {
    auto command=Reg(HKEY_CLASSES_ROOT,std::wstring(scheme)+L"\\shell\\open\\command",nullptr);
    // Some EA installations register an unquoted command with spaces in its path.
    auto at=elka::Key(command).find(elka::Key(exeName));if(at==command.npos)return {};
    auto path=command.substr(0,at+wcslen(exeName));if(!path.empty()&&path.front()==L'"')path.erase(0,1);
    return IsExe(path)?path:L"";
}
struct Vdf { std::wstring key,value; std::vector<Vdf> children; const Vdf* Get(const wchar_t* name)const {for(const auto& c:children)if(elka::Key(c.key)==elka::Key(name))return &c;return nullptr;} std::wstring Value(const wchar_t* name)const {auto c=Get(name);return c?c->value:L"";} };
class VdfReader {
    const std::string& data; size_t pos=0,tokens=0;
    void Space(){for(;;){while(pos<data.size()&&isspace(static_cast<unsigned char>(data[pos])))++pos;if(data.compare(pos,2,"//")!=0)break;while(pos<data.size()&&data[pos]!='\n')++pos;}}
    std::string Token(){Space();if(++tokens>100000||pos==data.size())throw std::runtime_error("VDF token");char c=data[pos++];if(c=='{'||c=='}')return {c};std::string r;if(c=='"'){bool closed=false;while(pos<data.size()){c=data[pos++];if(c=='"'){closed=true;break;}if(c=='\\'&&pos<data.size()){char next=data[pos];if(next=='\\'||next=='"'){c=next;++pos;}}r+=c;}if(!closed)throw std::runtime_error("VDF string");}else{r+=c;while(pos<data.size()&&!isspace(static_cast<unsigned char>(data[pos]))&&data[pos]!='{'&&data[pos]!='}')r+=data[pos++];}return r;}
    std::vector<Vdf> Object(int depth){if(depth>32)throw std::runtime_error("VDF nesting");std::vector<Vdf> r;for(;;){Space();if(pos==data.size()){if(depth)throw std::runtime_error("VDF truncated");return r;}auto key=Token();if(key=="}"){if(!depth)throw std::runtime_error("VDF brace");return r;}if(key=="{")throw std::runtime_error("VDF key");auto value=Token();Vdf node;node.key=FromUtf8(key);if(value=="{")node.children=Object(depth+1);else if(value=="}")throw std::runtime_error("VDF value");else node.value=FromUtf8(value);r.push_back(std::move(node));}}
public: explicit VdfReader(const std::string& text):data(text){if(data.compare(0,3,"\xef\xbb\xbf")==0)pos=3;} Vdf Parse(){return {L"",L"",Object(0)};}
};
static bool Digits(const std::wstring& s){return !s.empty()&&s.size()<=20&&s.find_first_not_of(L"0123456789")==s.npos;}
static bool Identifier(const std::wstring& s){return !s.empty()&&s.size()<160&&s.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:_.-")==s.npos;}
static void Add(Library& lib,Game game) {if(game.name.empty()||game.target.path.empty()||!Directory(game.folder))return;for(const auto& old:lib.games)if(old.Key()==game.Key())return;game.target.skipRunning=false;lib.games.push_back(std::move(game));}
void DiscoverSteam(const fs::path& root,Library& lib) {
    auto exe=(root/L"steam.exe").wstring();if(!IsExe(exe))return;lib.clients[0]=exe;std::set<fs::path> roots{root};
    auto data=Read(root/L"steamapps"/L"libraryfolders.vdf");
    if(!data.empty())try{auto v=VdfReader(data).Parse();auto folders=v.Get(L"libraryfolders");if(folders)for(const auto& f:folders->children){auto path=f.Value(L"path");if(path.empty()&&Digits(f.key))path=f.value;if(!path.empty()&&fs::path(path).is_absolute())roots.insert(path);}}catch(...){lib.notes.push_back(L"Steam library list could not be read; the default library is still checked.");}
    for(const auto& folder:roots){std::error_code ec;fs::directory_iterator it(folder/L"steamapps",ec),end;for(;!ec&&it!=end;it.increment(ec)){
        auto file=it->path();if(file.extension()!=L".acf"||file.filename().wstring().rfind(L"appmanifest_",0)!=0)continue;
        try{auto text=Read(file);auto doc=VdfReader(text).Parse();auto app=doc.Get(L"AppState");if(!app)continue;
            auto id=app->Value(L"appid"),name=app->Value(L"name"),dir=app->Value(L"installdir"),flags=app->Value(L"StateFlags");
            if(!Digits(id)||!Digits(flags)||!(std::stoul(flags)&4)||id==L"228980"||dir.empty()||dir==L"."||dir==L".."||dir.find_first_of(L"/\\:")!=dir.npos)continue;
            Add(lib,{Store::Steam,id,name,(folder/L"steamapps"/L"common"/dir).wstring(),{exe,L"-applaunch "+id,L"",false}});
        }catch(...){lib.notes.push_back(L"Skipped an unreadable Steam manifest: "+file.filename().wstring());}
    }}
}
std::vector<std::wstring> EaContentIds(const std::string& xml){
    std::vector<std::wstring> ids;size_t p=0;while((p=xml.find("<contentID>",p))!=xml.npos){p+=11;auto end=xml.find("</contentID>",p);if(end==xml.npos)break;auto id=FromUtf8(xml.substr(p,end-p));if(Identifier(id)&&std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);p=end+12;if(ids.size()>100)throw std::runtime_error("EA identifiers");}return ids;
}
static void AddEa(Library& lib,const std::wstring& path,const std::wstring& name){
    if(lib.clients[1].empty()||!Directory(path))return;try{auto ids=EaContentIds(Read(fs::path(path)/L"__Installer"/L"installerdata.xml"));if(ids.empty())return;std::wstring joined;for(const auto& id:ids){if(!joined.empty())joined+=L",";joined+=id;}
    Add(lib,{Store::EA,ids.front(),name,path,{lib.clients[1],L"\"origin2://game/launch?offerIds="+joined+L"\"",L"",false}});}catch(...){lib.notes.push_back(L"Could not read EA installation data for "+name);}
}
struct ProtoField { unsigned id; std::string bytes; };
static std::vector<ProtoField> Proto(const std::string& data){
    size_t at=0;auto number=[&](){uint64_t n=0;for(int i=0;i<10;++i){if(at==data.size())throw std::runtime_error("Truncated protobuf");auto b=static_cast<unsigned char>(data[at++]);if(i==9&&b>1)throw std::runtime_error("Protobuf integer");n|=uint64_t(b&127)<<(7*i);if(!(b&128))return n;}throw std::runtime_error("Protobuf integer");};
    std::vector<ProtoField> fields;while(at<data.size()){auto tag=number();if(!(tag>>3)||fields.size()>10000)throw std::runtime_error("Protobuf field");switch(tag&7){case 0:number();break;case 1:if(data.size()-at<8)throw std::runtime_error("Truncated protobuf");at+=8;break;case 5:if(data.size()-at<4)throw std::runtime_error("Truncated protobuf");at+=4;break;case 2:{auto n=number();if(n>data.size()-at)throw std::runtime_error("Protobuf size");fields.push_back({static_cast<unsigned>(tag>>3),data.substr(at,static_cast<size_t>(n))});at+=static_cast<size_t>(n);break;}default:throw std::runtime_error("Protobuf wire type");}}return fields;
}
std::vector<BattleInstall> ReadBattleDatabase(const std::string& data){std::vector<BattleInstall> r;for(const auto& f:Proto(data))if(f.id==1){BattleInstall p;for(const auto& v:Proto(f.bytes)){if(v.id==1)p.id=FromUtf8(v.bytes);if(v.id==2)p.product=FromUtf8(v.bytes);if(v.id==3)for(const auto& path:Proto(v.bytes))if(path.id==1)p.path=FromUtf8(path.bytes);}if(!p.id.empty()&&!p.path.empty())r.push_back(std::move(p));}return r;}
static void AddBattle(Library& lib,std::wstring id,std::wstring name,const std::wstring& path){
    if(lib.clients[2].empty())return;struct Map{const wchar_t* internal;const wchar_t* launch;const wchar_t* name;};
    static const Map products[]={{L"wow",L"WoW",L"World of Warcraft"},{L"diablo3",L"D3",L"Diablo III"},{L"s2",L"S2",L"StarCraft II"},{L"s1",L"S1",L"StarCraft"},{L"hs_beta",L"WTCG",L"Hearthstone"},{L"heroes",L"Hero",L"Heroes of the Storm"},{L"prometheus",L"Pro",L"Overwatch"},{L"w3",L"W3",L"Warcraft III"},{L"osi",L"OSI",L"Diablo II: Resurrected"},{L"fen",L"Fen",L"Diablo IV"},{L"anbs",L"ANBS",L"Diablo Immortal"},{L"auks",L"AUKS",L"Call of Duty"},{L"viper",L"VIPR",L"Call of Duty: Black Ops 4"},{L"odin",L"ODIN",L"Call of Duty: Modern Warfare"},{L"zeus",L"ZEUS",L"Call of Duty: Black Ops Cold War"},{L"fore",L"FORE",L"Call of Duty: Vanguard"},{L"w1r",L"W1R",L"Warcraft Remastered"},{L"w2r",L"W2R",L"Warcraft II Remastered"}};
    for(const auto& p:products)if(elka::Key(id)==p.internal){if(name.empty())name=p.name;Add(lib,{Store::BattleNet,id,name,path,{lib.clients[2],L"--exec=\"launch "+std::wstring(p.launch)+L"\"",L"",false}});return;}
    if(id!=L"battle.net"&&id!=L"agent"&&id!=L"bna"&&id!=L"bng")lib.notes.push_back(L"Unrecognized Battle.net product '"+id+L"'. Add its launcher command manually.");
}
Library Discover(){
    Library lib;auto steam=Reg(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath");if(steam.empty())steam=Reg(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Valve\\Steam",L"InstallPath",KEY_WOW64_32KEY);if(steam.empty())steam=(fs::path(Known(FOLDERID_ProgramFilesX86))/L"Steam").wstring();DiscoverSteam(steam,lib);
    lib.clients[1]=ProtocolClient(L"origin2",L"EALauncher.exe");lib.clients[2]=ProtocolClient(L"battlenet",L"Battle.net.exe");
    const std::wstring uninstall=L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for(auto base:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER})for(auto view:{KEY_WOW64_32KEY,KEY_WOW64_64KEY})for(const auto& key:Keys(base,uninstall,view)){
        auto entry=uninstall+L"\\"+key,name=Reg(base,entry,L"DisplayName",view),path=Reg(base,entry,L"InstallLocation",view),command=Reg(base,entry,L"UninstallString",view);
        if(!path.empty()){AddEa(lib,path,name);auto at=command.find(L"--uid=");if(at!=command.npos&&elka::Key(command).find(L"battle.net")!=command.npos){auto id=command.substr(at+6);id=id.substr(0,id.find_first_of(L" \"\t"));AddBattle(lib,id,name,path);}}
    }
    const std::wstring ea=L"SOFTWARE\\EA Games";for(const auto& k:Keys(HKEY_LOCAL_MACHINE,ea,KEY_WOW64_32KEY)){auto entry=ea+L"\\"+k;AddEa(lib,Reg(HKEY_LOCAL_MACHINE,entry,L"Install Dir",KEY_WOW64_32KEY),Reg(HKEY_LOCAL_MACHINE,entry,L"DisplayName",KEY_WOW64_32KEY));}
    auto db=Read(fs::path(Known(FOLDERID_ProgramData))/L"Battle.net"/L"Agent"/L"product.db");if(!db.empty())try{for(const auto& p:ReadBattleDatabase(db))AddBattle(lib,p.id,L"",p.path);}catch(...){lib.notes.push_back(L"Battle.net installation database could not be read. Registry installations are still available.");}
    std::sort(lib.games.begin(),lib.games.end(),[](const Game&a,const Game&b){return a.store==b.store?elka::Key(a.name)<elka::Key(b.name):a.store<b.store;});return lib;
}
std::string Encode(const State& s){std::ostringstream o;o<<"ELKA-GAMES 2\n"<<s.enabled<<' '<<s.topmost<<' '<<s.locked<<' '<<static_cast<int>(s.direction)<<' '<<s.x<<' '<<s.y<<' '<<std::quoted(ToUtf8(s.screen))<<' '<<s.toolbarVisible<<'\n';for(auto d:s.groupDirections)o<<static_cast<int>(d)<<' ';o<<'\n'<<s.favorites.size()<<'\n';for(const auto& f:s.favorites)o<<std::quoted(ToUtf8(f))<<'\n';o<<s.custom.size()<<'\n';for(const auto& g:s.custom)o<<static_cast<int>(g.store)<<' '<<std::quoted(ToUtf8(g.id))<<' '<<std::quoted(ToUtf8(g.name))<<' '<<std::quoted(ToUtf8(g.target.path))<<' '<<std::quoted(ToUtf8(g.target.arguments))<<'\n';return o.str();}
bool Decode(const std::string& text,State& out,std::wstring& error){try{if(text.size()>1024*1024)throw std::runtime_error("Size");std::istringstream in(text);std::string magic,screen;int version=0,e=0,t=0,l=0,d=0;State s;if(!(in>>magic>>version)||magic!="ELKA-GAMES"||(version!=1&&version!=2))throw std::runtime_error("Version");if(!(in>>e>>t>>l>>d>>s.x>>s.y>>std::quoted(screen))||(e&~1)||(t&~1)||(l&~1)||d<0||d>3||s.x< -1||s.y< -1||s.x>100000||s.y>100000)throw std::runtime_error("Options");s.enabled=e!=0;s.topmost=t!=0;s.locked=l!=0;s.direction=static_cast<Direction>(d);s.screen=FromUtf8(screen);if(version==2){int visible=0;if(!(in>>visible)||(visible&~1))throw std::runtime_error("Toolbar");s.toolbarVisible=visible!=0;}for(auto& dir:s.groupDirections){if(!(in>>d)||d<0||d>3)throw std::runtime_error("Direction");dir=static_cast<Direction>(d);if(!Allowed(s.direction,dir))dir=s.direction;}
    size_t count=0;if(!(in>>count)||count>5000)throw std::runtime_error("Favorites");for(size_t i=0;i<count;++i){std::string f;if(!(in>>std::quoted(f))||f.empty()||f.size()>256)throw std::runtime_error("Favorite");s.favorites.insert(FromUtf8(f));}
    if(!(in>>count)||count>1000)throw std::runtime_error("Custom games");std::set<std::wstring> keys;for(size_t i=0;i<count;++i){Game g;int store=0;std::string id,name,path,args;if(!(in>>store>>std::quoted(id)>>std::quoted(name)>>std::quoted(path)>>std::quoted(args))||store<0||store>3||id.empty()||id.size()>128||name.empty()||name.size()>512||path.empty()||path.size()>32000||args.size()>16000)throw std::runtime_error("Game");g.store=static_cast<Store>(store);g.id=FromUtf8(id);g.name=FromUtf8(name);g.target={FromUtf8(path),FromUtf8(args),L"",false};if(!keys.insert(g.Key()).second)throw std::runtime_error("Duplicate game");s.custom.push_back(std::move(g));}in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing data");out=std::move(s);error.clear();return true;
    }catch(...){error=L"Game launcher settings could not be read. The original launcher.elka file has been kept.";return false;}}
bool Load(const std::wstring& dir,State& out,std::wstring& error){std::error_code ec;auto file=fs::path(dir)/L"launcher.elka";if(!fs::exists(file,ec)&&!ec){out=State{};error.clear();return true;}return Decode(Read(file),out,error);}
bool Save(const std::wstring& dir,const State& s,std::wstring& error){auto text=Encode(s);State checked;if(!Decode(text,checked,error))return false;std::error_code ec;fs::create_directories(dir,ec);if(ec){error=L"Could not create launcher settings folder.";return false;}auto path=fs::path(dir)/L"launcher.elka",temp=fs::path(dir)/L"launcher.elka.tmp",backup=fs::path(dir)/L"launcher.elka.bak";{std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();if(!file){error=L"Could not write launcher settings.";return false;}}if(fs::exists(path,ec)&&!CopyFileW(path.c_str(),backup.c_str(),FALSE)){error=ErrorText(GetLastError());return false;}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=ErrorText(GetLastError());return false;}error.clear();return true;}
Placement LayoutDrawer(RECT widget,RECT tile,RECT work,int count,Direction primary,Direction secondary,UINT dpi){
    if(!Allowed(primary,secondary))secondary=primary;auto scale=[&](int n){return MulDiv(n,static_cast<int>(dpi),96);};const int cw=scale(138),ch=scale(80),inset=scale(9),header=scale(40),gap=scale(10),footer=scale(38);int aw=work.right-work.left,ah=work.bottom-work.top;
    switch(primary){case Direction::Up:ah=widget.top-gap-work.top;break;case Direction::Down:ah=work.bottom-widget.bottom-gap;break;case Direction::Left:aw=widget.left-gap-work.left;break;case Direction::Right:aw=work.right-widget.right-gap;break;}
    if(primary==Direction::Up||primary==Direction::Down){if(secondary==Direction::Left)aw=tile.right+scale(12)-work.left;if(secondary==Direction::Right)aw=work.right-tile.left+scale(12);}else{if(secondary==Direction::Up)ah=tile.bottom+scale(12)-work.top;if(secondary==Direction::Down)ah=work.bottom-tile.top+header+scale(12);}
    int maxC=(std::max)(1,((std::min)(aw,int(work.right-work.left))-2*inset)/cw),maxR=(std::max)(1,((std::min)(ah,int(work.bottom-work.top))-header-footer-2*inset)/ch);
    // Paginate oversized groups instead of creating off-screen controls.
    Placement p;p.capacity=maxC*maxR;int cells=std::clamp(count,1,p.capacity);bool across=secondary==Direction::Left||secondary==Direction::Right;p.columns=across?(std::min)(cells,maxC):(cells+(std::min)(cells,maxR)-1)/(std::min)(cells,maxR);p.rows=across?(cells+p.columns-1)/p.columns:(std::min)(cells,maxR);
    for(int i=0;i<(std::min)(count,p.capacity);++i){int col=across?i%p.columns:i/p.rows,row=across?i/p.columns:i%p.rows;if(secondary==Direction::Left||(primary==Direction::Left&&!across))col=p.columns-1-col;if(secondary==Direction::Up||(primary==Direction::Up&&across))row=p.rows-1-row;p.cells.push_back({inset+col*cw,header+inset+row*ch});}
    int width=(std::min)(p.columns*cw+2*inset,int(work.right-work.left)),height=(std::min)(p.rows*ch+header+footer+2*inset,int(work.bottom-work.top));POINT first=p.cells.empty()?POINT{inset,header+inset}:p.cells[0];int x=tile.left-first.x,y=tile.top-first.y;
    switch(primary){case Direction::Up:y=widget.top-gap-height;break;case Direction::Down:y=widget.bottom+gap;break;case Direction::Left:x=widget.left-gap-width;break;case Direction::Right:x=widget.right+gap;break;}
    x=std::clamp(x,int(work.left),(std::max)(int(work.left),int(work.right)-width));y=std::clamp(y,int(work.top),(std::max)(int(work.top),int(work.bottom)-height));p.bounds={x,y,x+width,y+height};return p;
}
}
