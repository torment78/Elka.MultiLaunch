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
void Tile(ui::Window& w,DRAWITEMSTRUCT* d,const std::wstring& name,const std::wstring& detail,COLORREF color,ui::IconArtwork* artwork=nullptr,COLORREF background=ui::Field){
    ui::DrawLauncherTile(d->hDC,d->rcItem,name,detail,color,w.font,w.bold,w.dpi,d->itemState,artwork,background);
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
    COLORREF color,accent;std::wstring name;bool updating=false,backgroundMode=false;SavedColors saved;ui::Hsv hsv;
    HBITMAP plane=nullptr;double planeHue=-1;int planeWidth=0,planeHeight=0,drag=0;
    static constexpr COLORREF presets[]={RGB(100,181,246),RGB(255,144,102),RGB(255,105,112),RGB(77,218,191),RGB(192,145,255),RGB(245,209,99),RGB(239,145,207),RGB(230,237,243)};
    ColorWindow(COLORREF value,std::wstring label,bool background,COLORREF textColor,const SavedColors& palette):color(value),accent(textColor),name(std::move(label)),backgroundMode(background),saved(palette),hsv(ui::ColorToHsv(value)){}
    ~ColorWindow()override{if(plane)DeleteObject(plane);}
    RECT Shade()const{return {S(24),S(54),S(322),S(254)};}
    RECT Hue()const{return {S(338),S(54),S(362),S(254)};}
    void Created()override{Label(backgroundMode?L"Choose a node background":L"Choose an accent color",20);Label(L"Hex color (#RRGGBB)",21);Edit(ColorText(color),22);SendMessageW(Get(22),EM_SETLIMITTEXT,7,0);Label(L"Saved colors",23);Button(L"Save this color",24);Label(L"Drag in the square for shade; use the rainbow for hue.\nSave up to eight colors. Once full, the oldest slot is replaced.",25);Label(L"Presets",26);for(int i=0;i<8;++i){auto button=Button(ColorText(presets[i]),40+i);SetPropW(button,L"customDraw",reinterpret_cast<HANDLE>(1));button=Button(L"Empty color slot",60+i);SetPropW(button,L"customDraw",reinterpret_cast<HANDLE>(1));}Button(L"Cancel",IDCANCEL);Button(L"Use color",IDOK,true);UpdateSaved();}
    void Layout(int w,int h)override{Place(20,24,18,w-48,24);Place(21,24,282,190,24);Place(22,218,276,154,30);Place(24,400,276,176,32);Place(26,24,314,w-48,22);Place(23,24,388,w-48,22);for(int i=0;i<8;++i){Place(40+i,24+i*70,340,62,30);Place(60+i,24+i*70,420,62,32);}Place(25,24,474,w-48,44);Place(IDCANCEL,w-264,h-56,112,34);Place(IDOK,w-140,h-56,116,34);}
    void UpdateSaved(){Value(23,L"Saved colors ("+std::to_wstring(saved.count)+L" / 8)");for(int i=0;i<8;++i){EnableWindow(Get(60+i),i<saved.count);Value(60+i,i<saved.count?ColorText(saved.colors[i]):L"Empty color slot");InvalidateRect(Get(60+i),nullptr,TRUE);}}
    void UpdateText(){updating=true;Value(22,ColorText(color));updating=false;InvalidateRect(hwnd,nullptr,FALSE);}
    void Select(COLORREF value){color=value;auto next=ui::ColorToHsv(value);if(next.saturation==0)next.hue=hsv.hue;hsv=next;UpdateText();}
    void DrawPlane(HDC dc){
        auto shade=Shade(),hue=Hue();int width=hue.right-shade.left,height=shade.bottom-shade.top,shadeWidth=shade.right-shade.left,hueStart=hue.left-shade.left;
        if(!plane||width!=planeWidth||height!=planeHeight||planeHue!=hsv.hue){
            auto next=ui::CreateColorPlane(width,height,shadeWidth,hueStart,hsv.hue);
            if(next){if(plane)DeleteObject(plane);plane=next;planeWidth=width;planeHeight=height;planeHue=hsv.hue;}
        }
        if(plane){auto memory=CreateCompatibleDC(dc);auto old=SelectObject(memory,plane);BitBlt(dc,shade.left,shade.top,width,height,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteDC(memory);}
        int x=shade.left+static_cast<int>(hsv.saturation*(shadeWidth-1)),y=shade.top+static_cast<int>((1-hsv.value)*(height-1));
        for(int pass=0;pass<2;++pass){auto pen=CreatePen(PS_SOLID,S(pass?1:3),pass?RGB(255,255,255):RGB(0,0,0));auto old=SelectObject(dc,pen);MoveToEx(dc,x-S(5),y-S(5),nullptr);LineTo(dc,x+S(6),y+S(6));MoveToEx(dc,x-S(5),y+S(5),nullptr);LineTo(dc,x+S(6),y-S(6));SelectObject(dc,old);DeleteObject(pen);}
        y=hue.top+static_cast<int>(hsv.hue/359.99*(height-1));Box(dc,{hue.left-S(2),y-S(2),hue.right+S(2),y+S(3)},RGB(0,0,0));Box(dc,{hue.left-S(2),y,hue.right+S(2),y+S(1)},RGB(255,255,255));
    }
    void Paint(HDC dc,RECT)override{DrawPlane(dc);ui::DrawLauncherTile(dc,{S(382),S(54),S(576),S(254)},name,L"Favorites",backgroundMode?accent:color,font,bold,dpi,0,nullptr,backgroundMode?color:ui::Field);}
    void Item(DRAWITEMSTRUCT* d)override{int id=static_cast<int>(d->CtlID);bool custom=id>=60;int i=id-(custom?60:40);if(i<0||i>=8)return;bool available=!custom||i<saved.count;auto value=custom?saved.colors[i]:presets[i];Box(d->hDC,d->rcItem,ui::Background);Box(d->hDC,d->rcItem,available?value:ui::Field,8);auto ink=(int(GetRValue(value))*299+int(GetGValue(value))*587+int(GetBValue(value))*114)>140000?RGB(0,0,0):RGB(255,255,255);TextAt(d->hDC,available?ColorText(value):L"—",d->rcItem,available?ink:ui::Muted,font,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS){auto r=d->rcItem;InflateRect(&r,-S(3),-S(3));DrawFocusRect(d->hDC,&r);}}
    void Command(int id,int code)override{
        if(id==IDCANCEL){End(0);return;}if(id>=40&&id<48)Select(presets[id-40]);if(id>=60&&id<60+saved.count)Select(saved.colors[id-60]);
        if(id==22&&code==EN_CHANGE&&!updating){COLORREF value;if(ParseColor(Value(22),value)){color=value;auto next=ui::ColorToHsv(value);if(next.saturation==0)next.hue=hsv.hue;hsv=next;InvalidateRect(hwnd,nullptr,FALSE);}}
        if(id==24||id==IDOK){if(!ParseColor(Value(22),color)){ui::Notice(hwnd,L"Color",L"Enter six color digits, for example #64B5F6, or select a shade.");return;}if(id==IDOK){End(IDOK);return;}bool exists=false;for(int i=0;i<saved.count;++i)if(saved.colors[i]==color)exists=true;if(!exists){saved.colors[saved.next]=color;saved.next=(saved.next+1)%8;saved.count=(std::min)(8,saved.count+1);}UpdateSaved();}
    }
    void Drag(POINT point){auto r=drag==1?Shade():Hue();if(drag==1){hsv.saturation=std::clamp(double(point.x-r.left)/(r.right-r.left-1),0.0,1.0);hsv.value=1-std::clamp(double(point.y-r.top)/(r.bottom-r.top-1),0.0,1.0);}else hsv.hue=359.99*std::clamp(double(point.y-r.top)/(r.bottom-r.top-1),0.0,1.0);color=ui::HsvToColor(hsv);UpdateText();}
    LRESULT Message(UINT message,WPARAM,LPARAM l,bool& handled)override{
        handled=false;POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        if(message==WM_LBUTTONDOWN){auto shade=Shade(),hue=Hue();if(PtInRect(&shade,point))drag=1;else if(PtInRect(&hue,point))drag=2;if(drag){SetCapture(hwnd);Drag(point);handled=true;}}
        if(message==WM_MOUSEMOVE&&drag&&GetCapture()==hwnd){Drag(point);handled=true;}
        if(message==WM_LBUTTONUP&&drag){Drag(point);drag=0;ReleaseCapture();handled=true;}if(message==WM_CAPTURECHANGED)drag=0;return 0;
    }
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
            image->Load(file,file==game.icon?game.iconIndex:0,128);found=gameArtwork.emplace(key,CachedIcon{std::move(image),0}).first;
        }
        found->second.used=++iconUse;return found->second.image.get();
    }
    std::unique_ptr<Widget> widget;std::unique_ptr<Drawer> drawer;std::array<ui::IconArtwork,StoreCount> artwork;std::array<std::wstring,StoreCount> artworkPaths;DockMetrics dock;
    std::thread scanner;std::mutex scanMutex;games::Library scanResult;bool scanning=false,scanned=false,placing=false,closing=false,rescan=false;int expanded=-1,page=0;
    Impl(HWND h,std::wstring d,Log l);~Impl();
    std::wstring Name(int store)const{return LauncherName(state,store);}
    bool Persist(){std::wstring error;if(!games::Save(directory,state,error)){ui::Notice(owner,L"Game launcher settings",error);return false;}return true;}
    void Refresh();void ScanDone();void PlaceWidget();void Apply();void Expand(int store,bool animate=true);void Collapse();void Options(HWND parent);void Browse(HWND parent,int store=0);std::optional<Target> Pick(HWND parent);void LaunchGame(const Game& game);
    std::vector<Game> All()const{auto list=library.games;for(const auto& g:state.custom)list.push_back(g);std::sort(list.begin(),list.end(),[](const Game&a,const Game&b){return a.store==b.store?elka::Key(a.name)<elka::Key(b.name):a.store<b.store;});return list;}
    std::vector<Game> Favorites(int store)const{std::vector<Game> r;for(const auto& g:All())if(static_cast<int>(g.store)==store&&state.favorites.count(g.Key()))r.push_back(g);return r;}
    bool Favorite(const Game& g){auto before=state.favorites;if(state.favorites.count(g.Key()))state.favorites.erase(g.Key());else state.favorites.insert(g.Key());if(!Persist()){state.favorites=before;return false;}Apply();return true;}
};
struct GameLauncher::Impl::Widget:ui::Window {
    Impl& app;int contextStore=-1;explicit Widget(Impl& a):app(a){}
    void Created()override{
        Button(L"Library",10);Button(L"Options",11);Button(L"Hide",12);Button(L"Lock",13);Button(L"Always on top",14);
        for(int i=0;i<static_cast<int>(StoreCount);++i){auto h=Button(app.Name(i),TileBase+i);SetPropW(h,L"customDraw",reinterpret_cast<HANDLE>(1));}
        Button(L"Add launchers",TileBase+static_cast<int>(StoreCount));
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SetWindowSubclass(child,DockContext,1,0);
    }
    void Layout(int,int)override{
        const int buttons[]{10,11,13,14,12};for(size_t i=0;i<app.dock.buttons.size();++i){auto r=app.dock.buttons[i];Place(buttons[i],r.left,r.top,r.right-r.left,r.bottom-r.top);}
        for(int id:{10,11,12,13,14})Show(id,app.state.toolbarVisible);
        Value(13,app.state.locked?L"Locked":L"Lock");
        SetPropW(Get(13),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(app.state.locked)));
        SetPropW(Get(14),L"accent",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(app.state.topmost)));
        InvalidateRect(Get(13),nullptr,TRUE);InvalidateRect(Get(14),nullptr,TRUE);
        for(int i=0;i<static_cast<int>(StoreCount);++i)Show(TileBase+i,false);
        auto visible=VisibleLaunchers(app.state);for(size_t at=0;at<visible.size();++at){int i=visible[at];Place(TileBase+i,12+static_cast<int>(at)%app.dock.columns*138,app.dock.tileTop+static_cast<int>(at)/app.dock.columns*102,132,96);Value(TileBase+i,app.Name(i));Show(TileBase+i,true);}
        int add=TileBase+static_cast<int>(StoreCount);Place(add,12,app.dock.tileTop,132,96);Show(add,visible.empty());
    }
    void Paint(HDC dc,RECT r)override{
        if(app.state.locked)return;
        int x=r.right/2-S(10),y=S(4);
        Box(dc,{x,y,x+S(20),y+S(1)},ui::Muted);Box(dc,{x,y+S(4),x+S(20),y+S(5)},ui::Muted);
    }
    void Item(DRAWITEMSTRUCT* d)override{
        if(d->CtlType==ODT_MENU){MenuItem(*this,d);return;}
        int i=static_cast<int>(d->CtlID)-TileBase;if(i<0||i>=static_cast<int>(StoreCount))return;auto count=app.Favorites(i).size();Tile(*this,d,app.Name(i),app.scanning?L"Scanning…":std::to_wstring(count)+L" favorites",app.state.colors[i],&app.artwork[i],app.state.backgrounds[i]);
    }
    void Context(POINT point){
        HWND child=GetFocus();if(point.x!=-1||point.y!=-1){auto local=point;ScreenToClient(hwnd,&local);child=ChildWindowFromPoint(hwnd,local);}contextStore=child?GetDlgCtrlID(child)-TileBase:-1;
        std::vector<MenuChoice> choices{{10,L"Library"},{11,L"Launchers, layout & colors"},{13,L"Lock position",app.state.locked},{14,L"Always on top",app.state.topmost},{15,app.state.toolbarVisible?L"Hide button row":L"Show button row"}};
        if(contextStore>=0&&contextStore<static_cast<int>(StoreCount))choices.push_back({16,L"Hide this launcher"});choices.push_back({12,L"Hide floating shortcuts"});ContextMenu(*this,point,choices);
    }
    void Command(int id,int)override{
        if(id==TileBase+static_cast<int>(StoreCount)){app.Options(hwnd);return;}
        if(id>=TileBase&&id<TileBase+static_cast<int>(StoreCount)){if(app.expanded==id-TileBase)app.Collapse();else app.Expand(id-TileBase);return;}
        if(id==10)app.Browse(hwnd);if(id==11)app.Options(hwnd);if(id==12)Close();
        if(id==13||id==14||id==15||id==16){auto before=app.state;if(id==16&&contextStore>=0&&contextStore<static_cast<int>(StoreCount))app.state.launchers[contextStore].visible=false;if(id==13)app.state.locked=!app.state.locked;if(id==14)app.state.topmost=!app.state.topmost;if(id==15){auto screen=CurrentScreen(app.state.screen);RECT r{};GetWindowRect(hwnd,&r);int oldTop=app.dock.tileTop;app.state.toolbarVisible=!app.state.toolbarVisible;auto next=LayoutDock(static_cast<int>(VisibleLaunchers(app.state).size()),MulDiv(screen.work.right-screen.work.left,96,dpi),app.state.toolbarVisible);app.state.y=(std::max)(0,MulDiv(r.top-screen.work.top,96,dpi)+oldTop-next.tileTop);}if(!app.Persist()){app.state=before;return;}app.Apply();auto visible=VisibleLaunchers(app.state);SetFocus(Get(TileBase+(visible.empty()?static_cast<int>(StoreCount):visible.front())));}
    }
    void Close()override{app.Collapse();ShowWindow(hwnd,SW_HIDE);}
    LRESULT Message(UINT m,WPARAM,LPARAM l,bool& handled)override{
        handled=true;if(m==Scanned){app.ScanDone();return 0;}
        if(m==WM_CONTEXTMENU||m==WM_NCRBUTTONUP){Context({GET_X_LPARAM(l),GET_Y_LPARAM(l)});return 0;}
        if(m==WM_NCHITTEST&&!app.state.locked){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd,&p);if(p.y>=0&&p.y<S(12))return HTCAPTION;handled=false;return 0;}
        if(m==WM_ENTERSIZEMOVE){app.Collapse();return 0;}
        if(m==WM_EXITSIZEMOVE){MONITORINFOEXW mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);RECT r{};GetWindowRect(hwnd,&r);app.state.screen=mi.szDevice;app.state.x=(std::max)(0,MulDiv(r.left-mi.rcWork.left,96,dpi));app.state.y=(std::max)(0,MulDiv(r.top-mi.rcWork.top,96,dpi));app.PlaceWidget();app.Persist();return 0;}
        if(m==WM_DISPLAYCHANGE){app.PlaceWidget();app.Collapse();return 0;}
        handled=false;return 0;
    }
};
struct GameLauncher::Impl::Drawer:ui::Window {
    Impl& app;std::vector<Game> games;games::Placement placement;int store=0;explicit Drawer(Impl&a):app(a){}
    static constexpr UINT_PTR RevealTimer=1;
    std::vector<RECT> nodes;ULONGLONG revealStarted=0;bool revealing=false,updatingRegion=false;int layoutWidth=0,layoutHeight=0;
    void StopReveal(){if(revealing)KillTimer(hwnd,RevealTimer);revealing=false;}
    void Region(const std::vector<RECT>& surfaces){
        if(updatingRegion)return;auto region=ui::CreateNodeRegion(surfaces,S(10));if(!region)return;
        updatingRegion=true;if(!SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);updatingRegion=false;
    }
    void FinishReveal(){StopReveal();RECT r{};GetClientRect(hwnd,&r);Layout(MulDiv(r.right,96,dpi),MulDiv(r.bottom,96,dpi));RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);}
    void RevealTick(){
        if(!revealing)return;auto elapsed=(std::min)(GetTickCount64()-revealStarted,ULONGLONG(1500));auto frame=RevealDrawer(nodes,app.state.direction,app.state.groupDirections[store],static_cast<unsigned>(elapsed));
        if(frame.finished){FinishReveal();return;}Region(frame.nodes);RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    void BeginReveal(bool animate){
        BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);if(!animate||!animations||nodes.empty())return;
        // Only a short-lived UI timer runs while the drawer unfolds. Node controls
        // and cached artwork stay at their final size, avoiding per-frame rescaling.
        revealing=true;revealStarted=GetTickCount64();for(int id:{10,11,12,13,14})Show(id,false);Region({});
        if(!SetTimer(hwnd,RevealTimer,16,nullptr))FinishReveal();
    }
    bool Controls()const{return app.state.drawerToolbarVisible[store];}
    int Pages()const{return (std::max)(1,(static_cast<int>(games.size())+placement.capacity-1)/placement.capacity);}
    RECT EmptyNode()const{return {S(9),S(Controls()?49:9),S(141),S(Controls()?123:83)};}
    void Created()override{Button(L"+",10);Button(L"×",11);Button(L"‹",12);Button(L"›",13);Button(L"↳",14);for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SetWindowSubclass(child,DockContext,1,0);}
    void Set(int s,std::vector<Game> list,games::Placement p){
        StopReveal();
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;){HWND next=GetWindow(child,GW_HWNDNEXT);if(GetDlgCtrlID(child)>=TileBase)DestroyWindow(child);child=next;}
        store=s;games=std::move(list);placement=std::move(p);static const wchar_t* arrows[]{L"↓",L"↑",L"←",L"→"};Value(14,arrows[static_cast<size_t>(app.state.groupDirections[store])]);app.page=std::clamp(app.page,0,Pages()-1);
        int begin=app.page*placement.capacity,end=(std::min)(static_cast<int>(games.size()),begin+placement.capacity);for(int i=begin;i<end;++i){auto h=Button(games[i].name,TileBase+i-begin);SetPropW(h,L"customDraw",reinterpret_cast<HANDLE>(1));SetWindowSubclass(h,DockContext,1,0);}
        const auto r=placement.bounds;SetWindowPos(hwnd,app.state.topmost?HWND_TOPMOST:HWND_NOTOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);RECT client{};GetClientRect(hwnd,&client);Layout(MulDiv(client.right,96,dpi),MulDiv(client.bottom,96,dpi));InvalidateRect(hwnd,nullptr,TRUE);
    }
    void Layout(int w,int h)override{
        if(updatingRegion||(revealing&&w==layoutWidth&&h==layoutHeight))return;StopReveal();nodes.clear();layoutWidth=w;layoutHeight=h;
        Place(10,w-98,6,28,28);Place(14,w-66,6,28,28);Place(11,w-34,6,28,28);Place(12,w-76,h-34,30,27);Place(13,w-40,h-34,30,27);
        for(int id:{10,11,12,13,14})Show(id,Controls());
        std::vector<RECT> surfaces;if(Controls()){surfaces.push_back({0,0,S(w),S(40)});surfaces.push_back({0,S(h-38),S(w),S(h)});}
        for(size_t i=0;i<placement.cells.size();++i){const auto p=placement.cells[i];if(auto child=Get(TileBase+static_cast<int>(i))){MoveWindow(child,p.x,p.y,S(132),S(74),TRUE);nodes.push_back({p.x,p.y,p.x+S(132),p.y+S(74)});}}
        surfaces.insert(surfaces.end(),nodes.begin(),nodes.end());
        if(games.empty())surfaces.push_back(EmptyNode());
        // The gaps are outside the window itself, so the desktop shows through.
        Region(surfaces);
        EnableWindow(Get(12),app.page>0);EnableWindow(Get(13),app.page+1<Pages());
    }
    void Paint(HDC dc,RECT r)override{
        if(Controls()&&!revealing){TextAt(dc,app.Name(store),{S(8),S(6),r.right-S(100),S(34)},app.state.colors[store],bold);TextAt(dc,L"Page "+std::to_wstring(app.page+1)+L" / "+std::to_wstring(Pages()),{S(12),r.bottom-S(34),r.right-S(82),r.bottom-S(5)},ui::Muted);}
        if(games.empty())ui::DrawLauncherTile(dc,EmptyNode(),L"No favorites",L"Right-click to add",app.state.colors[store],font,bold,dpi,0,nullptr,app.state.backgrounds[store]);
    }
    void Item(DRAWITEMSTRUCT* d)override{if(d->CtlType==ODT_MENU){MenuItem(*this,d);return;}auto index=app.page*placement.capacity+static_cast<int>(d->CtlID)-TileBase;if(index>=0&&index<static_cast<int>(games.size()))Tile(*this,d,games[index].name,L"Play",app.state.colors[store],app.GameIcon(games[index]),app.state.backgrounds[store]);}
    void Context(POINT point){if(revealing)FinishReveal();std::vector<MenuChoice> choices{{10,L"Library / add favorites"},{15,Controls()?L"Hide button row":L"Show button row"},{14,L"Build: "+std::wstring(DirectionName(app.state.groupDirections[store]))},{16,L"Launchers, layout & colors"}};if(Pages()>1){if(app.page>0)choices.push_back({12,L"Previous page"});if(app.page+1<Pages())choices.push_back({13,L"Next page"});}choices.push_back({11,L"Close favorites"});ContextMenu(*this,point,choices);}
    void Command(int id,int)override{
        if(id>=TileBase){auto index=app.page*placement.capacity+id-TileBase;if(index>=0&&index<static_cast<int>(games.size())){auto game=games[index];app.LaunchGame(game);}return;}
        if(id==10)app.Browse(hwnd,store);if(id==11)app.Collapse();if(id==16)app.Options(hwnd);if(id==12||id==13){app.page+=id==12?-1:1;app.Expand(store,false);}
        if(id==14||id==15){auto before=app.state;if(id==14){auto& d=app.state.groupDirections[store];do{d=static_cast<Direction>((static_cast<int>(d)+1)%4);}while(!Allowed(app.state.direction,d));}else app.state.drawerToolbarVisible[store]=!Controls();if(!app.Persist()){app.state=before;return;}app.Expand(store,id==14);if(auto first=Get(TileBase))SetFocus(first);else SetFocus(hwnd);}
    }
    void Close()override{app.Collapse();}
    LRESULT Message(UINT msg,WPARAM w,LPARAM l,bool& handled)override{handled=true;if(msg==WM_TIMER&&w==RevealTimer){RevealTick();return 0;}if(msg==WM_KEYDOWN&&w==VK_ESCAPE){app.Collapse();return 0;}if(msg==WM_CONTEXTMENU){Context({GET_X_LPARAM(l),GET_Y_LPARAM(l)});return 0;}if(msg==WM_DESTROY||(msg==WM_SHOWWINDOW&&!w)||msg==WM_DPICHANGED)StopReveal();if(msg==WM_SETTINGCHANGE&&revealing)FinishReveal();handled=false;return 0;}
};
struct ManualGame:ui::Window {
    Game game;const games::Library& library;LauncherEntry entry;
    ManualGame(Store store,const games::Library& lib,const State& state):library(lib),entry(state.launchers[static_cast<size_t>(store)]){game.store=store;game.id=NewId();game.target.skipRunning=false;}
    void Created()override{Label(L"Name",20);Edit(L"",21);Label(entry.kind==LauncherKind::Epic||entry.kind==LauncherKind::Ubisoft?L"Launcher application (optional for launch links)":L"Launcher or game application",22);Edit(L"",23,true);Button(L"Browse EXE",24);Label(entry.kind==LauncherKind::Epic?L"Epic ID or launch link (optional with a game folder)":entry.kind==LauncherKind::Gog?L"GOG game ID":entry.kind==LauncherKind::Ubisoft?L"Ubisoft game ID or uplay://launch/ link":entry.kind==LauncherKind::Rockstar?L"Rockstar opens this game using the installation folder below.":game.store==Store::Steam?L"Arguments (or just the Steam AppID)":L"Launch arguments (optional)",25);Edit(L"",26);Show(26,entry.kind!=LauncherKind::Rockstar);Label(entry.kind==LauncherKind::Rockstar?L"Game installation folder (required)":L"Game folder (optional; also used for its icon)",27);Edit(L"",28);Button(L"From game EXE",29);Button(L"Browse folder",30);Button(L"Cancel",IDCANCEL);Button(L"Add game",IDOK,true);Value(23,entry.executable.empty()?library.clients[static_cast<size_t>(game.store)]:entry.executable);}
    void Layout(int w,int)override{Place(20,24,20,w-48,24);Place(21,24,48,w-48,30);Place(22,24,96,w-48,24);Place(23,24,124,w-184,30);Place(24,w-148,122,124,34);Place(25,24,176,w-48,24);Place(26,24,204,w-48,30);Place(27,24,252,w-48,24);Place(28,24,280,w-324,30);Place(30,w-288,278,120,34);Place(29,w-156,278,132,34);Place(IDCANCEL,w-264,344,112,34);Place(IDOK,w-140,344,116,34);}
    void Command(int id,int)override{
        if(id==IDCANCEL)End(0);if(id==24){auto path=ui::PickExe(hwnd,Value(23));if(!path.empty())Value(23,path);}if(id==29){auto path=ui::PickExe(hwnd);if(!path.empty())Value(28,Parent(path));}
        if(id==30){auto folder=ui::PickFolder(hwnd,Value(28));if(!folder.empty())Value(28,folder);}
        if(id==IDOK){game.name=Value(21);game.folder=Value(28);auto args=Value(26);if(game.store==Store::Steam&&!args.empty()&&args.find_first_not_of(L"0123456789")==args.npos)args=L"-applaunch "+args;
            std::wstring error;if(game.name.empty()||game.name.size()>120){ui::Notice(hwnd,L"Game details",L"Enter a game name of up to 120 characters.");return;}
            if(!ManualTarget(entry.kind,Value(23),args,game.folder,game.target,error)){ui::Notice(hwnd,L"Game details",error);return;}
            if(!IsGameLaunchUri(game.target.path)&&!IsExe(game.target.path)){ui::Notice(hwnd,L"Game details",L"Choose an existing launcher or game EXE.");return;}std::error_code ec;if(!game.folder.empty()&&!std::filesystem::is_directory(game.folder,ec)){ui::Notice(hwnd,L"Game folder",L"Choose an existing game folder or leave it blank.");return;}End(IDOK);
        }
    }
};
struct LauncherWindow:ui::Window {
    State& draft;int index;LauncherEntry entry;COLORREF accent,background;SavedColors savedColors;
    LauncherWindow(State& state,int i):draft(state),index(i),entry(state.launchers[i]),accent(state.colors[i]),background(state.backgrounds[i]),savedColors(state.palette){if(entry.name.empty()){entry.name=L"New launcher";entry.visible=true;}}
    void Created()override{Button(L"",30);Label(L"Display name",20);Edit(entry.name,21);Label(L"Launcher application (can be added after installing the launcher)",22);Edit(entry.executable,23,true);Button(L"Browse EXE",24);Check(L"Show on floating dock",25,entry.visible);Button(L"",26);Button(L"",27);Button(L"Find installed",28);Label(L"Color choices apply to this launcher and its game nodes.",29);Button(L"Cancel",IDCANCEL);Button(L"Use launcher",IDOK,true);Update();}
    void Update(){Value(30,index<BuiltinCount?L"Built-in launcher":L"Preset: "+std::wstring(LauncherKindName(entry.kind)));EnableWindow(Get(30),index>=BuiltinCount);EnableWindow(Get(28),index>=BuiltinCount&&entry.kind!=LauncherKind::Custom);Value(26,L"Accent  "+ColorText(accent));Value(27,L"Background  "+ColorText(background));}
    void Layout(int w,int)override{Place(30,24,20,w-48,36);Place(20,24,76,w-48,24);Place(21,24,104,w-48,30);Place(22,24,156,w-48,24);Place(23,24,184,w-184,30);Place(24,w-148,182,124,34);Place(28,24,230,140,34);Place(25,180,230,w-204,34);Place(26,24,288,(w-60)/2,38);Place(27,36+(w-60)/2,288,(w-60)/2,38);Place(29,24,342,w-48,24);Place(IDCANCEL,w-268,402,112,34);Place(IDOK,w-144,402,120,34);}
    void Command(int id,int)override{
        if(id==IDCANCEL)End(0);if(id==30&&index>=BuiltinCount){std::vector<MenuChoice> choices;for(int kind=0;kind<LauncherKindCount;++kind)choices.push_back({static_cast<UINT>(100+kind),LauncherKindName(static_cast<LauncherKind>(kind)),static_cast<int>(entry.kind)==kind});RECT r{};GetWindowRect(Get(30),&r);ContextMenu(*this,{r.left,r.bottom},choices);}
        if(id>=100&&id<100+LauncherKindCount&&index>=BuiltinCount){auto next=static_cast<LauncherKind>(id-100);if(next!=entry.kind){auto old=std::wstring(LauncherKindName(entry.kind));entry.kind=next;if(Value(21)==old||Value(21)==L"Other"||Value(21)==L"New launcher")Value(21,entry.kind==LauncherKind::Custom?L"Other":LauncherKindName(entry.kind));Value(23,FindLauncher(entry.kind));Update();}}
        if(id==28){auto path=FindLauncher(entry.kind);if(path.empty())ui::Notice(hwnd,L"Launcher not found",L"This launcher was not found in Windows' installed-app records or standard folders. You can keep this launcher choice, install the client later, then use Find installed or Browse EXE.");else Value(23,path);}
        if(id==24){auto path=ui::PickExe(hwnd,Value(23));if(!path.empty())Value(23,path);}
        if(id==26||id==27){ColorWindow picker(id==26?accent:background,Value(21),id==27,accent,savedColors);picker.Create(id==26?L"Accent color":L"Node background",600,590,hwnd);if(picker.Modal(hwnd)==IDOK){if(id==26)accent=picker.color;else background=picker.color;savedColors=picker.saved;Update();}}
        if(id==IDOK){entry.name=Value(21);entry.executable=Value(23);entry.visible=Checked(25);if(entry.name.empty()||entry.name.size()>60||entry.name.find_first_not_of(L" \t")==entry.name.npos){ui::Notice(hwnd,L"Launcher name",L"Enter a name of up to 60 characters.");return;}if(!entry.executable.empty()&&!IsExe(entry.executable)){ui::Notice(hwnd,L"Launcher application",L"Choose an existing EXE or leave it blank.");return;}draft.launchers[index]=entry;draft.colors[index]=accent;draft.backgrounds[index]=background;draft.palette=savedColors;End(IDOK);}
    }
    void Item(DRAWITEMSTRUCT* d)override{if(d->CtlType==ODT_MENU)MenuItem(*this,d);}
};
struct GameLauncher::Impl::Browser:ui::Window {
    Impl& app;std::vector<Game> visible;int store=0;bool picking=false,changing=false;std::optional<Target> picked;
    Browser(Impl&a,int s,bool pick=false):app(a),store(s),picking(pick){}
    void Created()override{Button(L"",30);Button(L"Manage launchers",31);Edit(L"",20);Label(L"Filter installed games",21);List(22,60);Button(L"Refresh",23);Button(L"★ Favorite",24,true);Button(picking?L"Add target":L"Play",25);Button(L"Add manually",26);Button(L"Done",IDCANCEL);Button(L"Remove custom",27);Label(L"",28);Reload();}
    void Layout(int w,int h)override{Place(30,24,20,w-244,40);Place(31,w-208,20,184,40);Place(21,24,77,180,22);Place(20,204,72,w-340,30);Place(23,w-120,72,96,32);Place(22,24,120,w-48,h-256);Place(28,24,h-126,w-48,46);Place(24,24,h-68,144,36);Place(25,180,h-68,116,36);Place(26,308,h-68,126,36);Place(27,446,h-68,132,36);Place(IDCANCEL,w-116,h-68,92,36);}
    void Reload(){changing=true;auto selected=Selected();auto key=selected?selected->Key():L"";visible.clear();SendMessageW(Get(22),LB_RESETCONTENT,0,0);auto filter=elka::Key(Value(20));for(const auto& g:app.All())if(static_cast<int>(g.store)==store&&(filter.empty()||elka::Key(g.name+L" "+g.id).find(filter)!=std::wstring::npos)){visible.push_back(g);SendMessageW(Get(22),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(g.name.c_str()));if(g.Key()==key)SendMessageW(Get(22),LB_SETCURSEL,visible.size()-1,0);}
        if(SendMessageW(Get(22),LB_GETCURSEL,0,0)==LB_ERR&&!visible.empty())SendMessageW(Get(22),LB_SETCURSEL,0,0);
        Value(30,L"Launcher: "+app.Name(store)+L"  ▾");Value(28,app.scanning?L"Reading installed games from local launcher files…":std::to_wstring(visible.size())+L" games · Select a game and choose Favorite to put it in the floating drawer.\n"+(app.library.notes.empty()?L"No sign-in or online library access is used.":app.library.notes.front()));if(store==2&&visible.empty()&&Value(20).empty()&&!app.scanning)Value(28,app.library.clients[2].empty()?L"Battle.net client not found. Install it or add a game manually.":L"Battle.net is installed; no supported installed games were found in its local records.\nUse Refresh after installing or locating a game in Battle.net, or choose Add manually.");if(!app.scanning&&!app.library.status[store].empty())Value(28,std::to_wstring(visible.size())+L" games · "+app.library.status[store]);EnableWindow(Get(23),!app.scanning);changing=false;Update();}
    std::optional<Game> Selected()const{auto i=static_cast<int>(SendMessageW(Get(22),LB_GETCURSEL,0,0));if(i>=0&&i<static_cast<int>(visible.size()))return visible[i];return {};}
    void Update(){auto g=Selected();EnableWindow(Get(24),g.has_value());EnableWindow(Get(25),g.has_value());bool custom=g&&std::any_of(app.state.custom.begin(),app.state.custom.end(),[&](const Game& c){return c.Key()==g->Key();});EnableWindow(Get(27),custom);Value(24,g&&app.state.favorites.count(g->Key())?L"★ Unfavorite":L"☆ Favorite");}
    void Item(DRAWITEMSTRUCT* d)override{if(d->CtlType==ODT_MENU){MenuItem(*this,d);return;}if(d->itemID>=visible.size())return;const auto& g=visible[d->itemID];Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);auto r=d->rcItem;auto icon=app.GameIcon(g);if(icon->Draw(d->hDC,{r.left+S(10),r.top+S(8),r.left+S(54),r.top+S(52)},255))r.left+=S(54);r.left+=S(12);r.top+=S(5);r.right-=S(12);r.bottom=r.top+S(25);TextAt(d->hDC,(app.state.favorites.count(g.Key())?L"★  ":L"☆  ")+g.name,r,ui::Text,bold);r.top+=S(25);r.bottom+=S(24);TextAt(d->hDC,static_cast<int>(g.store)>=BuiltinCount?g.target.path:app.Name(static_cast<int>(g.store))+L" · "+g.id,r,ui::Muted);}
    void Command(int id,int code)override{
        if(id==30){RECT r{};GetWindowRect(Get(30),&r);std::vector<MenuChoice> choices;for(int i=0;i<static_cast<int>(StoreCount);++i)if(!app.Name(i).empty())choices.push_back({static_cast<UINT>(100+i),app.Name(i),i==store});ContextMenu(*this,{r.left,r.bottom},choices);return;}if(id>=100&&id<100+static_cast<int>(StoreCount)){store=id-100;Value(20,L"");Reload();return;}if(id==31){app.Options(hwnd);Reload();return;}if(id==20&&code==EN_CHANGE&&!changing){Reload();return;}if(id==22&&code==LBN_SELCHANGE){Update();return;}if(id==IDCANCEL){End(0);return;}if(id==23){app.Refresh();Reload();return;}
        if(id==26){ManualGame add(static_cast<Store>(store),app.library,app.state);add.Create(L"Add a game · "+app.Name(store),660,408,hwnd);if(add.Modal(hwnd)==IDOK){auto before=app.state;app.state.custom.push_back(add.game);app.state.favorites.insert(add.game.Key());if(!app.Persist())app.state=before;app.Apply();Reload();}return;}
        auto game=Selected();if(!game)return;if(id==24||(id==22&&code==LBN_DBLCLK)){app.Favorite(*game);Reload();}if(id==25){if(picking){picked=game->target;End(IDOK);}else app.LaunchGame(*game);}if(id==27){auto before=app.state;app.state.custom.erase(std::remove_if(app.state.custom.begin(),app.state.custom.end(),[&](const Game& g){return g.Key()==game->Key();}),app.state.custom.end());app.state.favorites.erase(game->Key());if(!app.Persist())app.state=before;app.Apply();Reload();}
    }
    LRESULT Message(UINT m,WPARAM,LPARAM,bool& handled)override{handled=m==LibraryChanged;if(handled)Reload();return 0;}
};
struct GameLauncher::Impl::OptionsWindow:ui::Window {
    Impl& app;State draft;std::vector<int> entries;OptionsWindow(Impl&a):app(a),draft(a.state){}
    void Created()override{Label(L"LAUNCHERS · LAYOUT & COLORS",20);List(21,52);Button(L"",22);Button(L"",30);Check(L"Always on top",23,draft.topmost);Check(L"Lock position",24,draft.locked);Label(L"Open sets the first direction. Build sets the selected launcher's node flow.\nDrag the dock grip onto any screen. Its position is remembered.\nHidden launchers keep their games and favorites.",25);Button(L"Reset position",26);Button(L"Show / hide",27);Button(L"Edit launcher",28);Button(L"Add launcher",29);Button(L"Cancel",IDCANCEL);Button(L"Save layout",IDOK,true);Reload();}
    int Selected()const{int row=static_cast<int>(SendMessageW(Get(21),LB_GETCURSEL,0,0));return row>=0&&row<static_cast<int>(entries.size())?entries[row]:-1;}
    void Directions(){int selected=Selected();Value(22,L"Open: "+std::wstring(DirectionName(draft.direction)));Value(30,selected<0?L"Build direction":L"Build: "+std::wstring(DirectionName(draft.groupDirections[selected])));EnableWindow(Get(30),selected>=0);}
    void Reload(int selected=-1){if(selected<0)selected=Selected();entries.clear();SendMessageW(Get(21),LB_RESETCONTENT,0,0);for(int i=0;i<static_cast<int>(StoreCount);++i)if(!draft.launchers[i].name.empty()){entries.push_back(i);SendMessageW(Get(21),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(draft.launchers[i].name.c_str()));if(i==selected)SendMessageW(Get(21),LB_SETCURSEL,entries.size()-1,0);}if(Selected()<0)SendMessageW(Get(21),LB_SETCURSEL,0,0);Directions();EnableWindow(Get(29),entries.size()<StoreCount);}
    void Layout(int w,int)override{Place(20,24,20,w-48,24);Place(21,24,58,w-48,212);Place(27,24,284,156,34);Place(28,192,284,164,34);Place(29,368,284,156,34);Place(22,24,338,(w-60)/2,38);Place(30,36+(w-60)/2,338,(w-60)/2,38);Place(23,24,392,280,34);Place(24,330,392,280,34);Place(25,24,446,w-48,58);Place(26,24,526,144,34);Place(IDCANCEL,w-268,526,112,34);Place(IDOK,w-144,526,120,34);}
    void Item(DRAWITEMSTRUCT* d)override{if(d->itemID>=entries.size())return;int i=entries[d->itemID];auto r=d->rcItem;Box(d->hDC,r,d->itemState&ODS_SELECTED?ui::Selected:ui::Panel);RECT sample{r.left+S(10),r.top+S(8),r.left+S(46),r.bottom-S(8)};Box(d->hDC,sample,draft.backgrounds[i],8);sample.right=sample.left+S(4);Box(d->hDC,sample,draft.colors[i]);r.left+=S(60);r.right-=S(12);r.bottom=r.top+S(28);TextAt(d->hDC,draft.launchers[i].name+(draft.launchers[i].visible?L"  ·  Shown":L"  ·  Hidden"),r,ui::Text,bold);r.top+=S(26);r.bottom+=S(22);TextAt(d->hDC,i<BuiltinCount?L"Installed-game discovery":std::wstring(LauncherKindName(draft.launchers[i].kind))+(HasDiscovery(draft.launchers[i].kind)?L" · Installed-game discovery":L" · Manual game entries"),r,ui::Muted);}
    void Command(int id,int code)override{
        if(id==IDCANCEL){End(0);return;}int selected=Selected();
        if(id==21&&code==LBN_SELCHANGE){Directions();return;}
        if(id==30&&selected>=0){auto& direction=draft.groupDirections[selected];do{direction=static_cast<Direction>((static_cast<int>(direction)+1)%4);}while(!Allowed(draft.direction,direction));Directions();}
        if(id==27&&selected>=0){draft.launchers[selected].visible=!draft.launchers[selected].visible;Reload(selected);}
        if(id==28||(id==21&&code==LBN_DBLCLK)||id==29){if(id==29){selected=-1;for(int i=BuiltinCount;i<static_cast<int>(StoreCount);++i)if(draft.launchers[i].name.empty()){selected=i;break;}}if(selected>=0){LauncherWindow editor(draft,selected);editor.Create(L"Launcher setup",620,466,hwnd);if(editor.Modal(hwnd)==IDOK)Reload(selected);}}
        if(id==22){draft.direction=static_cast<Direction>((static_cast<int>(draft.direction)+1)%4);for(auto& dir:draft.groupDirections)if(!Allowed(draft.direction,dir))dir=draft.direction;Reload();}if(id==26)draft.x=draft.y=-1;
        if(id==IDOK){draft.topmost=Checked(23);draft.locked=Checked(24);auto before=app.state;app.state=draft;if(!app.Persist()){app.state=before;return;}app.Apply();if(DiscoveryChanged(before.launchers,app.state.launchers)){app.Collapse();app.library.games.erase(std::remove_if(app.library.games.begin(),app.library.games.end(),[](const Game& game){return static_cast<int>(game.store)>=BuiltinCount;}),app.library.games.end());app.Refresh();}End(IDOK);}
    }
};
GameLauncher::Impl::Impl(HWND h,std::wstring d,Log l):owner(h),directory(std::move(d)),log(std::move(l)){
    std::wstring error;if(!games::Load(directory,state,error))log(error);widget=std::make_unique<Widget>(*this);Popup(*widget,L"Elka Game Launcher",438,160);drawer=std::make_unique<Drawer>(*this);Popup(*drawer,L"Elka Game Favorites",440,230);PlaceWidget();if(state.enabled){ShowWindow(widget->hwnd,SW_SHOWNOACTIVATE);Refresh();}
}
GameLauncher::Impl::~Impl(){closing=true;if(scanner.joinable())scanner.join();if(drawer&&IsWindow(drawer->hwnd))drawer->End(0);if(widget&&IsWindow(widget->hwnd))widget->End(0);}
void GameLauncher::Impl::Refresh(){if(closing)return;if(scanning){rescan=true;return;}if(scanner.joinable())scanner.join();scanning=true;auto launchers=state.launchers;InvalidateRect(widget->hwnd,nullptr,TRUE);scanner=std::thread([this,launchers]{games::Library result;try{result=Discover(launchers);}catch(...){result.notes.push_back(L"Game scan could not finish. Refresh to try again.");}{std::lock_guard lock(scanMutex);scanResult=std::move(result);}PostMessageW(widget->hwnd,Scanned,0,0);});}
void GameLauncher::Impl::ScanDone(){if(scanner.joinable())scanner.join();scanning=false;if(rescan){rescan=false;Refresh();return;}{std::lock_guard lock(scanMutex);library=std::move(scanResult);}scanned=true;gameArtwork.clear();log(L"Game library refreshed · "+std::to_wstring(library.games.size())+L" installed games");Apply();if(browser)PostMessageW(browser,LibraryChanged,0,0);}
void GameLauncher::Impl::PlaceWidget(){if(placing)return;placing=true;auto screen=CurrentScreen(state.screen);int x=screen.work.left+20,y=screen.work.top+40;SetWindowPos(widget->hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);dock=LayoutDock(static_cast<int>(VisibleLaunchers(state).size()),MulDiv(screen.work.right-screen.work.left,96,widget->dpi),state.toolbarVisible);int width=widget->S(dock.width),height=widget->S(dock.height);if(state.x>=0)x=screen.work.left+widget->S(state.x);else if(state.direction==Direction::Left)x=screen.work.right-width-widget->S(24);if(state.y>=0)y=screen.work.top+widget->S(state.y);else if(state.direction==Direction::Up)y=screen.work.bottom-height-widget->S(24);x=std::clamp(x,int(screen.work.left),(std::max)(int(screen.work.left),int(screen.work.right)-width));y=std::clamp(y,int(screen.work.top),(std::max)(int(screen.work.top),int(screen.work.bottom)-height));SetWindowPos(widget->hwnd,state.topmost?HWND_TOPMOST:HWND_NOTOPMOST,x,y,width,height,SWP_NOACTIVATE);placing=false;}
void GameLauncher::Impl::Apply(){
    for(size_t i=0;i<StoreCount;++i){auto path=state.launchers[i].visible?(state.launchers[i].executable.empty()?library.clients[i]:state.launchers[i].executable):L"";if(path!=artworkPaths[i]){artwork[i].Load(path);artworkPaths[i]=path;}}
    if(expanded>=0&&!state.launchers[expanded].visible)Collapse();PlaceWidget();RECT client{};GetClientRect(widget->hwnd,&client);widget->Layout(MulDiv(client.right,96,widget->dpi),MulDiv(client.bottom,96,widget->dpi));InvalidateRect(widget->hwnd,nullptr,TRUE);for(int i=0;i<static_cast<int>(StoreCount);++i)InvalidateRect(widget->Get(TileBase+i),nullptr,TRUE);if(!state.enabled){Collapse();ShowWindow(widget->hwnd,SW_HIDE);}else if(expanded>=0)Expand(expanded,false);if(browser)PostMessageW(browser,LibraryChanged,0,0);}
void GameLauncher::Impl::Expand(int store,bool animate){expanded=store;if(animate)page=0;RECT dockRect{},tile{};GetWindowRect(widget->hwnd,&dockRect);GetWindowRect(widget->Get(TileBase+store),&tile);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(widget->hwnd,MONITOR_DEFAULTTONEAREST),&mi);auto list=Favorites(store);auto layout=LayoutDrawer(dockRect,tile,mi.rcWork,static_cast<int>(list.size()),state.direction,state.groupDirections[store],widget->dpi,state.drawerToolbarVisible[store]);
    ShowWindow(drawer->hwnd,SW_HIDE);SetWindowPos(drawer->hwnd,nullptr,layout.bounds.left,layout.bounds.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);drawer->Set(store,std::move(list),std::move(layout));drawer->BeginReveal(animate);ShowWindow(drawer->hwnd,SW_SHOWNOACTIVATE);}
void GameLauncher::Impl::Collapse(){expanded=-1;page=0;if(drawer){drawer->StopReveal();ShowWindow(drawer->hwnd,SW_HIDE);}}
void GameLauncher::Impl::Options(HWND parent){OptionsWindow window(*this);window.Create(L"Floating shortcuts · launchers & colors",680,592,parent);window.Modal(parent);}
void GameLauncher::Impl::Browse(HWND parent,int store){if(browser){SetForegroundWindow(browser);return;}if(!scanned)Refresh();Browser window(*this,store);window.Create(L"Installed games & favorites",820,620,parent);browser=window.hwnd;window.Modal(parent);browser=nullptr;}
std::optional<Target> GameLauncher::Impl::Pick(HWND parent){if(!scanned)Refresh();Browser window(*this,0,true);window.Create(L"Add a game launch target",820,620,parent);browser=window.hwnd;window.Modal(parent);browser=nullptr;return window.picked;}
void GameLauncher::Impl::LaunchGame(const Game& game){DWORD pid=0;std::wstring error;if(!elka::Launch(game.target,pid,error)){log(L"Game launch failed · "+game.name+L" · "+error);ui::Notice(browser?browser:widget->hwnd,L"Could not launch game",error);return;}log(L"Launch request sent · "+game.name+L" · "+Name(static_cast<int>(game.store)));Collapse();}
GameLauncher::GameLauncher(HWND h,std::wstring d,Log l):impl(std::make_unique<Impl>(h,std::move(d),std::move(l))){}
GameLauncher::~GameLauncher()=default;
bool GameLauncher::Enabled()const{return impl->state.enabled;}
bool GameLauncher::Enable(bool enabled){bool old=impl->state.enabled;impl->state.enabled=enabled;if(!impl->Persist()){impl->state.enabled=old;return false;}impl->Apply();if(enabled){Show();if(!impl->scanned)impl->Refresh();}return true;}
void GameLauncher::Show(){if(!impl->state.enabled)return;impl->PlaceWidget();ShowWindow(impl->widget->hwnd,SW_SHOWNOACTIVATE);}
void GameLauncher::Options(HWND owner){impl->Options(owner);}
void GameLauncher::Library(HWND owner){impl->Browse(owner);}
std::optional<Target> GameLauncher::PickTarget(HWND owner){return impl->Pick(owner);}
}
