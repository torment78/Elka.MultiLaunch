#pragma once
#include "Core.h"
#include <map>
#include <vector>
#include <stdexcept>

namespace elka::detail {
struct Json { enum class Type { Null,Object,Array,String,Number,Boolean };Type type=Type::Null;std::map<std::string,Json> object;std::vector<Json> array;std::string text;bool boolean=false; };
class Reader {
    const std::string& s;size_t p=0,nodes=0;
    void Space(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\t'||s[p]=='\r'))++p;}
    char Take(){if(p==s.size())throw std::runtime_error("Truncated JSON");return s[p++];}
    unsigned Hex(){unsigned n=0;for(int i=0;i<4;++i){char c=Take();n*=16;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else throw std::runtime_error("JSON escape");}return n;}
    std::string String(){if(Take()!='"')throw std::runtime_error("JSON string");std::string r;for(;;){char c=Take();if(c=='"')break;if(static_cast<unsigned char>(c)<32)throw std::runtime_error("JSON control");if(c!='\\'){r+=c;continue;}c=Take();switch(c){case '"':case '\\':case '/':r+=c;break;case 'b':r+='\b';break;case 'f':r+='\f';break;case 'n':r+='\n';break;case 'r':r+='\r';break;case 't':r+='\t';break;case 'u':{unsigned h=Hex();std::wstring w(1,static_cast<wchar_t>(h));if(h>=0xd800&&h<=0xdbff){if(Take()!='\\'||Take()!='u')throw std::runtime_error("JSON surrogate");auto low=Hex();if(low<0xdc00||low>0xdfff)throw std::runtime_error("JSON surrogate");w+=static_cast<wchar_t>(low);}r+=ToUtf8(w);break;}default:throw std::runtime_error("JSON escape");}}return r;}
    bool Number(const std::string& token){size_t i=0;if(i<token.size()&&token[i]=='-')++i;if(i==token.size())return false;if(token[i]=='0')++i;else{if(token[i]<'1'||token[i]>'9')return false;while(i<token.size()&&token[i]>='0'&&token[i]<='9')++i;}if(i<token.size()&&token[i]=='.'){size_t start=++i;while(i<token.size()&&token[i]>='0'&&token[i]<='9')++i;if(i==start)return false;}if(i<token.size()&&(token[i]=='e'||token[i]=='E')){++i;if(i<token.size()&&(token[i]=='+'||token[i]=='-'))++i;size_t start=i;while(i<token.size()&&token[i]>='0'&&token[i]<='9')++i;if(i==start)return false;}return i==token.size();}
    Json Value(int depth){
        Space();if(depth>48||++nodes>100000||p==s.size())throw std::runtime_error("JSON limits");Json j;char c=s[p];
        if(c=='"'){j.type=Json::Type::String;j.text=String();FromUtf8(j.text);return j;}
        if(c=='{'||c=='['){++p;bool object=c=='{';j.type=object?Json::Type::Object:Json::Type::Array;Space();if(p<s.size()&&s[p]==(object?'}':']')){++p;return j;}
            for(;;){Space();std::string key;if(object){key=String();Space();if(Take()!=':')throw std::runtime_error("JSON colon");}auto child=Value(depth+1);if(object){if(!j.object.emplace(std::move(key),std::move(child)).second)throw std::runtime_error("Duplicate JSON key");}else j.array.push_back(std::move(child));Space();char end=Take();if(end==(object?'}':']'))break;if(end!=',')throw std::runtime_error("JSON comma");}return j;}
        auto at=p;while(p<s.size()&&s[p]!=','&&s[p]!=']'&&s[p]!='}'&&s[p]!=' '&&s[p]!='\r'&&s[p]!='\n'&&s[p]!='\t')++p;auto token=s.substr(at,p-at);
        if(token=="true"||token=="false"){j.type=Json::Type::Boolean;j.boolean=token=="true";}else if(token!="null"){if(!Number(token))throw std::runtime_error("JSON value");j.type=Json::Type::Number;}return j;
    }
public:explicit Reader(const std::string& text):s(text){if(s.compare(0,3,"\xef\xbb\xbf")==0)p=3;}Json Parse(){if(s.size()>2*1024*1024)throw std::runtime_error("JSON size");auto j=Value(0);Space();if(p!=s.size())throw std::runtime_error("Trailing JSON");return j;}
};
}
