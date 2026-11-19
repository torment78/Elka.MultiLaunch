#include "IconArtwork.h"
#include "Ui.h"
#include <shlobj.h>

namespace ui {
IconArtwork::~IconArtwork(){if(bitmap)DeleteObject(bitmap);}
bool IconArtwork::Load(const std::wstring& executable){
    source.Reset();if(bitmap){DeleteObject(bitmap);bitmap=nullptr;}width=height=0;
    if(executable.empty())return false;
    if(!factory&&FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
    HICON icon=nullptr;
    if(SHDefExtractIconW(executable.c_str(),0,0,&icon,nullptr,256)!=S_OK||!icon)return false;
    Microsoft::WRL::ComPtr<IWICBitmap> original;
    auto result=factory->CreateBitmapFromHICON(icon,&original);DestroyIcon(icon);
    if(FAILED(result))return false;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(original.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    source=converter;return true;
}
bool IconArtwork::Draw(HDC dc,RECT rectangle,BYTE opacity){
    int w=rectangle.right-rectangle.left,h=rectangle.bottom-rectangle.top;
    if(!source||w<=0||h<=0||w>1024||h>1024)return false;
    if(!bitmap||w!=width||h!=height){
        Microsoft::WRL::ComPtr<IWICBitmapScaler> scaled;
        if(FAILED(factory->CreateBitmapScaler(&scaled))||FAILED(scaled->Initialize(source.Get(),w,h,WICBitmapInterpolationModeFant)))return false;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;HBITMAP next=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!next)return false;
        if(FAILED(scaled->CopyPixels(nullptr,w*4,w*h*4,static_cast<BYTE*>(pixels)))){DeleteObject(next);return false;}
        if(bitmap)DeleteObject(bitmap);bitmap=next;width=w;height=h;
    }
    HDC memory=CreateCompatibleDC(dc);if(!memory)return false;
    auto previous=SelectObject(memory,bitmap);BLENDFUNCTION blend{AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
    bool okay=AlphaBlend(dc,rectangle.left,rectangle.top,w,h,memory,0,0,w,h,blend)!=FALSE;
    SelectObject(memory,previous);DeleteDC(memory);return okay;
}
void DrawLauncherTile(HDC dc,RECT r,const std::wstring& name,const std::wstring& detail,
    COLORREF accent,HFONT font,HFONT bold,UINT dpi,UINT state,IconArtwork* artwork){
    auto scale=[&](int value){return MulDiv(value,static_cast<int>(dpi),96);};
    auto fill=[&](RECT box,COLORREF color,int radius){auto brush=CreateSolidBrush(color);auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));if(radius)RoundRect(dc,box.left,box.top,box.right,box.bottom,scale(radius),scale(radius));else FillRect(dc,&box,brush);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);};
    fill(r,Background,0);fill(r,state&ODS_SELECTED?Selected:Field,10);
    RECT stripe{r.left,r.top+scale(10),r.left+scale(3),r.bottom-scale(10)};fill(stripe,accent,2);
    if(artwork){int size=scale(84),left=r.right-size-scale(6),top=r.top+(r.bottom-r.top-size)/2;artwork->Draw(dc,{left,top,left+size,top+size},108);}
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,Text);auto oldFont=SelectObject(dc,bold);
    RECT text{r.left+scale(12),r.top+scale(8),r.right-scale(8),r.top+scale(45)};
    DrawTextW(dc,name.c_str(),static_cast<int>(name.size()),&text,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);
    SelectObject(dc,font);SetTextColor(dc,accent);text={r.left+scale(12),r.bottom-scale(24),r.right-scale(8),r.bottom};
    DrawTextW(dc,detail.c_str(),static_cast<int>(detail.size()),&text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);SelectObject(dc,oldFont);
    if(state&ODS_FOCUS){RECT focus=r;InflateRect(&focus,-scale(4),-scale(4));DrawFocusRect(dc,&focus);}
}
}
