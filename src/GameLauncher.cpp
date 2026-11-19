#include "GameLauncher.h"
#include "Ui.h"
#include "IconArtwork.h"
#include <algorithm>
#include <thread>
#include <mutex>
#include <map>
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

namespace elka {
using namespace games;
namespace {
constexpr UINT Scanned=WM_APP+61,LibraryChanged=WM_APP+62;
constexpr int TileBase=1000;
struct MenuChoice { UINT id; std::wstring label; bool checked=false; };
LRESULT CALLBACK DockContext(HWND child,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(message==WM_CONTEXTMENU){SendMessageW(GetParent(child),message,w,l);return 0;}
    if(message==WM_NCDESTROY)RemoveWindowSubclass(child,DockContext,1);
    return DefSubclassProc(child,message,w,l);
}
struct Screen { std::wstring name; RECT work{}; bool primary=false; };
std::vector<Screen> Screens(){std::vector<Screen> r;EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM p)->BOOL{MONITORINFOEXW i{};i.cbSize=sizeof(i);if(GetMonitorInfoW(m,&i))reinterpret_cast<std::vector<Screen>*>(p)->push_back({i.szDevice,i.rcWork,(i.dwFlags&MONITORINFOF_PRIMARY)!=0});return TRUE;},reinterpret_cast<LPARAM>(&r));return r;}
Screen CurrentScreen(const std::wstring& name){auto all=Screens();for(const auto& s:all)if(s.name==name)return s;for(const auto& s:all)if(s.primary)return s;return all.empty()?Screen{L"",{0,0,1280,720},true}:all.front();}
void Popup(ui::Window& w,const wchar_t* caption,int width,int height){w.Create(caption,width,height);SetWindowLongPtrW(w.hwnd,GWL_STYLE,WS_POPUP|WS_CLIPCHILDREN);SetWindowLongPtrW(w.hwnd,GWL_EXSTYLE,WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT);SetWindowPos(w.hwnd,nullptr,0,0,w.S(width),w.S(height),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);}
void Tile(ui::Window& w,DRAWITEMSTRUCT* d,const std::wstring& name,const std::wstring& detail,COLORREF color,ui::IconArtwork* artwork=nullptr){
    ui::DrawLauncherTile(d->hDC,d->rcItem,name,detail,color,w.font,w.bold,w.dpi,d->itemState,artwork);
}
void MenuItem(ui::Window& window,DRAWITEMSTRUCT* d){
    auto choice=reinterpret_cast<const MenuChoice*>(d->itemData);if(!choice)return;
    window.Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);auto r=d->rcItem;r.left+=window.S(12);r.right=r.left+window.S(22);
    if(choice->checked)window.TextAt(d->hDC,L"✓",r,ui::Accent,window.bold);r=d->rcItem;r.left+=window.S(38);r.right-=window.S(12);window.TextAt(d->hDC,choice->label,r);
}
void ContextMenu(ui::Window& window,POINT point,const std::vector<MenuChoice>& choices){
    if(point.x==-1&&point.y==-1){RECT r{};HWND focus=GetFocus();GetWindowRect(IsChild(window.hwnd,focus)?focus:window.hwnd,&r);point={r.left+window.S(12),r.top+window.S(12)};}
    HMENU menu=CreatePopupMenu();if(!menu)return;
    for(const auto& choice:choices)AppendMenuW(menu,MF_OWNERDRAW|(choice.checked?MF_CHECKED:0),choice.id,reinterpret_cast<LPCWSTR>(&choice));
    HBRUSH brush=CreateSolidBrush(ui::Panel);MENUINFO info{sizeof(info)};info.fMask=MIM_BACKGROUND;info.hbrBack=brush;SetMenuInfo(menu,&info);SetForegroundWindow(window.hwnd);
    auto selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window.hwnd,nullptr);
    DestroyMenu(menu);DeleteObject(brush);PostMessageW(window.hwnd,WM_NULL,0,0);if(selected)window.Command(selected,BN_CLICKED);
}
struct ColorWindow:ui::Window {
    COLORREF color;std::wstring name;bool updating=false;
    static constexpr COLORREF palette[]={RGB(100,181,246),RGB(255,144,102),RGB(255,105,112),RGB(77,218,191),RGB(192,145,255),RGB(245,209,99),RGB(239,145,207),RGB(230,237,243)};
    ColorWindow(COLORREF value,std::wstring label):color(value),name(std::move(label)){}
    void Created()override{Label(L"Choose an accent color",20);for(int i=0;i<8;++i){auto button=Button(ColorText(palette[i]),40+i);SetPropW(button,L"customDraw",reinterpret_cast<HANDLE>(1));}Label(L"Custom color (#RRGGBB)",21);Edit(ColorText(color),22);SendMessageW(Get(22),EM_SETLIMITTEXT,7,0);Button(L"Cancel",IDCANCEL);Button(L"Use color",IDOK,true);}
    void Layout(int w,int h)override{Place(20,24,18,w-48,24);for(int i=0;i<8;++i)Place(40+i,24+(i%4)*106,54+(i/4)*40,96,32);Place(21,24,148,210,24);Place(22,244,144,194,30);Place(IDCANCEL,w-264,h-56,112,34);Place(IDOK,w-140,h-56,116,34);}
    void Paint(HDC dc,RECT)override{ui::DrawLauncherTile(dc,{S(24),S(194),S(438),S(290)},name,L"Favorites",color,font,bold,dpi,0);}
    void Item(DRAWITEMSTRUCT* d)override{int i=static_cast<int>(d->CtlID)-40;if(i<0||i>=8)return;Box(d->hDC,d->rcItem,ui::Background);Box(d->hDC,d->rcItem,palette[i],8);TextAt(d->hDC,ColorText(palette[i]),d->rcItem,ui::Background,font,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS){auto r=d->rcItem;InflateRect(&r,-S(3),-S(3));DrawFocusRect(d->hDC,&r);}}
    void Command(int id,int code)override{if(id==IDCANCEL){End(0);return;}if(id>=40&&id<48){color=palette[id-40];updating=true;Value(22,ColorText(color));updating=false;InvalidateRect(hwnd,nullptr,TRUE);}if(id==22&&code==EN_CHANGE&&!updating){COLORREF value;if(ParseColor(Value(22),value)){color=value;InvalidateRect(hwnd,nullptr,TRUE);}}if(id==IDOK){if(!ParseColor(Value(22),color)){ui::Notice(hwnd,L"Color",L"Enter six color digits, for example #64B5F6.");return;}End(IDOK);}}
};
}
struct GameLauncher::Impl {
    struct Widget;struct Drawer;struct Browser;struct OptionsWindow;
    HWND owner=nullptr,browser=nullptr;std::wstring directory;Log log;State state;games::Library library;
    struct CachedIcon { std::unique_ptr<ui::IconArtwork> image; uint64_t used=0; };
    std::map<std::wstring,CachedIcon> gameArtwork;uint64_t iconUse=0;
    ui::IconArtwork* GameIcon(const Game& game){
        auto key=game.Key();auto found=gameArtwork.find(key);
        if(found==gameArtwork.end()){
            if(gameArtwork.size()>=32){auto oldest=std::min_element(gameArtwork.begin(),gameArtwork.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});gameArtwork.erase(oldest);}
            auto image=std::make_unique<ui::IconArtwork>();auto file=FindGameIcon(game);
            image->Load(file,game.iconIndex,128);found=gameArtwork.emplace(key,CachedIcon{std::move(image),0}).first;
        }
        found->second.used=++iconUse;return found->second.image.get();
    }
    std::unique_ptr<Widget> widget;std::unique_ptr<Drawer> drawer;std::array<ui::IconArtwork,StoreCount> artwork;
    std::thread scanner;std::mutex scanMutex;games::Library scanResult;bool scanning=false,scanned=false,placing=false,closing=false;int expanded=-1,page=0;
    Impl(HWND h,std::wstring d,Log l);~Impl();
    bool Persist(){std::wstring error;if(!games::Save(directory,state,error)){ui::Notice(owner,L"Game launcher settings",error);return false;}return true;}
    void Refresh();void ScanDone();void PlaceWidget();void Apply();void Expand(int store,bool animate=true);void Collapse();void Options(HWND parent);void Browse(HWND parent,int store=0);std::optional<Target> Pick(HWND parent);void LaunchGame(const Game& game);
    std::vector<Game> All()const{auto list=library.games;for(const auto& g:state.custom)list.push_back(g);std::sort(list.begin(),list.end(),[](const Game&a,const Game&b){return a.store==b.store?elka::Key(a.name)<elka::Key(b.name):a.store<b.store;});return list;}
    std::vector<Game> Favorites(int store)const{std::vector<Game> r;for(const auto& g:All())if(static_cast<int>(g.store)==store&&state.favorites.count(g.Key()))r.push_back(g);return r;}
    bool Favorite(const Game& g){auto before=state.favorites;if(state.favorites.count(g.Key()))state.favorites.erase(g.Key());else state.favorites.insert(g.Key());if(!Persist()){state.favorites=before;return false;}Apply();return true;}
};
struct GameLauncher::Impl::Widget:ui::Window {
    Impl& app;explicit Widget(Impl& a):app(a){}
    void Created()override{
        Button(L"Library",10);Button(L"Options",11);Button(L"Hide",12);Button(L"Lock",13);Button(L"Always on top",14);
        for(int i=0;i<4;++i){auto h=Button(StoreName(static_cast<Store>(i)),TileBase+i);SetPropW(h,L"customDraw",reinterpret_cast<HANDLE>(1));}
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SetWindowSubclass(child,DockContext,1,0);
    }
    void Layout(int,int)override{
        Place(10,12,11,64,28);Place(11,82,11,64,28);Place(13,152,11,58,28);Place(14,216,11,106,28);Place(12,328,11,54,28);
        for(int id:{10,11,12,13,14})Show(id,app.state.toolbarVisible);
        Value(13,app.state.locked?L"Locked":L"Lock");
        SetPropW(Get(13),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(app.state.locked)));
        SetPropW(Get(14),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(app.state.topmost)));
        InvalidateRect(Get(13),nullptr,TRUE);InvalidateRect(Get(14),nullptr,TRUE);
        for(int i=0;i<4;++i){Place(TileBase+i,12+i*138,app.state.toolbarVisible?50:12,132,96);Show(TileBase+i,i<3||!app.state.custom.empty());}
    }
    void Paint(HDC dc,RECT r)override{
        if(app.state.locked)return;
        int x=app.state.toolbarVisible?r.right-S(32):r.right/2-S(10),y=S(app.state.toolbarVisible?22:4);
        Box(dc,{x,y,x+S(20),y+S(1)},ui::Muted);Box(dc,{x,y+S(4),x+S(20),y+S(5)},ui::Muted);
    }
    void Item(DRAWITEMSTRUCT* d)override{
        if(d->CtlType==ODT_MENU){MenuItem(*this,d);return;}
        int i=static_cast<int>(d->CtlID)-TileBase;if(i<0||i>3)return;auto count=app.Favorites(i).size();Tile(*this,d,StoreName(static_cast<Store>(i)),app.scanning?L"Scanning…":std::to_wstring(count)+L" favorites",app.state.colors[i],&app.artwork[i]);
    }
    void Context(POINT point){ContextMenu(*this,point,{{10,L"Library"},{11,L"Layout & colors"},{13,L"Lock position",app.state.locked},{14,L"Always on top",app.state.topmost},{15,app.state.toolbarVisible?L"Hide button row":L"Show button row"},{12,L"Hide launcher"}});}
    void Command(int id,int)override{
        if(id>=TileBase&&id<TileBase+4){if(app.expanded==id-TileBase)app.Collapse();else app.Expand(id-TileBase);return;}
        if(id==10)app.Browse(hwnd);if(id==11)app.Options(hwnd);if(id==12)Close();
        if(id==13||id==14||id==15){auto before=app.state;if(id==13)app.state.locked=!app.state.locked;if(id==14)app.state.topmost=!app.state.topmost;if(id==15)app.state.toolbarVisible=!app.state.toolbarVisible;if(!app.Persist()){app.state=before;return;}app.Apply();if(id==15&&!app.state.toolbarVisible)SetFocus(Get(TileBase));}
    }
    void Close()override{app.Collapse();ShowWindow(hwnd,SW_HIDE);}
    LRESULT Message(UINT m,WPARAM,LPARAM l,bool& handled)override{
        handled=true;if(m==Scanned){app.ScanDone();return 0;}
        if(m==WM_CONTEXTMENU||m==WM_NCRBUTTONUP){Context({GET_X_LPARAM(l),GET_Y_LPARAM(l)});return 0;}
        if(m==WM_NCHITTEST&&!app.state.locked){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd,&p);if(app.state.toolbarVisible?(p.y<S(45)&&p.x>S(388)):p.y<S(10))return HTCAPTION;handled=false;return 0;}
        if(m==WM_ENTERSIZEMOVE){app.Collapse();return 0;}
        if(m==WM_EXITSIZEMOVE){MONITORINFOEXW mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);RECT r{};GetWindowRect(hwnd,&r);app.state.screen=mi.szDevice;app.state.x=(std::max)(0,MulDiv(r.left-mi.rcWork.left,96,dpi));app.state.y=(std::max)(0,MulDiv(r.top-mi.rcWork.top,96,dpi));app.PlaceWidget();app.Persist();return 0;}
        if(m==WM_DISPLAYCHANGE){app.PlaceWidget();app.Collapse();return 0;}
        handled=false;return 0;
    }
};
struct GameLauncher::Impl::Drawer:ui::Window {
    Impl& app;std::vector<Game> games;games::Placement placement;int store=0;explicit Drawer(Impl&a):app(a){}
    bool Controls()const{return app.state.drawerToolbarVisible[store];}
    int Pages()const{return (std::max)(1,(static_cast<int>(games.size())+placement.capacity-1)/placement.capacity);}
    RECT EmptyNode()const{return {S(9),S(Controls()?49:9),S(141),S(Controls()?123:83)};}
    void Created()override{Button(L"+",10);Button(L"×",11);Button(L"‹",12);Button(L"›",13);Button(L"↳",14);for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SetWindowSubclass(child,DockContext,1,0);}
    void Set(int s,std::vector<Game> list,games::Placement p){
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;){HWND next=GetWindow(child,GW_HWNDNEXT);if(GetDlgCtrlID(child)>=TileBase)DestroyWindow(child);child=next;}
        store=s;games=std::move(list);placement=std::move(p);static const wchar_t* arrows[]{L"↓",L"↑",L"←",L"→"};Value(14,arrows[static_cast<size_t>(app.state.groupDirections[store])]);app.page=std::clamp(app.page,0,Pages()-1);
        int begin=app.page*placement.capacity,end=(std::min)(static_cast<int>(games.size()),begin+placement.capacity);for(int i=begin;i<end;++i){auto h=Button(games[i].name,TileBase+i-begin);SetPropW(h,L"customDraw",reinterpret_cast<HANDLE>(1));SetWindowSubclass(h,DockContext,1,0);}
        const auto r=placement.bounds;SetWindowPos(hwnd,app.state.topmost?HWND_TOPMOST:HWND_NOTOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);RECT client{};GetClientRect(hwnd,&client);Layout(MulDiv(client.right,96,dpi),MulDiv(client.bottom,96,dpi));InvalidateRect(hwnd,nullptr,TRUE);
    }
    void Layout(int w,int h)override{
        Place(10,w-98,6,28,28);Place(14,w-66,6,28,28);Place(11,w-34,6,28,28);Place(12,w-76,h-34,30,27);Place(13,w-40,h-34,30,27);
        for(int id:{10,11,12,13,14})Show(id,Controls());
        std::vector<RECT> surfaces;if(Controls()){surfaces.push_back({0,0,S(w),S(40)});surfaces.push_back({0,S(h-38),S(w),S(h)});}
        for(size_t i=0;i<placement.cells.size();++i){const auto p=placement.cells[i];if(auto child=Get(TileBase+static_cast<int>(i))){MoveWindow(child,p.x,p.y,S(132),S(74),TRUE);surfaces.push_back({p.x,p.y,p.x+S(132),p.y+S(74)});}}
        if(games.empty())surfaces.push_back(EmptyNode());
        // The gaps are outside the window itself, so the desktop shows through.
        if(auto region=ui::CreateNodeRegion(surfaces,S(10)))if(!SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);
        EnableWindow(Get(12),app.page>0);EnableWindow(Get(13),app.page+1<Pages());
    }
    void Paint(HDC dc,RECT r)override{
        if(Controls()){TextAt(dc,StoreName(static_cast<Store>(store)),{S(8),S(6),r.right-S(100),S(34)},app.state.colors[store],bold);TextAt(dc,L"Page "+std::to_wstring(app.page+1)+L" / "+std::to_wstring(Pages()),{S(12),r.bottom-S(34),r.right-S(82),r.bottom-S(5)},ui::Muted);}
        if(games.empty())ui::DrawLauncherTile(dc,EmptyNode(),L"No favorites",L"Right-click to add",app.state.colors[store],font,bold,dpi,0);
    }
    void Item(DRAWITEMSTRUCT* d)override{if(d->CtlType==ODT_MENU){MenuItem(*this,d);return;}auto index=app.page*placement.capacity+static_cast<int>(d->CtlID)-TileBase;if(index>=0&&index<static_cast<int>(games.size()))Tile(*this,d,games[index].name,L"Play",app.state.colors[store],app.GameIcon(games[index]));}
    void Context(POINT point){std::vector<MenuChoice> choices{{10,L"Library / add favorites"},{15,Controls()?L"Hide button row":L"Show button row"},{14,L"Flow: "+std::wstring(DirectionName(app.state.groupDirections[store]))},{16,L"Layout & colors"}};if(Pages()>1){if(app.page>0)choices.push_back({12,L"Previous page"});if(app.page+1<Pages())choices.push_back({13,L"Next page"});}choices.push_back({11,L"Close favorites"});ContextMenu(*this,point,choices);}
    void Command(int id,int)override{
        if(id>=TileBase){auto index=app.page*placement.capacity+id-TileBase;if(index>=0&&index<static_cast<int>(games.size())){auto game=games[index];app.LaunchGame(game);}return;}
        if(id==10)app.Browse(hwnd,store);if(id==11)app.Collapse();if(id==16)app.Options(hwnd);if(id==12||id==13){app.page+=id==12?-1:1;app.Expand(store,false);}
        if(id==14||id==15){auto before=app.state;if(id==14){auto& d=app.state.groupDirections[store];do{d=static_cast<Direction>((static_cast<int>(d)+1)%4);}while(!Allowed(app.state.direction,d));}else app.state.drawerToolbarVisible[store]=!Controls();if(!app.Persist()){app.state=before;return;}app.Expand(store,id==14);if(auto first=Get(TileBase))SetFocus(first);else SetFocus(hwnd);}
    }
    void Close()override{app.Collapse();}
    LRESULT Message(UINT msg,WPARAM w,LPARAM l,bool& handled)override{handled=true;if(msg==WM_KEYDOWN&&w==VK_ESCAPE){app.Collapse();return 0;}if(msg==WM_CONTEXTMENU){Context({GET_X_LPARAM(l),GET_Y_LPARAM(l)});return 0;}handled=false;return 0;}
};
struct ManualGame:ui::Window {
    Game game;const games::Library& library;
    ManualGame(Store store,const games::Library& lib):library(lib){game.store=store;game.id=NewId();game.target.skipRunning=false;}
    void Created()override{Label(L"Name",20);Edit(L"",21);Label(L"Application",22);Edit(L"",23,true);Button(L"Browse EXE",24);Label(L"Arguments (Steam: enter just the numeric AppID)",25);Edit(L"",26);Button(L"Cancel",IDCANCEL);Button(L"Add game",IDOK,true);auto client=library.clients[static_cast<size_t>(game.store)];Value(23,client);}
    void Layout(int w,int)override{Place(20,24,20,w-48,24);Place(21,24,48,w-48,30);Place(22,24,96,w-48,24);Place(23,24,124,w-184,30);Place(24,w-148,122,124,34);Place(25,24,176,w-48,24);Place(26,24,204,w-48,30);Place(IDCANCEL,w-264,270,112,34);Place(IDOK,w-140,270,116,34);}
    void Command(int id,int)override{if(id==IDCANCEL)End(0);if(id==24){auto p=ui::PickExe(hwnd,Value(23));if(!p.empty())Value(23,p);}if(id==IDOK){game.name=Value(21);game.target.path=Value(23);game.target.arguments=Value(26);if(game.name.empty()||game.name.size()>120||!IsExe(game.target.path)){ui::Notice(hwnd,L"Game details",L"Enter a name and choose an existing launcher or game EXE.");return;}if(game.store==Store::Steam&&game.target.arguments.find_first_not_of(L"0123456789")==std::wstring::npos&&!game.target.arguments.empty())game.target.arguments=L"-applaunch "+game.target.arguments;End(IDOK);}}
};
struct GameLauncher::Impl::Browser:ui::Window {
    Impl& app;std::vector<Game> visible;int store=0;bool picking=false,changing=false;std::optional<Target> picked;
    Browser(Impl&a,int s,bool pick=false):app(a),store(s),picking(pick){}
    void Created()override{for(int i=0;i<4;++i)Button(StoreName(static_cast<Store>(i)),30+i);Edit(L"",20);Label(L"Filter installed games",21);List(22,60);Button(L"Refresh",23);Button(L"★ Favorite",24,true);Button(picking?L"Add target":L"Play",25);Button(L"Add manually",26);Button(L"Done",IDCANCEL);Button(L"Remove custom",27);Label(L"",28);Reload();}
    void Layout(int w,int h)override{for(int i=0;i<4;++i)Place(30+i,24+i*150,20,140,40);Place(21,24,77,180,22);Place(20,204,72,w-340,30);Place(23,w-120,72,96,32);Place(22,24,120,w-48,h-256);Place(28,24,h-126,w-48,46);Place(24,24,h-68,144,36);Place(25,180,h-68,116,36);Place(26,308,h-68,126,36);Place(27,446,h-68,132,36);Place(IDCANCEL,w-116,h-68,92,36);}
    void Reload(){changing=true;auto selected=Selected();auto key=selected?selected->Key():L"";visible.clear();SendMessageW(Get(22),LB_RESETCONTENT,0,0);auto filter=elka::Key(Value(20));for(const auto& g:app.All())if(static_cast<int>(g.store)==store&&(filter.empty()||elka::Key(g.name+L" "+g.id).find(filter)!=std::wstring::npos)){visible.push_back(g);SendMessageW(Get(22),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(g.name.c_str()));if(g.Key()==key)SendMessageW(Get(22),LB_SETCURSEL,visible.size()-1,0);}
        if(SendMessageW(Get(22),LB_GETCURSEL,0,0)==LB_ERR&&!visible.empty())SendMessageW(Get(22),LB_SETCURSEL,0,0);
        for(int i=0;i<4;++i){SetPropW(Get(30+i),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(i==store)));InvalidateRect(Get(30+i),nullptr,TRUE);}Value(28,app.scanning?L"Reading installed games from local launcher files…":std::to_wstring(visible.size())+L" games · Select a game and choose Favorite to put it in the floating drawer.\n"+(app.library.notes.empty()?L"No sign-in or online library access is used.":app.library.notes.front()));if(store==2&&visible.empty()&&Value(20).empty()&&!app.scanning)Value(28,app.library.clients[2].empty()?L"Battle.net client not found. Install it or add a game manually.":L"Battle.net is installed; no supported installed games were found in its local records.\nUse Refresh after installing or locating a game in Battle.net, or choose Add manually.");EnableWindow(Get(23),!app.scanning);changing=false;Update();}
    std::optional<Game> Selected()const{auto i=static_cast<int>(SendMessageW(Get(22),LB_GETCURSEL,0,0));if(i>=0&&i<static_cast<int>(visible.size()))return visible[i];return {};}
    void Update(){auto g=Selected();EnableWindow(Get(24),g.has_value());EnableWindow(Get(25),g.has_value());bool custom=g&&std::any_of(app.state.custom.begin(),app.state.custom.end(),[&](const Game& c){return c.Key()==g->Key();});EnableWindow(Get(27),custom);Value(24,g&&app.state.favorites.count(g->Key())?L"★ Unfavorite":L"☆ Favorite");}
    void Item(DRAWITEMSTRUCT* d)override{if(d->itemID>=visible.size())return;const auto& g=visible[d->itemID];Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);auto r=d->rcItem;auto icon=app.GameIcon(g);if(icon->Draw(d->hDC,{r.left+S(10),r.top+S(8),r.left+S(54),r.top+S(52)},255))r.left+=S(54);r.left+=S(12);r.top+=S(5);r.right-=S(12);r.bottom=r.top+S(25);TextAt(d->hDC,(app.state.favorites.count(g.Key())?L"★  ":L"☆  ")+g.name,r,ui::Text,bold);r.top+=S(25);r.bottom+=S(24);TextAt(d->hDC,g.store==Store::Other?g.target.path:std::wstring(StoreName(g.store))+L" · "+g.id,r,ui::Muted);}
    void Command(int id,int code)override{
        if(id>=30&&id<34){store=id-30;Value(20,L"");Reload();return;}if(id==20&&code==EN_CHANGE&&!changing){Reload();return;}if(id==22&&code==LBN_SELCHANGE){Update();return;}if(id==IDCANCEL){End(0);return;}if(id==23){app.Refresh();Reload();return;}
        if(id==26){ManualGame add(static_cast<Store>(store),app.library);add.Create(L"Add a game",660,330,hwnd);if(add.Modal(hwnd)==IDOK){auto before=app.state;app.state.custom.push_back(add.game);app.state.favorites.insert(add.game.Key());if(!app.Persist())app.state=before;app.Apply();Reload();}return;}
        auto game=Selected();if(!game)return;if(id==24||(id==22&&code==LBN_DBLCLK)){app.Favorite(*game);Reload();}if(id==25){if(picking){picked=game->target;End(IDOK);}else app.LaunchGame(*game);}if(id==27){auto before=app.state;app.state.custom.erase(std::remove_if(app.state.custom.begin(),app.state.custom.end(),[&](const Game& g){return g.Key()==game->Key();}),app.state.custom.end());app.state.favorites.erase(game->Key());if(!app.Persist())app.state=before;app.Apply();Reload();}
    }
    LRESULT Message(UINT m,WPARAM,LPARAM,bool& handled)override{handled=m==LibraryChanged;if(handled)Reload();return 0;}
};
struct GameLauncher::Impl::OptionsWindow:ui::Window {
    Impl& app;State draft;OptionsWindow(Impl&a):app(a),draft(a.state){}
    void Created()override{Label(L"FLOATING SHORTCUTS · LAYOUT & COLORS",20);Button(L"",22);Check(L"Always on top",23,draft.topmost);Check(L"Lock position",24,draft.locked);Label(L"Drag the dock grip onto any screen. Its position is remembered.\nRight-click a store or game node to hide or restore its button row.\nClick a store again to close its favorites when the X is hidden.",25);Button(L"Reset position",26);for(int i=0;i<4;++i){auto button=Button(StoreName(static_cast<Store>(i)),30+i);SetPropW(button,L"customDraw",reinterpret_cast<HANDLE>(1));}Label(L"Store colors · select a tile to change its accent",27);Button(L"Cancel",IDCANCEL);Button(L"Save layout",IDOK,true);Update();}
    void Update(){Value(22,L"Expand: "+std::wstring(DirectionName(draft.direction)));for(int i=0;i<4;++i)InvalidateRect(Get(30+i),nullptr,TRUE);}
    void Layout(int w,int)override{Place(20,24,20,w-48,24);Place(27,24,60,w-48,24);for(int i=0;i<4;++i)Place(30+i,24+i*148,94,138,80);Place(22,24,198,w-48,38);Place(23,24,252,280,34);Place(24,330,252,280,34);Place(25,24,310,w-48,78);Place(26,24,412,144,34);Place(IDCANCEL,w-268,412,112,34);Place(IDOK,w-144,412,120,34);}
    void Item(DRAWITEMSTRUCT* d)override{int i=static_cast<int>(d->CtlID)-30;if(i>=0&&i<4)Tile(*this,d,StoreName(static_cast<Store>(i)),ColorText(draft.colors[i]),draft.colors[i]);}
    void Command(int id,int)override{if(id==IDCANCEL)End(0);if(id>=30&&id<34){ColorWindow picker(draft.colors[id-30],StoreName(static_cast<Store>(id-30)));picker.Create(L"Store accent color",462,370,hwnd);if(picker.Modal(hwnd)==IDOK){draft.colors[id-30]=picker.color;Update();}}if(id==22){draft.direction=static_cast<Direction>((static_cast<int>(draft.direction)+1)%4);for(auto& d:draft.groupDirections)if(!Allowed(draft.direction,d))d=draft.direction;Update();}if(id==26){draft.x=draft.y=-1;}if(id==IDOK){draft.topmost=Checked(23);draft.locked=Checked(24);auto before=app.state;app.state=draft;if(!app.Persist()){app.state=before;return;}app.Apply();End(IDOK);}}
};
GameLauncher::Impl::Impl(HWND h,std::wstring d,Log l):owner(h),directory(std::move(d)),log(std::move(l)){
    std::wstring error;if(!games::Load(directory,state,error))log(error);widget=std::make_unique<Widget>(*this);Popup(*widget,L"Elka Game Launcher",438,160);drawer=std::make_unique<Drawer>(*this);Popup(*drawer,L"Elka Game Favorites",440,230);PlaceWidget();if(state.enabled){ShowWindow(widget->hwnd,SW_SHOWNOACTIVATE);Refresh();}
}
GameLauncher::Impl::~Impl(){closing=true;if(scanner.joinable())scanner.join();if(drawer&&IsWindow(drawer->hwnd))drawer->End(0);if(widget&&IsWindow(widget->hwnd))widget->End(0);}
void GameLauncher::Impl::Refresh(){if(scanning||closing)return;if(scanner.joinable())scanner.join();scanning=true;InvalidateRect(widget->hwnd,nullptr,TRUE);scanner=std::thread([this]{games::Library result;try{result=Discover();}catch(...){result.notes.push_back(L"Game scan could not finish. Refresh to try again.");}{std::lock_guard lock(scanMutex);scanResult=std::move(result);}PostMessageW(widget->hwnd,Scanned,0,0);});}
void GameLauncher::Impl::ScanDone(){if(scanner.joinable())scanner.join();{std::lock_guard lock(scanMutex);library=std::move(scanResult);}scanning=false;scanned=true;gameArtwork.clear();for(size_t i=0;i<3;++i)artwork[i].Load(library.clients[i]);log(L"Game library refreshed · "+std::to_wstring(library.games.size())+L" installed games");Apply();if(browser)PostMessageW(browser,LibraryChanged,0,0);}
void GameLauncher::Impl::PlaceWidget(){if(placing)return;placing=true;auto screen=CurrentScreen(state.screen);int x=screen.work.left+20,y=screen.work.top+40;SetWindowPos(widget->hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);int width=widget->S(state.custom.empty()?438:576),height=widget->S(state.toolbarVisible?160:122);if(state.x>=0)x=screen.work.left+widget->S(state.x);else if(state.direction==Direction::Left)x=screen.work.right-width-widget->S(24);if(state.y>=0)y=screen.work.top+widget->S(state.y);else if(state.direction==Direction::Up)y=screen.work.bottom-height-widget->S(24);x=std::clamp(x,int(screen.work.left),(std::max)(int(screen.work.left),int(screen.work.right)-width));y=std::clamp(y,int(screen.work.top),(std::max)(int(screen.work.top),int(screen.work.bottom)-height));SetWindowPos(widget->hwnd,state.topmost?HWND_TOPMOST:HWND_NOTOPMOST,x,y,width,height,SWP_NOACTIVATE);placing=false;}
void GameLauncher::Impl::Apply(){PlaceWidget();RECT client{};GetClientRect(widget->hwnd,&client);widget->Layout(MulDiv(client.right,96,widget->dpi),MulDiv(client.bottom,96,widget->dpi));InvalidateRect(widget->hwnd,nullptr,TRUE);for(int i=0;i<4;++i)InvalidateRect(widget->Get(TileBase+i),nullptr,TRUE);if(!state.enabled){Collapse();ShowWindow(widget->hwnd,SW_HIDE);}else if(expanded>=0)Expand(expanded,false);}
void GameLauncher::Impl::Expand(int store,bool animate){expanded=store;if(animate)page=0;RECT dock{},tile{};GetWindowRect(widget->hwnd,&dock);GetWindowRect(widget->Get(TileBase+store),&tile);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(widget->hwnd,MONITOR_DEFAULTTONEAREST),&mi);auto list=Favorites(store);auto layout=LayoutDrawer(dock,tile,mi.rcWork,static_cast<int>(list.size()),state.direction,state.groupDirections[store],widget->dpi,state.drawerToolbarVisible[store]);
    ShowWindow(drawer->hwnd,SW_HIDE);SetWindowPos(drawer->hwnd,nullptr,layout.bounds.left,layout.bounds.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);drawer->Set(store,std::move(list),std::move(layout));BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);if(animate&&animations){DWORD direction=state.direction==Direction::Down?AW_VER_POSITIVE:state.direction==Direction::Up?AW_VER_NEGATIVE:state.direction==Direction::Left?AW_HOR_NEGATIVE:AW_HOR_POSITIVE;AnimateWindow(drawer->hwnd,150,AW_SLIDE|direction);}ShowWindow(drawer->hwnd,SW_SHOWNOACTIVATE);}
void GameLauncher::Impl::Collapse(){expanded=-1;page=0;if(drawer)ShowWindow(drawer->hwnd,SW_HIDE);}
void GameLauncher::Impl::Options(HWND parent){OptionsWindow window(*this);window.Create(L"Floating shortcuts · layout & colors",640,478,parent);window.Modal(parent);}
void GameLauncher::Impl::Browse(HWND parent,int store){if(browser){SetForegroundWindow(browser);return;}if(!scanned)Refresh();Browser window(*this,store);window.Create(L"Installed games & favorites",820,620,parent);browser=window.hwnd;window.Modal(parent);browser=nullptr;}
std::optional<Target> GameLauncher::Impl::Pick(HWND parent){if(!scanned)Refresh();Browser window(*this,0,true);window.Create(L"Add a game launch target",820,620,parent);browser=window.hwnd;window.Modal(parent);browser=nullptr;return window.picked;}
void GameLauncher::Impl::LaunchGame(const Game& game){DWORD pid=0;std::wstring error;if(!elka::Launch(game.target,pid,error)){log(L"Game launch failed · "+game.name+L" · "+error);ui::Notice(browser?browser:widget->hwnd,L"Could not launch game",error);return;}log(L"Launch request sent · "+game.name+L" · "+StoreName(game.store));Collapse();}
GameLauncher::GameLauncher(HWND h,std::wstring d,Log l):impl(std::make_unique<Impl>(h,std::move(d),std::move(l))){}
GameLauncher::~GameLauncher()=default;
bool GameLauncher::Enabled()const{return impl->state.enabled;}
bool GameLauncher::Enable(bool enabled){bool old=impl->state.enabled;impl->state.enabled=enabled;if(!impl->Persist()){impl->state.enabled=old;return false;}impl->Apply();if(enabled){Show();if(!impl->scanned)impl->Refresh();}return true;}
void GameLauncher::Show(){if(!impl->state.enabled)return;impl->PlaceWidget();ShowWindow(impl->widget->hwnd,SW_SHOWNOACTIVATE);}
void GameLauncher::Options(HWND owner){impl->Options(owner);}
void GameLauncher::Library(HWND owner){impl->Browse(owner);}
std::optional<Target> GameLauncher::PickTarget(HWND owner){return impl->Pick(owner);}
}
