#pragma once
#include "Catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
namespace inn {
struct Artwork {
    std::string path;
    std::array<float,4> uv{0,0,1,1};
    float aspect=1;
    int quarterTurns{}; // Clockwise display rotation. Original PNG remains unchanged.
    float displayScale=1.f; // Optional per-title size, independent of full-stroke UV bounds.
    float bodyCenterY=.5f; // Optical text-body center within the complete UV region.
};
struct InnArtwork {
    std::string plugin,key,nameEnglish,nameChinese;
    std::uint32_t local{};
    Artwork image;
};
struct ArtworkCatalog {
    std::vector<InnArtwork> entries;
    void Load(const std::filesystem::path& file) {
        entries.clear();
        Catalog::Read(file,[&](const auto& fields){
            if(fields.size()<11||fields.size()>14)throw std::runtime_error("Artwork catalog needs 11 through 14 fields");
            InnArtwork row;row.plugin=fields[0];row.local=std::stoul(fields[1],nullptr,16);
            row.key=fields[2];row.nameEnglish=fields[3];row.nameChinese=fields[4];
            // A dash keeps venue text and title lookup without an illustration.
            if(fields[5]!="-") {
                if(std::filesystem::path(fields[5]).filename().string()!=fields[5]||!fields[5].ends_with(".png"))
                    throw std::runtime_error("Artwork filename must be a local PNG or -");
                row.image.path="Data/Interface/PullUpAChairOrdering/InnSigns/"+fields[5];
            }
            for(int i=0;i<4;++i){row.image.uv[i]=std::stof(fields[6+i]);if(!std::isfinite(row.image.uv[i])||row.image.uv[i]<0||row.image.uv[i]>1)throw std::runtime_error("Invalid artwork UV");}
            row.image.aspect=std::stof(fields[10]);
            if(fields.size()>=12)row.image.quarterTurns=std::stoi(fields[11]);
            if(fields.size()>=13) {
                std::size_t consumed{};
                row.image.displayScale=std::stof(fields[12],&consumed);
                if(consumed!=fields[12].size()||!std::isfinite(row.image.displayScale)||
                   row.image.displayScale<.25f||row.image.displayScale>2.f)
                    throw std::runtime_error("Invalid artwork display scale");
            }
            if(fields.size()==14) {
                std::size_t consumed{};
                row.image.bodyCenterY=std::stof(fields[13],&consumed);
                if(consumed!=fields[13].size()||!std::isfinite(row.image.bodyCenterY)||
                   row.image.bodyCenterY<0.f||row.image.bodyCenterY>1.f)
                    throw std::runtime_error("Invalid artwork body center");
            }
            if(row.image.quarterTurns<0||row.image.quarterTurns>3)throw std::runtime_error("Invalid artwork rotation");
            if(row.image.uv[0]>=row.image.uv[2]||row.image.uv[1]>=row.image.uv[3]||!std::isfinite(row.image.aspect)||row.image.aspect<=0)
                throw std::runtime_error("Empty artwork region");
            if(Find(row.key))throw std::runtime_error("Duplicate artwork key");
            entries.push_back(std::move(row));
        });
    }
    const InnArtwork* Find(const std::string& key) const {
        auto row=std::find_if(entries.begin(),entries.end(),[&](const auto& entry){return entry.key==key;});
        return row==entries.end()?nullptr:&*row;
    }
};
struct ArtworkFrame {float x{},y{},width{},height{};};
inline float InnTitleDisplayScale(const Artwork& art,bool chinese) {
    return chinese?1.f:art.displayScale;
}
inline float InnTitleBodyCenter(const Artwork& art,bool chinese) {
    return chinese?.5f:art.bodyCenterY;
}
inline float InnTitleRowHeight(float height,const Artwork& art,bool chinese) {
    const auto center=InnTitleBodyCenter(art,chinese);
    // Leave room for fractional-pixel rounding at smaller menu scales.
    return height*2.f*std::max(center,1.f-center)+(chinese?0.f:2.f);
}
// Keep the existing catalog for English and older installations. Chinese may
// supply a separate set of title PNGs without changing shared inn/sign assets.
inline std::filesystem::path InnTitleCatalogPath(const std::filesystem::path& folder,bool chinese) {
    const auto localized=folder/"inn_titles_zh.tsv";
    return chinese&&std::filesystem::exists(localized)?localized:folder/"inn_titles.tsv";
}
inline ArtworkFrame FitArtwork(float side,float aspect) {
    if(!std::isfinite(aspect)||aspect<=0)aspect=1;
    const float width=std::min(side,side*aspect),height=std::min(side,side/aspect);
    return {(side-width)/2,(side-height)/2,width,height};
}
inline std::array<std::array<float,2>,4> ArtworkUVs(const Artwork& art) {
    const std::array<std::array<float,2>,4> original{{{art.uv[0],art.uv[1]},{art.uv[2],art.uv[1]},
                                                  {art.uv[2],art.uv[3]},{art.uv[0],art.uv[3]}}};
    std::array<std::array<float,2>,4> result{};
    for(int i=0;i<4;++i)result[i]=original[(i-art.quarterTurns+4)%4];
    return result;
}
inline ArtworkCatalog innArtwork;
}
