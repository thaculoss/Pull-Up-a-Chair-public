#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace inn::settingsstore {
inline constexpr auto path=L".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini";
using Entries=std::vector<std::pair<std::string,std::string>>;
std::string Get(std::string_view ini,std::string_view section,std::string_view key,std::string fallback);
int Integer(std::string_view ini,std::string_view section,std::string_view key,int fallback);
std::string Update(std::string_view ini,std::string_view section,const Entries& entries);
bool Read(std::string& contents,const wchar_t* file=path);
// All writers share one lock, use the existing virtual file handle (no rename),
// flush it and verify through a freshly opened handle before reporting success.
bool Save(const std::function<std::string(std::string_view)>& update,const wchar_t* file=path);
inline bool SaveSection(std::string_view section,const Entries& entries) {
    return Save([&](std::string_view before){return Update(before,section,entries);});
}
}
