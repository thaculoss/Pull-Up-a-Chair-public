#pragma once
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
namespace inn {
class Localization {
    std::map<std::string, std::string, std::less<>> english, active;
    std::atomic_bool chinese_{};
    static std::string Decode(std::string_view value) {
        std::string out;out.reserve(value.size());
        for(std::size_t i=0;i<value.size();++i) {
            if(value[i]=='\\'&&i+1<value.size()) {
                const auto next=value[i+1];
                if(next=='n'||next=='t'||next=='\\'){out+=next=='n'?'\n':next=='t'?'\t':'\\';++i;continue;}
            }
            out+=value[i];
        }
        return out;
    }
    static bool Read(const std::filesystem::path& path, std::map<std::string,std::string,std::less<>>& target) {
        std::ifstream stream(path);
        if (!stream) return false;
        std::string line;
        while (std::getline(stream,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.empty() || line.front()=='#') continue;
            auto separator=line.find('\t');
            if (separator!=std::string::npos) target[line.substr(0,separator)]=Decode(line.substr(separator+1));
        }
        return true;
    }
public:
    bool Load(const std::filesystem::path& folder, std::string_view language) {
        english.clear(); active.clear();
        const bool valid=Read(folder / "en.tsv",english);
        Read(folder / "zh.tsv",active);
        UseChinese(language=="zh");
        return valid;
    }
    // Both dictionaries remain immutable after startup; previously returned text
    // pointers stay valid while render, Papyrus and service threads change language.
    void UseChinese(bool value) { chinese_.store(value,std::memory_order_relaxed); }
    bool IsChinese() const { return chinese_.load(std::memory_order_relaxed); }
    // A blank per-inn value inherits the active language's shared wording.
    // This is plain promotional text, so braces are never treated as fmt fields.
    const char* InnText(std::string_view key,std::string_view innKey) const {
        if(!innKey.empty()) {
            const auto qualified=std::string(key)+"."+std::string(innKey);
            const auto& localized=IsChinese()?active:english;
            if(auto i=localized.find(qualified);i!=localized.end()&&!i->second.empty())return i->second.c_str();
        }
        return Text(key);
    }
    const char* Text(std::string_view key) const {
        if (IsChinese())if (auto i=active.find(key); i!=active.end()) return i->second.c_str();
        if (auto i=english.find(key); i!=english.end()) return i->second.c_str();
        return "[missing translation]";
    }
    std::string Retranslate(std::string_view value) const {
        if(value.empty())return {};
        for(const auto* dictionary:{&english,&active})
            for(const auto& [key,text]:*dictionary)if(text==value)return Text(key);
        return std::string(value);
    }
};
inline Localization strings;
inline const char* T(std::string_view key) { return strings.Text(key); }
}
