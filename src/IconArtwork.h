#pragma once
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>

namespace ui {
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
    bool Load(const std::wstring& executable);
    bool Draw(HDC dc,RECT rectangle,BYTE opacity=108);
    bool Available()const{return source!=nullptr;}
};
void DrawLauncherTile(HDC dc,RECT rect,const std::wstring& name,const std::wstring& detail,
    COLORREF accent,HFONT font,HFONT bold,UINT dpi,UINT state,IconArtwork* artwork=nullptr);
}
