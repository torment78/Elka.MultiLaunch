#include "IconArtwork.h"
#include "Ui.h"
#include <shlobj.h>
#include <algorithm>
#include <cmath>

namespace ui {
Hsv ColorToHsv(COLORREF color){
    double r=GetRValue(color)/255.0,g=GetGValue(color)/255.0,b=GetBValue(color)/255.0,high=(std::max)({r,g,b}),low=(std::min)({r,g,b}),delta=high-low;Hsv value;value.value=high;value.saturation=high>0?delta/high:0;
    if(delta>0){if(high==r)value.hue=60*std::fmod((g-b)/delta,6.0);else if(high==g)value.hue=60*((b-r)/delta+2);else value.hue=60*((r-g)/delta+4);if(value.hue<0)value.hue+=360;}return value;
}
COLORREF HsvToColor(Hsv value){
    double hue=std::fmod((std::max)(0.0,value.hue),360.0)/60,saturation=std::clamp(value.saturation,0.0,1.0),brightness=std::clamp(value.value,0.0,1.0),chroma=brightness*saturation,x=chroma*(1-std::abs(std::fmod(hue,2.0)-1)),m=brightness-chroma,r=0,g=0,b=0;
    if(hue<1){r=chroma;g=x;}else if(hue<2){r=x;g=chroma;}else if(hue<3){g=chroma;b=x;}else if(hue<4){g=x;b=chroma;}else if(hue<5){r=x;b=chroma;}else{r=chroma;b=x;}
    return RGB(int(std::lround((r+m)*255)),int(std::lround((g+m)*255)),int(std::lround((b+m)*255)));
}
HBITMAP CreateColorPlane(int width,int height,int shadeWidth,int hueStart,double hue){
    if(width<2||height<2||width>2048||height>2048||shadeWidth<2||shadeWidth>hueStart||hueStart>=width)return nullptr;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=nullptr;auto bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap)return nullptr;
    auto data=static_cast<DWORD*>(pixels);for(int y=0;y<height;++y)for(int x=0;x<width;++x){COLORREF value=Background;if(x<shadeWidth)value=HsvToColor({hue,double(x)/(shadeWidth-1),1-double(y)/(height-1)});else if(x>=hueStart)value=HsvToColor({359.99*double(y)/(height-1),1,1});data[y*width+x]=(DWORD(GetRValue(value))<<16)|(DWORD(GetGValue(value))<<8)|GetBValue(value);}return bitmap;
}
HRGN CreateNodeRegion(const std::vector<RECT>& nodes,int diameter){
    auto region=CreateRectRgn(0,0,0,0);if(!region)return nullptr;
    for(auto r:nodes){auto part=CreateRoundRectRgn(r.left,r.top,r.right,r.bottom,diameter,diameter);if(!part){DeleteObject(region);return nullptr;}int result=CombineRgn(region,region,part,RGN_OR);DeleteObject(part);if(result==ERROR){DeleteObject(region);return nullptr;}}
    return region;
}
IconArtwork::~IconArtwork(){if(bitmap)DeleteObject(bitmap);}
bool IconArtwork::Load(const std::wstring& executable,int index,int pixels){
    source.Reset();if(bitmap){DeleteObject(bitmap);bitmap=nullptr;}width=height=0;
    if(executable.empty())return false;
    if(!factory&&FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
    HICON icon=nullptr;
    if(SHDefExtractIconW(executable.c_str(),index,0,&icon,nullptr,static_cast<UINT>(pixels))!=S_OK||!icon)return false;
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
    COLORREF accent,HFONT font,HFONT bold,UINT dpi,UINT state,IconArtwork* artwork,COLORREF background){
    auto scale=[&](int value){return MulDiv(value,static_cast<int>(dpi),96);};
    auto fill=[&](RECT box,COLORREF color,int radius){auto brush=CreateSolidBrush(color);auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));if(radius)RoundRect(dc,box.left,box.top,box.right,box.bottom,scale(radius),scale(radius));else FillRect(dc,&box,brush);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);};
    fill(r,Background,0);fill(r,background,10);
    int saved=SaveDC(dc);
    if(saved){
        // Animation capture translates the child control's DC. A GDI path follows
        // that logical origin; a region passed directly to ExtSelectClipRgn does not.
        if(BeginPath(dc)){RoundRect(dc,r.left,r.top,r.right,r.bottom,scale(10),scale(10));if(EndPath(dc))SelectClipPath(dc,RGN_AND);else AbortPath(dc);}
    }
    if(artwork){int size=(std::max)(int(r.right-r.left),int(r.bottom-r.top)),left=r.left+(r.right-r.left-size)/2,top=r.top+(r.bottom-r.top-size)/2;artwork->Draw(dc,{left,top,left+size,top+size},168);}
    if(saved)RestoreDC(dc,saved);
    RECT stripe{r.left,r.top+scale(10),r.left+scale(3),r.bottom-scale(10)};fill(stripe,accent,2);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,Text);auto oldFont=SelectObject(dc,bold);
    RECT text{r.left+scale(12),r.top+scale(8),r.right-scale(8),r.top+scale(45)};
    auto shadow=text;OffsetRect(&shadow,scale(1),scale(1));SetTextColor(dc,RGB(0,0,0));DrawTextW(dc,name.c_str(),static_cast<int>(name.size()),&shadow,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);SetTextColor(dc,Text);DrawTextW(dc,name.c_str(),static_cast<int>(name.size()),&text,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);
    SelectObject(dc,font);SetTextColor(dc,accent);text={r.left+scale(12),r.bottom-scale(24),r.right-scale(8),r.bottom};
    shadow=text;OffsetRect(&shadow,scale(1),scale(1));SetTextColor(dc,RGB(0,0,0));DrawTextW(dc,detail.c_str(),static_cast<int>(detail.size()),&shadow,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);SetTextColor(dc,accent);DrawTextW(dc,detail.c_str(),static_cast<int>(detail.size()),&text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);SelectObject(dc,oldFont);
    if(state&(ODS_FOCUS|ODS_SELECTED)){RECT focus=r;InflateRect(&focus,-scale(4),-scale(4));DrawFocusRect(dc,&focus);}
}
}
