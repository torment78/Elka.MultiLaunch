#pragma once
#include <string>
namespace elka {
inline constexpr wchar_t AppVersion[]=L"0.2.0-dev.1";
struct UpdateResult { bool available=false; std::wstring version,url,error; };
UpdateResult ParseReleases(const std::string& json,const std::wstring& current);
UpdateResult CheckUpdates();
}
