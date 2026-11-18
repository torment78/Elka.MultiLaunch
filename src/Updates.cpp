#include "Updates.h"
#include "Core.h"
#include <windows.h>
#include <winhttp.h>
#include <map>
#include <vector>
#include <array>
#include <stdexcept>
#include <charconv>

namespace elka {
namespace {
struct Json { std::map<std::string,Json> object;std::vector<Json> array;std::string text;bool boolean=false; };
class Reader {
    const std::string& s;size_t p=0,nodes=0;
    void Space(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\t'||s[p]=='\r'))++p;}
    char Take(){if(p==s.size())throw std::runtime_error("Truncated JSON");return s[p++];}
    unsigned Hex(){unsigned n=0;for(int i=0;i<4;++i){char c=Take();n*=16;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else throw std::runtime_error("JSON escape");}return n;}
    std::string String(){if(Take()!='"')throw std::runtime_error("JSON string");std::string r;for(;;){char c=Take();if(c=='"')break;if(static_cast<unsigned char>(c)<32)throw std::runtime_error("JSON control");if(c!='\\'){r+=c;continue;}c=Take();switch(c){case '"':case '\\':case '/':r+=c;break;case 'b':r+='\b';break;case 'f':r+='\f';break;case 'n':r+='\n';break;case 'r':r+='\r';break;case 't':r+='\t';break;case 'u':{unsigned h=Hex();std::wstring w(1,static_cast<wchar_t>(h));if(h>=0xd800&&h<=0xdbff){if(Take()!='\\'||Take()!='u')throw std::runtime_error("JSON surrogate");auto low=Hex();if(low<0xdc00||low>0xdfff)throw std::runtime_error("JSON surrogate");w+=static_cast<wchar_t>(low);}r+=ToUtf8(w);break;}default:throw std::runtime_error("JSON escape");}}return r;}
    Json Value(int depth){Space();if(depth>48||++nodes>100000||p==s.size())throw std::runtime_error("JSON limits");Json j;char c=s[p];if(c=='"'){j.text=String();return j;}if(c=='{'||c=='['){++p;bool object=c=='{';Space();if(p<s.size()&&s[p]==(object?'}':']')){++p;return j;}for(;;){Space();std::string key;if(object){key=String();Space();if(Take()!=':')throw std::runtime_error("JSON colon");}auto child=Value(depth+1);if(object)j.object.emplace(std::move(key),std::move(child));else j.array.push_back(std::move(child));Space();char end=Take();if(end==(object?'}':']'))break;if(end!=',')throw std::runtime_error("JSON comma");}return j;}auto at=p;while(p<s.size()&&s[p]!=','&&s[p]!=']'&&s[p]!='}'&&s[p]!=' '&&s[p]!='\r'&&s[p]!='\n'&&s[p]!='\t')++p;auto token=s.substr(at,p-at);if(token=="true")j.boolean=true;else if(token!="false"&&token!="null"){if(token.empty()||token.find_first_not_of("0123456789eE+.-")!=token.npos)throw std::runtime_error("JSON value");}return j;}
public:explicit Reader(const std::string& text):s(text){}Json Parse(){if(s.size()>2*1024*1024)throw std::runtime_error("JSON size");auto j=Value(0);Space();if(p!=s.size())throw std::runtime_error("Trailing JSON");return j;}
};
bool Version(std::wstring value,std::array<unsigned,5>& parts){if(!value.empty()&&value[0]==L'v')value.erase(0,1);auto dash=value.find(L'-');auto core=value.substr(0,dash);parts={0,0,0,1,0};if(dash!=value.npos){if(value.substr(dash,5)!=L"-dev.")return false;parts[3]=0;auto dev=value.substr(dash+5);if(dev.empty()||dev.find_first_not_of(L"0123456789")!=dev.npos||dev.size()>8)return false;parts[4]=std::stoul(dev);}size_t p=0;for(size_t i=0;i<3;++i){auto next=core.find(L'.',p);if((i<2&&next==core.npos)||(i==2&&next!=core.npos))return false;auto n=core.substr(p,next-p);if(n.empty()||n.size()>8||n.find_first_not_of(L"0123456789")!=n.npos)return false;parts[i]=std::stoul(n);p=next+1;}return true;}
struct Internet {HINTERNET h=nullptr;~Internet(){if(h)WinHttpCloseHandle(h);}};
}
UpdateResult ParseReleases(const std::string& json,const std::wstring& current){UpdateResult result;try{auto releases=Reader(json).Parse();if(releases.array.empty()&&(json.find_first_not_of(" \r\n\t")==json.npos||json[json.find_first_not_of(" \r\n\t") ]!='['))throw std::runtime_error("Release list");std::array<unsigned,5> best{};if(!Version(current,best))throw std::runtime_error("Version");for(auto& r:releases.array){if(r.object["draft"].boolean)continue;auto tag=FromUtf8(r.object["tag_name"].text);std::array<unsigned,5> v{};if(!Version(tag,v)||v<=best)continue;auto url=FromUtf8(r.object["html_url"].text);const std::wstring prefix=L"https://github.com/torment78/Elka.MultiLaunch/releases/tag/";if(url!=prefix+tag)continue;best=v;result.available=true;result.version=tag;result.url=url;}}catch(...){result.error=L"GitHub returned an unreadable release list. Please try again later.";}return result;}
UpdateResult CheckUpdates(){
    UpdateResult result;Internet session{WinHttpOpen(L"ElkaMultiLaunch/0.2.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};if(!session.h){result.error=L"Could not start the GitHub update check.";return result;}
    WinHttpSetTimeouts(session.h,5000,5000,5000,8000);Internet connection{WinHttpConnect(session.h,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.h){result.error=L"Could not connect to GitHub.";return result;}
    Internet request{WinHttpOpenRequest(connection.h,L"GET",L"/repos/torment78/Elka.MultiLaunch/releases?per_page=30",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.h){result.error=L"Could not create the GitHub request.";return result;}
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));const wchar_t* headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(request.h,headers,static_cast<DWORD>(-1),nullptr,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr)){result.error=L"GitHub could not be reached. Check your connection and try again.";return result;}
    DWORD status=0,size=sizeof(status);WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);if(status!=200){result.error=status==403||status==429?L"GitHub's request limit was reached. Please try again later.":L"GitHub update check failed (HTTP "+std::to_wstring(status)+L").";return result;}
    std::string body;char buffer[16384];DWORD read=0;for(;;){if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&read)){result.error=L"The GitHub response was interrupted. Try again.";return result;}if(!read)break;body.append(buffer,read);if(body.size()>2*1024*1024){result.error=L"The GitHub response was too large.";return result;}}return ParseReleases(body,AppVersion);
}
}
