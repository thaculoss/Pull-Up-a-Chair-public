#pragma once
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cmath>
#include <set>
#include <algorithm>
#include <string_view>
#include <cctype>
#include "TableGeometry.h"
namespace inn {
inline std::string FoodModelKey(std::string_view model) {
    std::string key(model);
    for(auto& ch:key)ch=ch=='\\'?'/':static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if(key.starts_with("meshes/"))key.erase(0,7);
    return key;
}
struct FoodProfile {
    std::string plugin;
    std::uint32_t local{};
    int course{}, mask{};
    bool plate{};
    std::array<float,6> bounds{};
    float width{}, depth{}, radius{}, scale{}, support{};
    // Optional packs are resolved only against an installed record with this model.
    // Base profiles keep their stable indices and the existing override behavior.
    std::string expectedModel,pack;
    bool MatchesModel(std::string_view model) const {return expectedModel.empty()||expectedModel==FoodModelKey(model);}
};
struct Catalog {
    struct TableAlias {int index;std::string plugin;std::uint32_t local;std::string model;SurfaceShape shape;bool counter;};
    struct EmbeddedAlias {int index;std::string plugin;std::uint32_t local;std::string model;std::uint32_t segment;std::array<float,5> placement;};
    std::vector<FoodProfile> foods;
    std::size_t baseFoodCount{};
    std::vector<std::string> foodPackErrors;
    std::vector<std::array<float,5>> tables;
    std::vector<TableAlias> tableAliases;
    std::vector<EmbeddedAlias> embeddedAliases;
    std::vector<SurfaceShape> tableShapes;
    std::vector<bool> tableCounters;
    // Minimum tabletop height above the seat reference. Low-table exceptions
    // are explicitly registered; ordinary tables and counters retain 35.
    std::vector<float> tableSeatMinima;
    float MinimumSeatHeight(int index) const {
        return index>=0&&index<static_cast<int>(tableSeatMinima.size())?tableSeatMinima[index]:35.f;
    }
    std::vector<std::array<float,6>> props;
    static std::vector<std::string> Fields(std::string line) {
        std::vector<std::string> fields;
        std::istringstream input(line);
        while(std::getline(input,line,'\t')) fields.push_back(line);
        return fields;
    }
    template<class F> static void Read(const std::filesystem::path& file,F parse) {
        std::ifstream input(file);
        if(!input) throw std::runtime_error("Missing catalog: "+file.string());
        std::string line;
        while(std::getline(input,line)) {
            if(line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            parse(Fields(line));
        }
    }
    static FoodProfile ParseFood(const std::vector<std::string>& f,bool optional) {
        if(f.size()!=(optional?18:17))throw std::runtime_error(optional?"Food pack needs 18 fields":"foods.tsv needs 17 fields");
        FoodProfile p;p.plugin=f[0];
        std::transform(p.plugin.begin(),p.plugin.end(),p.plugin.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(p.plugin.empty()||p.plugin.find_first_of("/\\:")!=std::string::npos||
           !(p.plugin.ends_with(".esm")||p.plugin.ends_with(".esp")||p.plugin.ends_with(".esl")))throw std::runtime_error("Invalid food plugin");
        std::size_t parsed{};auto local=std::stoul(f[1],&parsed,16);
        if(parsed!=f[1].size()||!local||local>0xFFFFFF)throw std::runtime_error("Invalid food local FormID");
        p.local=static_cast<std::uint32_t>(local);
        auto integer=[](const std::string& value){std::size_t consumed{};auto result=std::stoi(value,&consumed);if(consumed!=value.size())throw std::runtime_error("Invalid integer field");return result;};
        auto number=[](const std::string& value){std::size_t consumed{};auto result=std::stof(value,&consumed);if(consumed!=value.size())throw std::runtime_error("Invalid numeric field");return result;};
        p.course=integer(f[2]);p.mask=integer(f[3]);auto plate=integer(f[4]);
        if(plate!=0&&plate!=1)throw std::runtime_error("Invalid extra plate flag");p.plate=plate!=0;
        for(int i=0;i<6;++i)p.bounds[i]=number(f[5+i]);
        p.width=number(f[11]);p.depth=number(f[12]);p.radius=number(f[13]);
        p.scale=number(f[14]);p.support=number(f[15]);
        if(p.course<0||p.course>2||p.mask<0||p.mask>7||(p.mask&&!(p.mask&(1<<p.course))))throw std::runtime_error("Invalid food category");
        for(auto value:p.bounds)if(!std::isfinite(value))throw std::runtime_error("Non-finite food bounds");
        for(auto value:{p.width,p.depth,p.radius,p.scale})if(!std::isfinite(value)||value<=0)throw std::runtime_error("Invalid food size");
        if(!std::isfinite(p.support))throw std::runtime_error("Invalid plate support");
        for(int i=0;i<3;++i)if(p.bounds[i]>p.bounds[i+3])throw std::runtime_error("Inverted food bounds");
        if(optional) {
            p.expectedModel=FoodModelKey(f[17]);
            if(p.expectedModel.empty()||p.expectedModel.starts_with('/')||p.expectedModel.find(':')!=std::string::npos||
               p.expectedModel.find("..")!=std::string::npos||!p.expectedModel.ends_with(".nif"))throw std::runtime_error("Invalid expected food model");
        }
        return p;
    }
    void LoadFoodPacks(const std::filesystem::path& folder) {
        foodPackErrors.clear();
        std::error_code error;
        if(!std::filesystem::exists(folder,error)&&!error)return;
        if(error||!std::filesystem::is_directory(folder,error)){foodPackErrors.push_back("Optional food pack directory unavailable");return;}
        std::vector<std::filesystem::path> files;
        std::filesystem::directory_iterator iterator(folder,error),end;
        for(;!error&&iterator!=end;iterator.increment(error))if(iterator->is_regular_file(error)&&FoodModelKey(iterator->path().extension().string())==".tsv")files.push_back(iterator->path());
        if(error){foodPackErrors.push_back("Cannot enumerate optional food packs: "+error.message());return;}
        std::sort(files.begin(),files.end());
        std::set<std::pair<std::string,std::uint32_t>> ids;
        for(const auto& food:foods)ids.emplace(food.plugin,food.local);
        for(const auto& file:files) {
            try {
                // One invalid pack cannot partially alter or disable the base menu.
                auto stagedIds=ids;std::vector<FoodProfile> staged;
                Read(file,[&](const auto& fields){
                    auto food=ParseFood(fields,true);food.pack=file.filename().string();
                    if(!stagedIds.emplace(food.plugin,food.local).second)throw std::runtime_error("Duplicate food record");
                    staged.push_back(std::move(food));
                });
                if(staged.empty())throw std::runtime_error("Empty food pack");
                foods.insert(foods.end(),staged.begin(),staged.end());ids=std::move(stagedIds);
            } catch(const std::exception& e) {foodPackErrors.push_back(file.filename().string()+": "+e.what());}
        }
    }
    void Load(const std::filesystem::path& folder) {
        foods.clear(); tables.clear(); props.clear();tableAliases.clear();embeddedAliases.clear();tableShapes.clear();tableCounters.clear();tableSeatMinima.clear();foodPackErrors.clear();baseFoodCount=0;
        Read(folder/"foods.tsv",[&](const auto& f){
            foods.push_back(ParseFood(f,false));
        });
        Read(folder/"tables.tsv",[&](const auto& f){
            if(f.size()!=6)throw std::runtime_error("tables.tsv needs 6 fields");
            if(std::stoul(f[0])!=tables.size())throw std::runtime_error("Table indices must match the base FormList");
            std::array<float,5> row{}; for(int i=0;i<5;++i){row[i]=std::stof(f[i+1]);if(!std::isfinite(row[i]))throw std::runtime_error("Invalid table bounds");} tables.push_back(row);
        });
        Read(folder/"props.tsv",[&](const auto& f){
            if(f.size()!=7)throw std::runtime_error("props.tsv needs 7 fields");
            if(std::stoul(f[0])!=props.size())throw std::runtime_error("Prop indices must match the base FormList");
            std::array<float,6> row{}; for(int i=0;i<6;++i){row[i]=std::stof(f[i+1]);if(!std::isfinite(row[i]))throw std::runtime_error("Invalid prop bounds");}props.push_back(row);
        });
        if(foods.size()<54||tables.size()<35||props.size()!=41)throw std::runtime_error("Base catalog is incomplete");
        tableShapes.assign(tables.size(),SurfaceShape::rectangle);tableCounters.assign(tables.size(),false);
        tableSeatMinima.assign(tables.size(),35.f);
        std::set<int> seatHeightOverrides;
        for(std::size_t i=25;i<35;++i)tableCounters[i]=true;
        Read(folder/"table_registry.tsv",[&](const auto& f){
            if(f.size()!=7&&f.size()!=8)throw std::runtime_error("table_registry.tsv needs 7 or 8 fields");
            const auto index=std::stoi(f[0]);
            if(index<0||index>=static_cast<int>(tables.size())||(f[4]!="rect"&&f[4]!="ellipse")||(f[5]!="table"&&f[5]!="counter"))throw std::runtime_error("Invalid table registration");
            const auto shape=f[4]=="ellipse"?SurfaceShape::ellipse:SurfaceShape::rectangle;
            if(f.size()==8) {
                std::size_t consumed{};const float minimum=std::stof(f[7],&consumed);
                if(consumed!=f[7].size()||!std::isfinite(minimum)||minimum<10.f||minimum>35.f||f[5]!="table")throw std::runtime_error("Invalid table minimum seat height");
                if(!seatHeightOverrides.insert(index).second&&tableSeatMinima[index]!=minimum)throw std::runtime_error("Conflicting table minimum seat height");
                tableSeatMinima[index]=minimum;
            }
            tableShapes[index]=shape;tableCounters[index]=f[5]=="counter";
            tableAliases.push_back({index,f[1],static_cast<std::uint32_t>(std::stoul(f[2],nullptr,16)),TableModelKey(f[3]),shape,f[5]=="counter"});
        });
        if(tableAliases.size()<35)throw std::runtime_error("Table registry is incomplete");
        if(std::filesystem::exists(folder/"embedded_surfaces.tsv"))Read(folder/"embedded_surfaces.tsv",[&](const auto& f){
            if(f.size()!=12&&f.size()!=16)throw std::runtime_error("embedded_surfaces.tsv needs 12 or 16 fields");
            const auto segment=static_cast<std::uint32_t>(std::stoul(f[3]));
            if(!segment||(f[4]!="rect"&&f[4]!="ellipse")||(f[5]!="table"&&f[5]!="counter"))throw std::runtime_error("Invalid embedded surface");
            std::array<float,5> bounds{};
            for(int i=0;i<5;++i){bounds[i]=std::stof(f[6+i]);if(!std::isfinite(bounds[i]))throw std::runtime_error("Invalid embedded bounds");}
            if(bounds[0]>=bounds[2]||bounds[1]>=bounds[3])throw std::runtime_error("Inverted embedded bounds");
            auto placement=bounds;
            if(f.size()==16)for(int i=0;i<4;++i){placement[i]=std::stof(f[11+i]);if(!std::isfinite(placement[i]))throw std::runtime_error("Invalid embedded placement");}
            if(placement[0]<bounds[0]||placement[1]<bounds[1]||placement[2]>bounds[2]||placement[3]>bounds[3]||placement[0]>=placement[2]||placement[1]>=placement[3])throw std::runtime_error("Embedded placement outside surface");
            const auto index=static_cast<int>(tables.size());tables.push_back(bounds);
            tableShapes.push_back(f[4]=="ellipse"?SurfaceShape::ellipse:SurfaceShape::rectangle);tableCounters.push_back(f[5]=="counter");
            tableSeatMinima.push_back(35.f);
            embeddedAliases.push_back({index,f[0],static_cast<std::uint32_t>(std::stoul(f[1],nullptr,16)),TableModelKey(f[2]),segment,placement});
        });
        std::set<std::pair<std::string,std::uint32_t>> ids;
        for(const auto& food:foods)if(!ids.emplace(food.plugin,food.local).second)throw std::runtime_error("Duplicate food record");
        baseFoodCount=foods.size();LoadFoodPacks(folder/"food_packs");
    }
};
inline Catalog catalog;
}
