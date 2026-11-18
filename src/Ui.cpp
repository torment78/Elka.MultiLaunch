#include "Ui.h"
#include "Platform.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <filesystem>
#include <algorithm>

namespace ui {
static HBRUSH background=CreateSolidBrush(Background),field=CreateSolidBrush(Field);
static LRESULT CALLBACK ListProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(msg==WM_MOUSEWHEEL){int top=static_cast<int>(SendMessageW(h,LB_GETTOPINDEX,0,0));int count=static_cast<int>(SendMessageW(h,LB_GETCOUNT,0,0));int next=std::clamp(top-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*3,0,(std::max)(0,count-1));SendMessageW(h,LB_SETTOPINDEX,next,0);return 0;}
    return DefSubclassProc(h,msg,w,l);
}
Window::~Window(){if(font)DeleteObject(font);if(bold)DeleteObject(bold);if(title)DeleteObject(title);}
void Window::Fonts(){
    for(auto f:{font,bold,title})if(f)DeleteObject(f);
    font=CreateFontW(-S(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    bold=CreateFontW(-S(14),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    title=CreateFontW(-S(28),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);auto height=reinterpret_cast<INT_PTR>(GetPropW(child,L"rowHeight"));if(height)SendMessageW(child,LB_SETITEMHEIGHT,0,S(static_cast<int>(height)));}
}
HWND Window::Create(const std::wstring& caption,int width,int height,HWND owner,bool resize){
    static bool registered=false;
    if(!registered){WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"Elka.MultiLaunch.Window";wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(101));wc.hIconSm=wc.hIcon;RegisterClassExW(&wc);registered=true;}
    dpi=owner?GetDpiForWindow(owner):GetDpiForSystem();DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;
    if(resize)style|=WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
    RECT r{0,0,S(width),S(height)};AdjustWindowRectExForDpi(&r,style,FALSE,WS_EX_CONTROLPARENT,dpi);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int x=work.left+(work.right-work.left-(r.right-r.left))/2,y=work.top+(work.bottom-work.top-(r.bottom-r.top))/2;
    return CreateWindowExW(WS_EX_CONTROLPARENT,L"Elka.MultiLaunch.Window",caption.c_str(),style,x,(std::max)(static_cast<int>(work.top),y),r.right-r.left,r.bottom-r.top,owner,nullptr,GetModuleHandleW(nullptr),this);
}
int Window::Modal(HWND owner){EnableWindow(owner,FALSE);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);MSG msg{};
    while(!done&&GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){Close();continue;}if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    EnableWindow(owner,TRUE);SetForegroundWindow(owner);return result;
}
void Window::End(int code){result=code;done=true;DestroyWindow(hwnd);}
HWND Window::Control(const wchar_t* type,const std::wstring& caption,int id,DWORD style){HWND child=CreateWindowExW(0,type,caption.c_str(),WS_CHILD|WS_VISIBLE|style,0,0,1,1,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SetWindowTheme(child,L"",L"");return child;}
HWND Window::Label(const std::wstring& text,int id){return Control(L"STATIC",text,id,SS_LEFT);}
HWND Window::Button(const std::wstring& text,int id,bool accent){auto h=Control(L"BUTTON",text,id,WS_TABSTOP|BS_OWNERDRAW);if(accent)SetPropW(h,L"accent",reinterpret_cast<HANDLE>(1));return h;}
HWND Window::Check(const std::wstring& text,int id,bool value){auto h=Button(text,id);SetPropW(h,L"check",reinterpret_cast<HANDLE>(1));Checked(id,value);return h;}
HWND Window::Edit(const std::wstring& text,int id,bool readOnly){auto h=Control(L"EDIT",text,id,WS_TABSTOP|ES_AUTOHSCROLL|(readOnly?ES_READONLY:0)|WS_BORDER);SendMessageW(h,EM_SETLIMITTEXT,32000,0);SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(8),S(8)));return h;}
HWND Window::List(int id,int rowHeight){auto h=Control(L"LISTBOX",L"",id,WS_TABSTOP|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT);SetPropW(h,L"rowHeight",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rowHeight)));SendMessageW(h,LB_SETITEMHEIGHT,0,S(rowHeight));SetWindowSubclass(h,ListProc,1,0);return h;}
std::wstring Window::Value(int id)const{auto h=Get(id);int n=GetWindowTextLengthW(h);std::wstring s(n+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
void Window::Value(int id,const std::wstring& s){SetWindowTextW(Get(id),s.c_str());}
bool Window::Checked(int id)const{return GetPropW(Get(id),L"checked")!=nullptr;}
void Window::Checked(int id,bool b){SetPropW(Get(id),L"checked",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(b)));InvalidateRect(Get(id),nullptr,TRUE);}
void Window::Place(int id,int x,int y,int w,int h){MoveWindow(Get(id),S(x),S(y),S(w),S(h),TRUE);}
void Window::Show(int id,bool show){ShowWindow(Get(id),show?SW_SHOW:SW_HIDE);}
void Window::TextAt(HDC dc,const std::wstring& text,RECT r,COLORREF color,HFONT use,UINT format)const{SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);auto old=SelectObject(dc,use?use:font);DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&r,format);SelectObject(dc,old);}
void Window::Box(HDC dc,RECT r,COLORREF color,int radius)const{auto brush=CreateSolidBrush(color);if(radius){auto old=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,S(radius),S(radius));SelectObject(dc,pen);SelectObject(dc,old);}else FillRect(dc,&r,brush);DeleteObject(brush);}
LRESULT CALLBACK Window::Proc(HWND h,UINT m,WPARAM w,LPARAM l){
    Window* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,m,w,l);
    bool handled=false;auto result=self->Message(m,w,l,handled);if(handled)return result;
    switch(m){
    case WM_CREATE:{self->Fonts();BOOL dark=TRUE;DwmSetWindowAttribute(h,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));COLORREF bg=Background,fg=Text;DwmSetWindowAttribute(h,DWMWA_CAPTION_COLOR,&bg,sizeof(bg));DwmSetWindowAttribute(h,DWMWA_TEXT_COLOR,&fg,sizeof(fg));self->Created();return 0;}
    case WM_SIZE:self->Layout(MulDiv(LOWORD(l),96,self->dpi),MulDiv(HIWORD(l),96,self->dpi));return 0;
    case WM_DPICHANGED:{self->dpi=HIWORD(w);self->Fonts();auto r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
    case WM_COMMAND:if(HIWORD(w)==BN_CLICKED&&GetPropW(reinterpret_cast<HWND>(l),L"check"))self->Checked(LOWORD(w),!self->Checked(LOWORD(w)));self->Command(LOWORD(w),HIWORD(w));return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);FillRect(dc,&r,background);self->Paint(dc,r);EndPaint(h,&ps);return 0;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,Text);SetBkColor(dc,m==WM_CTLCOLORSTATIC?Background:Field);return reinterpret_cast<LRESULT>(m==WM_CTLCOLORSTATIC?background:field);}
    case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(d->CtlType!=ODT_BUTTON||GetPropW(d->hwndItem,L"customDraw")){self->Item(d);return TRUE;}
        bool checked=GetPropW(d->hwndItem,L"checked"),isCheck=GetPropW(d->hwndItem,L"check"),accent=GetPropW(d->hwndItem,L"accent"),disabled=(d->itemState&ODS_DISABLED)!=0;
        auto r=d->rcItem;
        // Owner-drawn buttons must paint the entire rectangle first. Otherwise
        // the native button surface remains visible outside the rounded face.
        self->Box(d->hDC,r,Background);
        self->Box(d->hDC,r,isCheck?Background:accent?Accent:Field,8);RECT textRect=r;
        if(isCheck){RECT square{r.left+self->S(2),r.top+self->S(8),r.left+self->S(20),r.top+self->S(26)};self->Box(d->hDC,square,checked?Accent:Edge,4);if(checked)self->TextAt(d->hDC,L"✓",square,Background,self->bold,DT_CENTER|DT_VCENTER|DT_SINGLELINE);textRect.left+=self->S(30);}
        wchar_t caption[256]{};GetWindowTextW(d->hwndItem,caption,256);self->TextAt(d->hDC,caption,textRect,disabled?Muted:accent?Background:Text,self->font,(isCheck?DT_LEFT:DT_CENTER)|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(d->itemState&(ODS_FOCUS|ODS_SELECTED)){InflateRect(&r,-self->S(3),-self->S(3));auto pen=CreatePen(PS_SOLID,1,accent?Background:Accent);auto old=SelectObject(d->hDC,pen);auto b=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,self->S(6),self->S(6));SelectObject(d->hDC,b);SelectObject(d->hDC,old);DeleteObject(pen);}return TRUE;}
    case WM_MEASUREITEM:{auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);item->itemHeight=self->S(item->CtlType==ODT_MENU?34:48);if(item->CtlType==ODT_MENU)item->itemWidth=self->S(214);return TRUE;}
    case WM_CLOSE:self->Close();return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
class NoticeWindow:public Window {
    std::wstring message;bool confirm;
public:
    NoticeWindow(std::wstring s,bool c):message(std::move(s)),confirm(c){}
    void Created()override{Button(confirm?L"Continue":L"OK",IDOK,true);if(confirm)Button(L"Cancel",IDCANCEL);}
    void Layout(int w,int h)override{Place(IDOK,w-132,h-56,108,34);if(confirm)Place(IDCANCEL,w-252,h-56,108,34);}
    void Paint(HDC dc,RECT r)override{r.left+=S(24);r.top+=S(20);r.right-=S(24);r.bottom-=S(76);TextAt(dc,message,r,Text,font,DT_LEFT|DT_WORDBREAK);}
    void Command(int id,int)override{if(id==IDOK||id==IDCANCEL)End(id);}
};
bool Notice(HWND owner,const std::wstring& title,const std::wstring& text,bool confirm){NoticeWindow w(text,confirm);w.Create(title,560,230,owner);return w.Modal(owner)==IDOK;}
class ProcessPicker:public Window {
    std::vector<elka::Process> processes;std::vector<size_t> filtered;
public:
    std::wstring selected;
    void Created()override{Label(L"Find a running application",10);Edit(L"",11);Button(L"Refresh",12);List(13,54);Label(L"",14);Button(L"Cancel",IDCANCEL);Button(L"Use application",IDOK,true);Refresh();SetFocus(Get(11));}
    void Layout(int w,int h)override{Place(10,24,18,w-48,22);Place(11,24,48,w-172,30);Place(12,w-136,48,112,30);Place(13,24,92,w-48,h-206);Place(14,24,h-100,w-48,40);Place(IDCANCEL,w-280,h-50,112,34);Place(IDOK,w-156,h-50,132,34);}
    void Refresh(){processes=elka::Processes(true);Filter();}
    void Filter(){auto query=elka::Key(Value(11));filtered.clear();SendMessageW(Get(13),LB_RESETCONTENT,0,0);for(size_t i=0;i<processes.size();++i){const auto& p=processes[i];if(query.empty()||elka::Key(p.name+L" "+p.path+L" "+std::to_wstring(p.pid)).find(query)!=std::wstring::npos){filtered.push_back(i);SendMessageW(Get(13),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.name.c_str()));}}Value(14,std::to_wstring(filtered.size())+L" processes · filter by name, path or PID. Scroll for more.\nProtected processes are listed; use the EXE picker if their path is unavailable.");}
    void Command(int id,int code)override{if(id==11&&code==EN_CHANGE)Filter();else if(id==12)Refresh();else if(id==IDCANCEL)End(0);else if(id==IDOK||(id==13&&code==LBN_DBLCLK)){auto index=SendMessageW(Get(13),LB_GETCURSEL,0,0);if(index<0||static_cast<size_t>(index)>=filtered.size())return;auto& p=processes[filtered[index]];if(p.path.empty()){Notice(hwnd,L"Protected process",L"Windows did not allow access to this process's EXE path. Select the application's EXE file instead, or restart Multi-Launch as administrator.");return;}selected=p.path;End(IDOK);}}
    void Item(DRAWITEMSTRUCT* d)override{if(d->itemID>=filtered.size())return;auto& p=processes[filtered[d->itemID]];Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?Selected:Field);RECT r=d->rcItem;r.left+=S(12);r.right-=S(12);r.bottom=r.top+S(28);TextAt(d->hDC,p.name+L"    ·    PID "+std::to_wstring(p.pid),r,Text,bold);r.top+=S(27);r.bottom+=S(22);TextAt(d->hDC,p.path.empty()?L"Protected / system process · EXE path unavailable":p.path,r,Muted);}
};
std::wstring PickProcess(HWND owner){ProcessPicker p;p.Create(L"Select a running process",820,620,owner,true);p.Modal(owner);return p.selected;}
class ExePicker:public Window {
    struct Entry{std::wstring name,path;bool folder;};std::vector<Entry> entries;std::wstring current,initial;
public:
    std::wstring selected;
    explicit ExePicker(std::wstring previous):initial(std::move(previous)){}
    void Created()override{Label(L"Folder or full EXE path",10);Edit(L"",11);Button(L"Go",12);Button(L"Up",13);Button(L"Drives",14);List(15,36);Label(L"Double-click a folder or application. You can also paste a full EXE path above.",16);Button(L"Cancel",IDCANCEL);Button(L"Use application",IDOK,true);auto folder=elka::Parent(initial);if(folder.empty()){wchar_t p[32768]{};GetEnvironmentVariableW(L"ProgramFiles",p,32768);folder=p;}Navigate(folder);}
    void Layout(int w,int h)override{Place(10,24,18,w-48,22);Place(11,24,48,w-298,30);Place(12,w-264,48,64,30);Place(13,w-190,48,70,30);Place(14,w-110,48,86,30);Place(15,24,94,w-48,h-202);Place(16,24,h-94,w-48,38);Place(IDCANCEL,w-280,h-50,112,34);Place(IDOK,w-156,h-50,132,34);}
    void Navigate(std::wstring path){
        if(path.size()>=2&&path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);
        if(elka::IsExe(path)){selected=path;End(IDOK);return;}
        std::error_code ec;std::vector<Entry> next;
        if(path.empty()){wchar_t drives[512]{};GetLogicalDriveStringsW(512,drives);for(auto p=drives;*p;p+=wcslen(p)+1)next.push_back({p,p,true});}
        else{auto full=std::filesystem::absolute(path,ec);if(ec||!std::filesystem::is_directory(full,ec)){Value(16,L"Folder not found. Enter a folder path or select an .exe file.");return;}path=full.wstring();
            std::filesystem::directory_iterator it(full,std::filesystem::directory_options::skip_permission_denied,ec),end;
            if(ec){Value(16,L"This folder could not be opened: "+elka::ErrorText(static_cast<DWORD>(ec.value())));return;}
            for(;it!=end;it.increment(ec)){if(ec)break;auto name=it->path().filename().wstring();bool folder=it->is_directory(ec);auto ext=elka::Key(it->path().extension().wstring());if(folder||ext==L".exe")next.push_back({name,it->path().wstring(),folder});}
            std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.folder!=b.folder?a.folder>b.folder:elka::Key(a.name)<elka::Key(b.name);});
        }
        entries=std::move(next);current=path;Value(11,path);SendMessageW(Get(15),LB_RESETCONTENT,0,0);for(const auto& e:entries)SendMessageW(Get(15),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(e.name.c_str()));Value(16,std::to_wstring(entries.size())+L" folders and EXE files · scroll for more. Double-click to open.\nYou can paste a folder or full EXE path above, then select Go.");
    }
    void Command(int id,int code)override{if(id==IDCANCEL)End(0);else if(id==12)Navigate(Value(11));else if(id==13){auto p=std::filesystem::path(current).parent_path();Navigate(p==std::filesystem::path(current)?L"":p.wstring());}else if(id==14)Navigate(L"");else if(id==IDOK||(id==15&&code==LBN_DBLCLK)){auto index=SendMessageW(Get(15),LB_GETCURSEL,0,0);if(index>=0&&static_cast<size_t>(index)<entries.size())Navigate(entries[index].path);else if(Value(11)!=current)Navigate(Value(11));}}
    void Item(DRAWITEMSTRUCT* d)override{if(d->itemID>=entries.size())return;const auto& e=entries[d->itemID];Box(d->hDC,d->rcItem,d->itemState&ODS_SELECTED?Selected:Field);RECT r=d->rcItem;r.left+=S(12);r.right-=S(12);TextAt(d->hDC,(e.folder?L"▸   ":L"     ")+e.name,r,e.folder?Text:Accent);}
};
std::wstring PickExe(HWND owner,const std::wstring& previous){ExePicker p(previous);p.Create(L"Select an EXE file",820,620,owner,true);p.Modal(owner);return p.selected;}
}
