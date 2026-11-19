#pragma once
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace ui {
struct Hsv { double hue=0,saturation=0,value=0; };
Hsv ColorToHsv(COLORREF color);
COLORREF HsvToColor(Hsv color);
HBITMAP CreateColorPlane(int width,int height,int shadeWidth,int hueStart,double hue);
// One source image and one DPI-sized bitmap per store; no recurring image work.
class IconArtwork {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    HBITMAP bitmap=nullptr;
    int width=0,height=0;
public:
    ~IconArtwork();
    IconArtwork()=default;
    IconArtwork(const IconArtwork&)=delete;
    IconArtwork& operator=(const IconArtwork&)=delete;
    bool Load(const std::wstring& executable,int index=0,int pixels=256);
    bool Draw(HDC dc,RECT rectangle,BYTE opacity=108);
    bool Available()const{return source!=nullptr;}
};
void DrawLauncherTile(HDC dc,RECT rect,const std::wstring& name,const std::wstring& detail,
    COLORREF accent,HFONT font,HFONT bold,UINT dpi,UINT state,IconArtwork* artwork=nullptr,COLORREF background=RGB(31,39,49));
// Caller owns the result until successfully passing it to SetWindowRgn.
HRGN CreateNodeRegion(const std::vector<RECT>& nodes,int cornerDiameter);
}
