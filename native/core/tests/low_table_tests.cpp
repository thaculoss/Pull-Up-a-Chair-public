#include "Catalog.h"
#include "SurfacePolicy.h"
#include "CallFocusPolicy.h"
#include <iostream>

namespace {
int checks{};
void Check(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    try {
        Check(argc==3,"Need new and baseline catalog paths");
        inn::Catalog current,previous;current.Load(argv[1]);previous.Load(argv[2]);
        Check(current.tableAliases.size()==previous.tableAliases.size()+2,"Only two low-table aliases should be added");
        Check(current.tables.size()==previous.tables.size()+2,"Only two low-table profiles should be added");
        const auto count=previous.tables.size()-previous.embeddedAliases.size();
        Check(count==144,"Unexpected baseline standalone profile count");
        for(std::size_t i=0;i<count;++i) {
            Check(current.tables[i]==previous.tables[i],"Existing bounds changed");
            Check(current.tableShapes[i]==previous.tableShapes[i]&&current.tableCounters[i]==previous.tableCounters[i],"Existing surface type changed");
            Check(current.MinimumSeatHeight(static_cast<int>(i))==35.f,"Ordinary table height was widened");
        }
        for(std::size_t i=0;i<previous.tableAliases.size();++i) {
            const auto& a=current.tableAliases[i];const auto& b=previous.tableAliases[i];
            Check(a.index==b.index&&a.plugin==b.plugin&&a.local==b.local&&a.model==b.model&&a.shape==b.shape&&a.counter==b.counter,"Existing registration changed");
        }
        Check(current.embeddedAliases.size()==previous.embeddedAliases.size(),"Embedded surface count changed");
        for(std::size_t i=0;i<previous.embeddedAliases.size();++i) {
            const auto& a=current.embeddedAliases[i];const auto& b=previous.embeddedAliases[i];
            Check(a.index==b.index+2&&a.plugin==b.plugin&&a.local==b.local&&a.model==b.model&&a.segment==b.segment&&a.placement==b.placement,"Embedded identity or placement changed");
            Check(current.tables[a.index]==previous.tables[b.index]&&current.MinimumSeatHeight(a.index)==35.f,"Embedded geometry or minimum changed");
        }
        const auto& low=current.tableAliases[current.tableAliases.size()-2];
        Check(low.index==144&&low.plugin=="skyrim.esm"&&low.local==0xC29E0,"Wrong vanilla table base record");
        Check(low.model==inn::TableModelKey("Meshes\\Furniture\\Noble\\NobleTableLow01.nif")&&!low.counter,"Wrong model recognition");
        Check(current.MinimumSeatHeight(low.index)==20.f,"Missing low-table height exception");
        Check(current.MinimumSeatHeight(-1)==35.f&&current.MinimumSeatHeight(100000)==35.f,"Invalid profile fallback changed");
        const auto& bounds=current.tables[low.index];
        Check(bounds[0]>=-55.495140f&&bounds[1]>=-36.017853f&&bounds[2]<=55.804878f&&bounds[3]<=34.537464f,"Placement leaves measured vanilla tabletop");
        Check(std::abs(bounds[4]-29.445659637451172f)<0.001f,"Wrong measured tabletop height");
        // Skyrim.esm: WhiterunBanneredMare, table C8FC8, Uthgerd package
        // 091917 -> chair C8FCA. C8FCE is the chair across this same table.
        const inn::Surface table{0xC8FC8,low.index,{100.998672f,235.635590f,68.f},4.6831913f,1.f,bounds};
        for(const inn::Point chair: {inn::Point{191.678192f,239.724579f,70.627899f},inn::Point{22.711769f,228.012421f,67.999985f}}) {
            const auto delta=table.Top()-chair.z;
            Check(delta<35.f,"Fixture no longer reproduces old height rejection");
            Check(delta>current.MinimumSeatHeight(low.index)&&delta<105.f,"Serving still rejects vanilla low table");
            Check(table.EdgeDistance(chair)<140.f,"Vanilla chair outside cleanup range");
        }
        // Cleanup must still distinguish the tabletop from objects on the floor.
        const inn::Point onTable{table.origin.x,table.origin.y,table.Top()};
        Check(inn::OnSurface(table,onTable,0.f),"Top-surface item rejected");
        Check(!inn::OnSurface(table,table.origin,0.f),"Floor item admitted by low-table support");
        Check(current.MinimumSeatHeight(low.index)>0.f,"Floor-level surfaces admitted");
        const auto& low02=current.tableAliases.back();
        Check(low02.index==145&&low02.plugin=="skyrim.esm"&&low02.local==0xC57AA,"Wrong screenshot table base record");
        Check(low02.model==inn::TableModelKey("Furniture\\Noble\\NobleTableLow02.nif")&&!low02.counter,"Wrong Low02 model recognition");
        Check(current.MinimumSeatHeight(low02.index)==20.f,"Low02 lacks height exception");
        const auto& bounds02=current.tables[low02.index];
        Check(bounds02[0]>=-39.999900f&&bounds02[1]>=-32.f&&bounds02[2]<=40.000068f&&bounds02[3]<=31.999988f,"Low02 placement leaves measured tabletop");
        Check(bounds02[2]-bounds02[0]<bounds[2]-bounds[0],"Low02 incorrectly reuses the larger Low01 footprint");
        Check(std::abs(bounds02[4]-29.445659637451172f)<0.001f,"Wrong Low02 tabletop height");
        // Skyrim.esm and the reported log: Candlehearth table CBA60, seat CB93E.
        const inn::Surface table02{0xCBA60,low02.index,{-570.796570f,-3604.751709f,324.137665f},3.600000381f,1.f,bounds02};
        const inn::Point chair02{-516.787415f,-3546.148926f,320.f};
        const auto delta02=table02.Top()-chair02.z;
        Check(delta02<35.f&&delta02>current.MinimumSeatHeight(low02.index)&&delta02<105.f,"Candlehearth chair still fails height selection");
        Check(table02.EdgeDistance(chair02)<140.f,"Candlehearth chair outside cleanup range");
        const inn::Point top02{table02.origin.x,table02.origin.y,table02.Top()};
        Check(inn::OnSurface(table02,top02,0.f),"Low02 top item rejected by cleanup");
        Check(!inn::OnSurface(table02,table02.origin,0.f),"Low02 floor item admitted by cleanup");
        const inn::Point eye02{chair02.x,chair02.y,chair02.z+100.f};
        const inn::Point direction{top02.x-eye02.x,top02.y-eye02.y,top02.z-eye02.z};
        const std::array candidates{table02};
        const auto hit=inn::call::focus::PickTable(candidates,chair02,eye02,direction);
        Check(hit&&hit->reference==0xCBA60,"Low02 clear tabletop focus rejected");
        std::cout<<checks<<" low-table and unchanged-catalog regression checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
