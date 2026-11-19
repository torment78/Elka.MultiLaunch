#include "../src/IconArtwork.h"
#include "../src/Ui.h"
#include "../src/Platform.h"
#include <filesystem>
#include <stdexcept>
#include <algorithm>

namespace {
struct Surface {
    static constexpr int Size=512;
    HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=nullptr;HGDIOBJ previous=nullptr;DWORD* pixels=nullptr;
    Surface(){
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=Size;info.bmiHeader.biHeight=-Size;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!dc||!bitmap)throw std::runtime_error("render regression surface creation");previous=SelectObject(dc,bitmap);Clear();
    }
    ~Surface(){SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);}
    void Clear(){GdiFlush();std::fill_n(pixels,Size*Size,0x00333F51);}
    bool Same(const Surface& other)const{GdiFlush();for(int i=0;i<Size*Size;++i)if((pixels[i]&0xffffff)!=(other.pixels[i]&0xffffff))return false;return true;}
};
class RenderFixture:public ui::Window {
public:
    ui::IconArtwork* artwork=nullptr;int draws=0;
    ~RenderFixture(){if(IsWindow(hwnd))DestroyWindow(hwnd);}
    void Created()override{auto button=Button(L"",100);SetPropW(button,L"customDraw",reinterpret_cast<HANDLE>(1));}
    void Paint(HDC dc,RECT)override{RECT marker{4,4,12,12};Box(dc,marker,RGB(220,40,100));}
    void Item(DRAWITEMSTRUCT* d)override{++draws;auto f=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));ui::DrawLauncherTile(d->hDC,d->rcItem,L"",L"",ui::Accent,f,f,96,0,artwork,RGB(110,110,110));}
};
}
int ArtworkChecks(){
    int count=0;auto check=[&](bool value,const char* message){++count;if(!value)throw std::runtime_error(message);};
    ui::IconArtwork icon;auto file=std::filesystem::path(elka::Parent(elka::ExePath()))/L".."/L".."/L"assets"/L"app.ico";
    check(icon.Load(file.wstring(),0,128),"load repository artwork fixture");
    Surface reference,translated;auto font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));DWORD handles=0;
    // Warm every DPI path once before checking Windows' retained GDI caches.
    for(int pass=0;pass<2;++pass){
    for(auto dpi:{96u,144u,192u})for(auto color:{RGB(100,100,100),RGB(20,70,110),RGB(120,30,40)})for(bool windowOrigin:{false,true}){
        reference.Clear();translated.Clear();int width=MulDiv(132,dpi,96),height=MulDiv(74,dpi,96);RECT target{80,184,80+width,184+height};
        int first=SaveDC(reference.dc),second=SaveDC(translated.dc);auto clip=CreateRectRgn(84,188,target.right-4,target.bottom-4);SelectClipRgn(reference.dc,clip);SelectClipRgn(translated.dc,clip);DeleteObject(clip);
        ui::DrawLauncherTile(reference.dc,target,L"Test",L"Play",ui::Accent,font,font,dpi,0,&icon,color);
        if(windowOrigin)SetWindowOrgEx(translated.dc,-80,-184,nullptr);else SetViewportOrgEx(translated.dc,80,184,nullptr);
        ui::DrawLauncherTile(translated.dc,{0,0,width,height},L"Test",L"Play",ui::Accent,font,font,dpi,0,&icon,color);
        // The caller's clip must survive drawing, including during animation capture.
        SetPixelV(reference.dc,84,188,RGB(10,20,30));SetPixelV(translated.dc,4,4,RGB(10,20,30));
        SetPixelV(reference.dc,80,184,RGB(255,0,0));SetPixelV(translated.dc,0,0,RGB(255,0,0));
        RestoreDC(reference.dc,first);RestoreDC(translated.dc,second);
        check(reference.Same(translated),"translated animation drawing must match ordinary painting at every DPI/background");
    }
        if(pass==0)handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    }
    check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==handles,"translated drawing must not leak GDI resources");
    RenderFixture window;window.artwork=&icon;check(window.Create(L"Artwork regression",300,200)!=nullptr,"create hidden native render fixture");
    translated.Clear();int saved=SaveDC(translated.dc);SetViewportOrgEx(translated.dc,80,184,nullptr);
    SendMessageW(window.hwnd,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(translated.dc),PRF_CLIENT);RestoreDC(translated.dc,saved);
    check(GetPixel(translated.dc,86,190)==RGB(220,40,100)&&GetPixel(translated.dc,100,204)==ui::Background,"animation capture paints the custom window client area");
    MoveWindow(window.Get(100),16,40,264,148,FALSE);reference.Clear();translated.Clear();
    ui::DrawLauncherTile(reference.dc,{80,184,344,332},L"",L"",ui::Accent,font,font,96,0,&icon,RGB(110,110,110));
    saved=SaveDC(translated.dc);SetViewportOrgEx(translated.dc,80,184,nullptr);
    SendMessageW(window.Get(100),WM_PRINT,reinterpret_cast<WPARAM>(translated.dc),PRF_CLIENT);RestoreDC(translated.dc,saved);
    check(window.draws>0&&reference.Same(translated),"native owner-drawn button animation capture retains complete centered artwork");
    return count;
}
