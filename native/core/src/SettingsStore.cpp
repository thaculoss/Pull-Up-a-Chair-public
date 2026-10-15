#include "SettingsStore.h"
#include <Windows.h>
#include <charconv>
#include <mutex>
#include <stdexcept>
namespace inn::settingsstore {
namespace {
std::mutex mutex;
constexpr DWORD maxBytes=1024*1024;
struct File {
    HANDLE handle=INVALID_HANDLE_VALUE;
    ~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    explicit operator bool() const {return handle!=INVALID_HANDLE_VALUE;}
};
std::string_view Trim(std::string_view s){const auto b=s.find_first_not_of(" \t\r");return b==s.npos?std::string_view{}:s.substr(b,s.find_last_not_of(" \t\r")-b+1);}
std::string Lower(std::string_view s){std::string r(s);for(auto& c:r)if(c>='A'&&c<='Z')c+=32;return r;}
bool Text(std::string_view s){return !s.starts_with("\xff\xfe")&&!s.starts_with("\xfe\xff")&&s.find('\0')==s.npos;}
bool ReadHandle(HANDLE h,std::string& out) {
    LARGE_INTEGER size{},zero{};
    if(!GetFileSizeEx(h,&size)||size.QuadPart<0||size.QuadPart>maxBytes||!SetFilePointerEx(h,zero,nullptr,FILE_BEGIN))return false;
    out.resize(static_cast<std::size_t>(size.QuadPart));DWORD read{};
    return ReadFile(h,out.data(),static_cast<DWORD>(out.size()),&read,nullptr)&&read==out.size()&&Text(out);
}
bool ReadFileText(const wchar_t* file,std::string& out) {
    File f{CreateFileW(file,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    return f&&ReadHandle(f.handle,out);
}
bool WriteHandle(HANDLE h,const std::string& s) {
    LARGE_INTEGER zero{};DWORD written{};
    return SetFilePointerEx(h,zero,nullptr,FILE_BEGIN)&&WriteFile(h,s.data(),static_cast<DWORD>(s.size()),&written,nullptr)&&
        written==s.size()&&SetEndOfFile(h)&&FlushFileBuffers(h);
}
// Parse without normalizing the original bytes, comments or line endings.
template<class F> void Lines(std::string_view s,F f) {
    std::string section;
    for(std::size_t p=0;p<s.size();) {
        auto end=s.find('\n',p);end=end==s.npos?s.size():end+1;
        auto raw=s.substr(p,end-p),line=raw;
        if(p==0&&line.starts_with("\xef\xbb\xbf"))line.remove_prefix(3);
        if(line.ends_with('\n'))line.remove_suffix(1);line=Trim(line);
        const bool header=line.starts_with('[')&&line.ends_with(']');
        if(header)section=Lower(Trim(line.substr(1,line.size()-2)));
        f(raw,line,section,header);p=end;
    }
}
}
std::string Get(std::string_view ini,std::string_view section,std::string_view key,std::string fallback) {
    const auto target=Lower(section),name=Lower(key);bool found=false;
    Lines(ini,[&](auto,auto line,const auto& current,bool header){
        if(found||header||current!=target||line.empty()||line.starts_with(';')||line.starts_with('#'))return;
        const auto eq=line.find('=');
        if(eq!=line.npos&&Lower(Trim(line.substr(0,eq)))==name){fallback=Trim(line.substr(eq+1));found=true;}
    });return fallback;
}
int Integer(std::string_view ini,std::string_view section,std::string_view key,int fallback) {
    auto text=Get(ini,section,key,"");int result{};
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()?result:fallback;
}
std::string Update(std::string_view ini,std::string_view section,const Entries& entries) {
    const auto target=Lower(section);std::string out;bool inside=false,written=false;
    auto emit=[&]{if(written)return;if(!out.empty()&&!out.ends_with('\n'))out+="\r\n";
        for(const auto& [key,value]:entries)out+=key+"="+value+"\r\n";written=true;};
    Lines(ini,[&](auto raw,auto line,const auto& current,bool header){
        if(header){if(inside)emit();inside=current==target;}
        if(inside&&!header){const auto eq=line.find('=');if(eq!=line.npos)
            for(const auto& [key,value]:entries)if(Lower(Trim(line.substr(0,eq)))==Lower(key))return;}
        out+=raw;
    });
    if(!written){if(!inside){if(!out.empty()&&!out.ends_with('\n'))out+="\r\n";out+="["+std::string(section)+"]\r\n";}emit();}
    return out;
}
bool Read(std::string& contents,const wchar_t* file) {
    std::lock_guard lock(mutex);return ReadFileText(file,contents);
}
bool Save(const std::function<std::string(std::string_view)>& update,const wchar_t* file) {
    std::lock_guard lock(mutex);
    try {
        File f{CreateFileW(file,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        std::string before;if(!f||!ReadHandle(f.handle,before))return false;
        const auto after=update(before);if(after.size()>maxBytes||!Text(after))return false;
        if(after==before)return true;
        std::string actual;
        if(WriteHandle(f.handle,after)&&ReadFileText(file,actual)&&actual==after)return true;
        // Best-effort rollback on I/O or readback failure. Never update live UI state.
        WriteHandle(f.handle,before);return false;
    }catch(...){return false;}
}
}
