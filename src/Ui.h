#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace ui {
constexpr COLORREF Background=RGB(17,21,27), Panel=RGB(24,30,38), Field=RGB(31,39,49), Edge=RGB(48,61,75), Text=RGB(230,237,243), Muted=RGB(142,158,177), Accent=RGB(77,218,191), Selected=RGB(32,68,68), Danger=RGB(255,161,142);
class Window {
public:
    HWND hwnd=nullptr;UINT dpi=96;HFONT font=nullptr,bold=nullptr,title=nullptr;bool done=false;int result=0;
    virtual ~Window();
    int S(int value)const{return MulDiv(value,static_cast<int>(dpi),96);}
    HWND Create(const std::wstring& caption,int width,int height,HWND owner=nullptr,bool resize=false);
    int Modal(HWND owner);
    void End(int code);
    HWND Control(const wchar_t* type,const std::wstring& caption,int id,DWORD style=0);
    HWND Label(const std::wstring& caption,int id);
    HWND Button(const std::wstring& caption,int id,bool accent=false);
    HWND Check(const std::wstring& caption,int id,bool value);
    HWND Edit(const std::wstring& text,int id,bool readOnly=false);
    HWND List(int id,int rowHeight=48);
    HWND Get(int id)const{return GetDlgItem(hwnd,id);}
    std::wstring Value(int id)const;
    void Value(int id,const std::wstring& value);
    bool Checked(int id)const;
    void Checked(int id,bool value);
    void Place(int id,int x,int y,int width,int height);
    void Show(int id,bool show);
    virtual void Created(){}
    virtual void Layout(int,int){}
    virtual void Command(int,int){}
    virtual void Paint(HDC,RECT){}
    virtual void Item(DRAWITEMSTRUCT*){}
    virtual LRESULT Message(UINT,WPARAM,LPARAM,bool& handled){handled=false;return 0;}
    virtual void Close(){End(0);}
    void TextAt(HDC dc,const std::wstring& text,RECT rect,COLORREF color=Text,HFONT use=nullptr,UINT format=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS)const;
    void Box(HDC dc,RECT rect,COLORREF color,int radius=0)const;
private:
    void Fonts();
    static LRESULT CALLBACK Proc(HWND,UINT,WPARAM,LPARAM);
};
bool Notice(HWND owner,const std::wstring& title,const std::wstring& text,bool confirm=false);
std::wstring PickProcess(HWND owner);
std::wstring PickExe(HWND owner,const std::wstring& previous=L"");
}
