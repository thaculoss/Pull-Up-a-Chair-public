#include "Catalog.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <map>
namespace {
int checks{};
void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class F> void Rejected(F function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}Check(rejected,message);}
std::string Line(std::string plugin="Example.esp",std::string id="000001",std::string model="Clutter/Food/Cooked.nif") {
    return plugin+"\t"+id+"\t0\t1\t1\t-4\t-4\t0\t4\t4\t2\t4.5\t4.5\t6\t1\t3.2\tCooked food\t"+model;
}
}
int main(int argc,char** argv) {
    try {
        Check(argc==2,"Pass the project config directory");
        inn::Catalog catalog;catalog.Load(argv[1]);
        Check(catalog.baseFoodCount==87,"Base food indices changed");
        Check(catalog.foods.size()==182,"Audited pack count differs from 95");
        Check(catalog.foodPackErrors.empty(),"Shipped optional pack was rejected");
        std::map<std::string,int> counts;
        for(std::size_t i=0;i<catalog.foods.size();++i) {
            const auto& food=catalog.foods[i];
            if(i<catalog.baseFoodCount){Check(food.pack.empty()&&food.expectedModel.empty(),"Base profile acquired optional restriction");continue;}
            ++counts[food.pack];
            Check(!food.pack.empty()&&!food.expectedModel.empty(),"Optional food lacks model guard");
            Check(food.MatchesModel("Meshes/"+food.expectedModel),"Canonical path model mismatch");
            Check(!food.MatchesModel("unknown/Replacement.nif"),"Replacement model was silently accepted");
            Check(food.mask!=0,"Pack contains intentionally excluded food");
        }
        Check(counts==std::map<std::string,int>{{"cc-fishing.tsv",31},{"cooking-reseasoned.tsv",25},{"gourmet.tsv",39}},"Batch coverage changed");
        auto fields=inn::Catalog::Fields(Line());auto food=inn::Catalog::ParseFood(fields,true);
        Check(food.plugin=="example.esp","Plugin identity not case normalized");
        Check(food.MatchesModel("MESHES\\CLUTTER\\FOOD\\COOKED.NIF"),"Windows model path not normalized");
        Check(!food.MatchesModel(""),"Missing current model accepted");
        for(auto bad:{"../Food.nif","/Food.nif","C:/Food.nif","Clutter/Food.dds",""})
            Rejected([&]{inn::Catalog::ParseFood(inn::Catalog::Fields(Line("Example.esp","000001",bad)),true);},"Unsafe model path accepted");
        for(auto bad:{"0","1000000","12junk"})Rejected([&]{inn::Catalog::ParseFood(inn::Catalog::Fields(Line("Example.esp",bad)),true);},"Invalid local FormID accepted");
        for(auto bad:{"../Example.esp","C:Example.esp","Example.txt"})Rejected([&]{inn::Catalog::ParseFood(inn::Catalog::Fields(Line(bad)),true);},"Invalid plugin name accepted");
        fields[5]="nan";Rejected([&]{inn::Catalog::ParseFood(fields,true);},"Non-finite geometry accepted");
        fields=inn::Catalog::Fields(Line());fields[11]="0";Rejected([&]{inn::Catalog::ParseFood(fields,true);},"Zero footprint accepted");
        fields=inn::Catalog::Fields(Line());fields[4]="2";Rejected([&]{inn::Catalog::ParseFood(fields,true);},"Invalid plate flag accepted");
        fields=inn::Catalog::Fields(Line());fields[3]="4";Rejected([&]{inn::Catalog::ParseFood(fields,true);},"Default course outside mask accepted");
        const auto temp=std::filesystem::temp_directory_path()/("inn-food-pack-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(temp);
        std::ofstream(temp/"02-valid.tsv")<<Line("Second.esp")<<'\n';
        std::ofstream(temp/"01-valid.tsv")<<Line()<<'\n';
        std::ofstream(temp/"03-duplicate.tsv")<<Line("Unique.esp")<<'\n'<<Line("EXAMPLE.ESP")<<'\n';
        std::ofstream(temp/"04-corrupt.tsv")<<Line("Unused.esp")<<'\n'<<"broken\n";
        inn::Catalog isolated;isolated.LoadFoodPacks(temp);
        Check(isolated.foods.size()==2,"Rejected pack partially polluted catalog");
        Check(isolated.foods[0].plugin=="example.esp"&&isolated.foods[1].plugin=="second.esp","Pack loading is not deterministic");
        Check(isolated.foodPackErrors.size()==2,"Duplicate and corrupt pack not isolated");
        isolated.LoadFoodPacks(temp/"absent");Check(isolated.foods.size()==2,"Missing optional directory damaged catalog");
        for(auto name:{"01-valid.tsv","02-valid.tsv","03-duplicate.tsv","04-corrupt.tsv"})std::filesystem::remove(temp/name);
        std::filesystem::remove(temp);
        std::cout<<checks<<" food pack checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
