#include "../src/Games.h"
#include "../src/IconArtwork.h"
#include "../src/Ui.h"
#include "../src/GameLauncher.h"
#include <commctrl.h>
#include <thread>
#include <algorithm>
#include <stdexcept>

namespace {
bool Same(RECT a,RECT b){return EqualRect(&a,&b)!=FALSE;}
bool GrowingFrom(RECT visible,RECT full,elka::games::Direction direction){
    using elka::games::Direction;
    switch(direction){
    case Direction::Up:return visible.bottom==full.bottom&&visible.top>full.top&&visible.left==full.left&&visible.right==full.right;
    case Direction::Down:return visible.top==full.top&&visible.bottom<full.bottom&&visible.left==full.left&&visible.right==full.right;
    case Direction::Left:return visible.right==full.right&&visible.left>full.left&&visible.top==full.top&&visible.bottom==full.bottom;
    case Direction::Right:return visible.left==full.left&&visible.right<full.right&&visible.top==full.top&&visible.bottom==full.bottom;
    }
    return false;
}
}
int DrawerChecks(){
    using namespace elka::games;int count=0;auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};
    check(DrawerRevealDuration(0)==0&&RevealDrawer({},Direction::Up,Direction::Left,0).finished,"empty drawers require no animation timer");
    check(DrawerRevealDuration(1)==260&&DrawerRevealDuration(5)==740&&DrawerRevealDuration(1000)<=1500,"single nodes and long favorite lists have bounded animation duration");
    for(auto dpi:{96u,192u})for(bool controls:{false,true})for(int first=0;first<4;++first)for(int flow=0;flow<4;++flow){
        auto opening=static_cast<Direction>(first),build=static_cast<Direction>(flow);if(!Allowed(opening,build))continue;
        auto p=LayoutDrawer({-1800,900,-1500,1100},{-1788,950,-1524,1098},{-3600,0,0,2160},5,opening,build,dpi,controls);std::vector<RECT> nodes;
        for(auto point:p.cells)nodes.push_back({point.x,point.y,point.x+MulDiv(132,dpi,96),point.y+MulDiv(74,dpi,96)});
        auto firstFrame=RevealDrawer(nodes,opening,build,130),turn=RevealDrawer(nodes,opening,build,300),end=RevealDrawer(nodes,opening,build,DrawerRevealDuration(nodes.size()));
        bool okay=nodes.size()==5&&firstFrame.nodes.size()==1&&GrowingFrom(firstFrame.nodes[0],nodes[0],opening)&&turn.nodes.size()==2&&Same(turn.nodes[0],nodes[0])&&GrowingFrom(turn.nodes[1],nodes[1],build)&&end.finished&&end.nodes.size()==nodes.size();
        std::vector<LONG> areas(nodes.size(),0);
        for(unsigned elapsed=0;elapsed<DrawerRevealDuration(nodes.size())+20;elapsed+=16){
            auto frame=RevealDrawer(nodes,opening,build,elapsed);
            for(size_t i=0;i<frame.nodes.size();++i){auto r=frame.nodes[i],full=nodes[i];auto area=(r.right-r.left)*(r.bottom-r.top);okay=okay&&r.left>=full.left&&r.top>=full.top&&r.right<=full.right&&r.bottom<=full.bottom&&area>=areas[i];areas[i]=area;}
        }
        for(size_t i=0;i<end.nodes.size();++i)okay=okay&&Same(end.nodes[i],nodes[i]);
        check(okay,"drawer reveals outward, then builds in the chosen direction with stable artwork bounds at both display scales");
    }
    auto wrapped=LayoutDrawer({300,260,450,380},{310,300,442,374},{0,0,800,600},100,Direction::Up,Direction::Left,96,false);std::vector<RECT> nodes;
    for(auto point:wrapped.cells)nodes.push_back({point.x,point.y,point.x+132,point.y+74});
    auto final=RevealDrawer(nodes,Direction::Up,Direction::Left,1500);
    check(final.finished&&final.nodes.size()==nodes.size()&&nodes.size()<=static_cast<size_t>(wrapped.capacity),"wrapped and paginated drawers finish with exactly the visible page's nodes");
    auto region=ui::CreateNodeRegion(RevealDrawer({{147,9,279,83},{9,9,141,83}},Direction::Up,Direction::Left,130).nodes,10);
    check(region&&PtInRegion(region,200,70)&&!PtInRegion(region,80,70)&&!PtInRegion(region,144,70),"unrevealed nodes and desktop gaps remain outside the native window region");if(region)DeleteObject(region);
    return count;
}
void DrawerPreview(){
    using namespace elka::games;
    constexpr int width=750,height=690;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;auto bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto dc=CreateCompatibleDC(nullptr);auto old=SelectObject(dc,bitmap);auto font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    RECT surface{0,0,width,height};auto brush=CreateSolidBrush(RGB(46,55,68));FillRect(dc,&surface,brush);DeleteObject(brush);ui::IconArtwork icon;icon.Load((std::filesystem::path(elka::Parent(elka::ExePath()))/L".."/L".."/L"assets"/L"app.ico").wstring(),0,128);
    const unsigned times[]{130,300,440,580,740};
    for(int row=0;row<5;++row){int y=20+row*134;std::vector<RECT> nodes;for(int i=0;i<5;++i)nodes.push_back({582-i*138,y,714-i*138,y+74});auto frame=RevealDrawer(nodes,Direction::Up,Direction::Left,times[row]);auto region=ui::CreateNodeRegion(frame.nodes,10);SelectClipRgn(dc,region);
        for(int i=0;i<5;++i)ui::DrawLauncherTile(dc,nodes[i],L"Game "+std::to_wstring(i+1),L"Play",RGB(100,181,246),font,font,96,0,&icon,RGB(40,70,94));SelectClipRgn(dc,nullptr);DeleteObject(region);
        ui::DrawLauncherTile(dc,{582,y+84,714,y+120},L"Launcher",L"",RGB(100,181,246),font,font,96,0,nullptr,RGB(24,30,38));RECT label{30,y+88,500,y+120};SetTextColor(dc,RGB(230,237,243));SetBkMode(dc,TRANSPARENT);auto text=L"Open up, build left · "+std::to_wstring(times[row])+L" ms";DrawTextW(dc,text.c_str(),-1,&label,DT_LEFT|DT_SINGLELINE);
    }
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));Microsoft::WRL::ComPtr<IWICBitmap> image;factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapIgnoreAlpha,&image);Microsoft::WRL::ComPtr<IWICStream> stream;factory->CreateStream(&stream);auto path=std::filesystem::path(elka::Parent(elka::ExePath()))/L"drawer-preview.png";stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;encoder->CreateNewFrame(&frame,nullptr);frame->Initialize(nullptr);auto result=frame->WriteSource(image.Get(),nullptr);if(SUCCEEDED(result))result=frame->Commit();if(SUCCEEDED(result))result=encoder->Commit();SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);if(FAILED(result))throw std::runtime_error("drawer preview PNG encoding failed");
}
int DrawerNativeChecks(){
    // Exercise the real drawer on an inactive test desktop, without taking focus
    // or opening fixture windows on the user's desktop.
    int count=0;std::exception_ptr failure;std::thread worker([&]{
        auto desktop=CreateDesktopW((L"ElkaDrawerTest-"+std::to_wstring(GetCurrentProcessId())).c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);auto original=GetThreadDesktop(GetCurrentThreadId());bool initialized=false;
        try{
            auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};
            check(desktop&&SetThreadDesktop(desktop),"create an inactive native drawer test desktop");CoInitializeEx(nullptr,COINIT_MULTITHREADED);initialized=true;
            auto directory=std::filesystem::path(elka::Parent(elka::ExePath()))/L"drawer-native-tests";elka::games::State state;state.direction=elka::games::Direction::Up;state.groupDirections[3]=elka::games::Direction::Left;state.drawerToolbarVisible[3]=false;state.launchers[3].visible=true;std::wstring error;
            for(int i=0;i<5;++i){elka::games::Game game{elka::games::Store::Other,std::to_wstring(i),L"Fixture",L"",{elka::ExePath(),L"",L"",false}};state.custom.push_back(game);state.favorites.insert(game.Key());}
            check(elka::games::Save(directory.wstring(),state,error),"save isolated animation fixtures");
            {
                elka::GameLauncher launcher(nullptr,directory.wstring(),[](const std::wstring&){});struct Windows{HWND dock=nullptr,drawer=nullptr;} windows;
                EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL{wchar_t title[80]{};GetWindowTextW(window,title,80);auto& found=*reinterpret_cast<Windows*>(data);if(std::wstring(title)==L"Elka Game Launcher")found.dock=window;if(std::wstring(title)==L"Elka Game Favorites")found.drawer=window;return TRUE;},reinterpret_cast<LPARAM>(&windows));
                check(windows.dock&&windows.drawer,"native launcher fixture created both windows");unsigned ticks=0;
                SetWindowSubclass(windows.drawer,[](HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data)->LRESULT{if(message==WM_TIMER&&w==1)++*reinterpret_cast<unsigned*>(data);return DefSubclassProc(window,message,w,l);},77,reinterpret_cast<DWORD_PTR>(&ticks));
                auto pump=[](unsigned duration){auto until=GetTickCount64()+duration;do{MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}if(GetTickCount64()<until)MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}while(GetTickCount64()<until);};
                auto click=[&]{SendMessageW(windows.dock,WM_COMMAND,MAKEWPARAM(1003,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(windows.dock,1003)));};
                BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);click();pump(120);auto partial=CreateRectRgn(0,0,0,0);GetWindowRgn(windows.drawer,partial);pump(1450);auto complete=CreateRectRgn(0,0,0,0);GetWindowRgn(windows.drawer,complete);
                check(IsWindowVisible(windows.drawer)&&(!animations||(!EqualRgn(partial,complete)&&ticks>3)),"real drawer opens progressively and survives initial show/layout messages");DeleteObject(partial);DeleteObject(complete);
                auto finishedTicks=ticks;pump(200);check(ticks==finishedTicks,"finished drawer has no animation timer wakeups");
                click();click();pump(80);click();pump(40);auto cancelledTicks=ticks;pump(200);check(!IsWindowVisible(windows.drawer)&&ticks==cancelledTicks,"closing a partial drawer cancels animation without reopening it");
                click();pump(30);click();click();pump(1550);finishedTicks=ticks;pump(200);check(IsWindowVisible(windows.drawer)&&ticks==finishedTicks,"rapid close/reopen replaces the previous animation and then becomes idle");
            }
        }catch(...){failure=std::current_exception();}
        if(initialized)CoUninitialize();SetThreadDesktop(original);if(desktop)CloseDesktop(desktop);
    });worker.join();if(failure)std::rethrow_exception(failure);return count;
}
