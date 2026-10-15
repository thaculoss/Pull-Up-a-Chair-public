#include "OrderPolicy.h"
#include "RuntimePolicy.h"
#include "TablePolicy.h"
#include "Catalog.h"
#include "Localization.h"
#include "SettingsUiLayout.h"
#include "Navigation.h"
#include "Transaction.h"
#include "DailyPolicy.h"
#include "BundlePolicy.h"
#include "BarterPolicy.h"
#include "MenuLayout.h"
#include "UiLayout.h"
#include "SurfacePolicy.h"
#include "CleanupPolicy.h"
#include "Artwork.h"
#include <set>
#include <iostream>
#include <limits>
int checks=0;
void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        {
            const auto fixture=std::filesystem::path(argv[0]).parent_path()/"skse-settings-layout-fixture.ini";
            {std::ofstream out(fixture);out<<"[Layout]\nControlWidth=400\nFontScale=99\nHelpMode=2\nSectionGap=nan\n[General]\nCallFirst=0\nCleanupExpanded=1\n[Stock]\nListHeight=-1\n";}
            inn::settingsui::Layout layout;
            Check(layout.Load(fixture),"SKSE layout did not load");
            Check(layout.controlWidth==400&&layout.fontScale==1.5f&&layout.helpMode==2,"SKSE layout fields/clamping differ from editor schema");
            Check(layout.sectionGap==0&&layout.callFirst==0&&layout.cleanupExpanded==1&&layout.stockHeight==100,"Invalid layout value or grouped field not handled");
            std::filesystem::remove(fixture);
            Check(!layout.Load(fixture)&&layout.fontScale==1&&layout.callFirst==1,"Missing SKSE layout did not retain existing UI defaults");
        }
        {
            inn::ArtworkCatalog titles;
            titles.Load(std::filesystem::path(argv[1])/"inn_titles.tsv");
            Check(titles.entries.size()==19,"English title catalog missing inns");
            for(const auto& title:titles.entries) {
                const auto& art=title.image;
                const auto height=92.f*inn::InnTitleDisplayScale(art,false);
                const auto rowHeight=inn::InnTitleRowHeight(height,art,false);
                const auto top=rowHeight*.5f-height*inn::InnTitleBodyCenter(art,false);
                Check(top>=-.001f&&top+height<=rowHeight+.001f,"Optical alignment clips a full title stroke");
                Check(inn::InnTitleBodyCenter(art,true)==.5f&&inn::InnTitleRowHeight(height,art,true)==height,
                    "English alignment affects Chinese title positioning");
            }
            const auto fixture=std::filesystem::path(argv[0]).parent_path()/"title-anchor-fixture.tsv";
            auto write=[&](const std::string& extra){std::ofstream out(fixture);out<<"Skyrim.esm\t1\tlegacy\tLegacy\t旧标题\tlegacy.png\t0\t0\t1\t1\t2"<<extra<<"\n";};
            for(const std::string extra:{"","\t0","\t0\t1.18"}) {
                write(extra);titles.Load(fixture);
                Check(titles.entries[0].image.bodyCenterY==.5f,"Legacy title catalog lost centered default");
            }
            for(const std::string value:{"-0.1","1.1","nan","0.4bad"}) {
                write("\t0\t1.18\t"+value);
                bool rejected=false;try{titles.Load(fixture);}catch(const std::exception&){rejected=true;}
                Check(rejected,"Invalid title body center accepted");
            }
            std::filesystem::remove(fixture);
        }
        std::map<inn::Id,inn::Offer> offers{{10,{2,7,5}},{20,{1,4,2}},{30,{1,9,4}}};
        Check(!inn::Price({0,0,0},offers).valid,"Empty order accepted");
        Check(inn::Price({10,0,0},offers).total==7,"Single main price");
        Check(inn::Price({0,20,0},offers).total==4,"Drink-only order");
        Check(inn::Price({0,0,30},offers).total==9,"Side-only order");
        Check(inn::Price({10,20,30},offers).total==20,"Full meal price");
        {
            Check(inn::menuTabs==std::array{3,0,1,2},"Specialty tab must be first; All must be absent");
            Check(inn::MoveMenuTab(3,1)==0&&inn::MoveMenuTab(0,-1)==3&&inn::MoveMenuTab(2,1)==3,"Category cycling order");
            std::array<inn::Id,3> selection{},bundle{10,20,30};bool dailyMode=false;
            Check(inn::ToggleBundle(selection,dailyMode,bundle,false,offers)&&selection==bundle&&!dailyMode,"Whole house set selection");
            Check(inn::BundlePrice(bundle,offers,true).total==10,"Daily set price is not half the combined price");
            Check(inn::ToggleBundle(selection,dailyMode,bundle,true,offers)&&selection==bundle&&dailyMode,"Switching to daily set cancelled instead of applying discount");
            Check(!inn::BundleSelected(selection,dailyMode,bundle,false)&&inn::BundleSelected(selection,dailyMode,bundle,true),"Set cards ignore discount identity");
            Check(inn::ToggleBundle(selection,dailyMode,bundle,true,offers)&&selection==std::array<inn::Id,3>{}&&!dailyMode,"Second set click did not clear the whole meal");
            auto depleted=offers;depleted[20].stock=0;
            Check(!inn::ToggleBundle(selection,dailyMode,bundle,true,depleted)&&selection==std::array<inn::Id,3>{},"Unavailable daily set was selectable");
            std::map<inn::Id,inn::Offer> generated;
            std::array<inn::Id,3> house{inn::SourceKey(10,true),inn::SourceKey(20,true),inn::SourceKey(30,true)};
            for(auto [id,offer]:offers){offer.generated=true;offer.stock=0;generated[inn::SourceKey(inn::BaseForm(id),true)]=offer;}
            Check(inn::ToggleBundle(selection,dailyMode,house,false,generated),"House set incorrectly depends on merchant food stock");
            Check(!inn::BundleSelected({},false,{},false),"Empty meal shown as selected");
        }
        {
            struct Row {std::uint32_t id;int course,mask,menuMask;bool generated;};
            Row stockSoup{100,2,4,4,false},specialSoup{101,2,5,4,true};
            const auto generated=inn::RowKey(specialSoup);
            Check(!(inn::BrowseMask(stockSoup)&1)&&(inn::BrowseMask(stockSoup)&4),"Stock soup browse category");
            Check(inn::BrowseMask(specialSoup)==0,"Regional soup leaked into individual browsing");
            Check(inn::SelectionSlot(stockSoup,-1)==2&&inn::SelectionSlot(specialSoup,3)==2,"Soup default slot");
            std::array<inn::Id,3> selected{generated,102,103};
            std::map<inn::Id,inn::Offer> soupOffers{{100,{1,7,4}},{generated,{0,7,5,true}},{102,{1,4,2}},{103,{1,9,4}}};
            Check(inn::Price(selected,soupOffers).valid,"Authored soup/drink/dessert combo rejected");
            Check(inn::RowSelected(selected,specialSoup,2),"Preset soup not shown as selected");
            inn::ToggleRow(selected,specialSoup,2);
            Check(selected[0]==0&&selected[1]==102&&selected[2]==103,"Cancel soup changed remaining combo");
            inn::ToggleRow(selected,specialSoup,2);
            Check(selected[0]==0&&selected[2]==generated,"Single specialty soup did not use Side");
            Check(inn::Price(selected,soupOffers).valid,"Single specialty soup quote rejected");
            Check(!inn::Price({100,102,103},soupOffers).valid,"Stock soup still accepted as Main");
            Check(inn::Price({0,102,100},soupOffers).valid,"Stock soup rejected as Side");
            // Shared base forms must stay distinct: hide only the generated
            // source, preserve vendor browsing and stock-independent bundles.
            std::array<inn::Id,3> house{};std::map<inn::Id,inn::Offer> houseOffers;
            for(int slot=0;slot<3;++slot){
                Row stock{static_cast<unsigned>(200+slot),slot,1<<slot,1<<slot,false};
                auto regional=stock;regional.generated=true;
                Check((inn::BrowseMask(stock)&(1<<slot))!=0,"Ordinary stock dish hidden with regional counterpart");
                Check(inn::BrowseMask(regional)==0,"Regional dish remained in a category");
                regional.menuMask=-1;Check(inn::BrowseMask(regional)==0,"Fallback slot mask exposed regional dish");
                house[slot]=inn::RowKey(regional);houseOffers[house[slot]]={0,7,regional.mask,true};
            }
            bool houseDaily=false;std::array<inn::Id,3> houseSelection{};
            Check(inn::ToggleBundle(houseSelection,houseDaily,house,false,houseOffers)&&houseSelection==house,"Hiding regional dishes broke house bundle selection");
            const auto houseQuote=inn::BundlePrice(houseSelection,houseOffers,false);
            Check(houseQuote.valid&&houseQuote.total==21&&houseQuote.inventoryQuantities.empty(),"Hidden regional bundle lost pricing or depended on stock");
            std::map<inn::Id,inn::Offer> dailyOffers{{100,{1,7,4}},{102,{1,4,2}},{103,{1,9,4}},{104,{1,11,1}}};
            Check(!inn::DailyCategoriesMatch({100,102,103},dailyOffers),"Old soup-in-Main menu retained");
            Check(inn::DailyCategoriesMatch({104,102,100},dailyOffers),"New daily categories rejected");
            dailyOffers[100].stock=0;
            Check(inn::DailyCategoriesMatch({104,102,100},dailyOffers),"Stock depletion caused a daily redraw");
            for(unsigned day=1;day<=20;++day) {
                const auto daily=inn::PickDaily(123,day,dailyOffers);
                Check(daily[0]==104&&daily[2]!=104&&inn::Price(daily,dailyOffers).valid,"Daily soup category regression");
            }
        }
        Check(!inn::Price({20,0,0},offers).valid,"Wrong course accepted");
        Check(inn::Price({10,0,10},offers).quantities.at(10)==2,"Shared-slot quantity lost");
        offers[10].stock=1;
        Check(!inn::Price({10,0,10},offers).valid,"Double-count stock accepted");
        auto quote=inn::Price({10,20,0},offers);
        Check(!inn::CanPurchase(quote,10,true),"Insufficient gold accepted");
        Check(inn::CanPurchase(quote,11,true),"Exact gold rejected");
        Check(!inn::CanPurchase(quote,100,false),"Changed seat accepted");
        offers[20].stock=0;
        Check(!inn::Price({10,20,0},offers).valid,"Sold-out drink accepted");
        offers[10].price=0;
        Check(inn::Price({10,0,0},offers).valid,"Free food rejected");
        offers[10].price=-1;
        Check(!inn::Price({10,0,0},offers).valid,"Negative price accepted");
        offers[10]={2,std::numeric_limits<int>::max(),5};
        Check(!inn::Price({10,0,10},offers).valid,"Price overflow accepted");
        Check(!inn::Price({99,0,0},offers).valid,"Unknown food accepted");
        struct Store {
            std::map<inn::Id,int> stock{{10,2},{20,1}};
            int gold=100,merchantGold=20,charges=0;
            bool failStock=false,partialCharge=false,forbidStock=false;
            int stockReads=0,stockWrites=0;
            int Stock(inn::Id id){++stockReads;if(forbidStock)throw std::runtime_error("Generated-only order accessed vendor stock");return stock[id];}
            int Gold(){return gold;}
            void TakeStock(inn::Id id,int count){++stockWrites;if(!(failStock&&id==20))stock[id]-=count;}
            void ReturnStock(inn::Id id,int count){++stockWrites;stock[id]+=count;}
            void Charge(int count){++charges;if(partialCharge)--count;gold-=count;merchantGold+=count;}
            void Refund(int count){gold+=count;merchantGold-=count;}
        } store;
        offers={{10,{2,7,5}},{20,{1,4,2}}};
        quote=inn::Price({10,20,0},offers);
        auto phase=food::Phase::committing;
        Check(inn::Purchase(phase,quote,store),"Valid transaction rejected");
        Check(store.gold==89&&store.merchantGold==31&&store.stock[10]==1&&store.stock[20]==0,"Commit did not debit exact counts");
        Check(!inn::Purchase(phase,quote,store)&&store.charges==1,"Repeated commit charged twice");
        store=Store{};store.failStock=true;phase=food::Phase::committing;
        Check(!inn::Purchase(phase,quote,store),"Partial stock removal accepted");
        Check(store.stock[10]==2&&store.stock[20]==1&&store.gold==100&&store.charges==0,"Failed stock removal not rolled back");
        store=Store{};store.partialCharge=true;phase=food::Phase::committing;
        Check(!inn::Purchase(phase,quote,store),"Partial gold transfer accepted");
        Check(store.stock[10]==2&&store.stock[20]==1&&store.gold==100&&store.merchantGold==20,"Partial payment not refunded");
        store=Store{};phase=food::Phase::cancelled;
        Check(!inn::Purchase(phase,quote,store)&&store.charges==0,"Cancelled order charged");

        const auto generated=inn::SourceKey(10,true);
        Check(generated!=10&&inn::BaseForm(generated)==10&&inn::Generated(generated),"Generated offer key collided with its stock form");
        Check(inn::BaseForm(inn::SourceKey(0xFE123ABC,true))==0xFE123ABC,"Light-plugin food ID was truncated");
        offers={{10,{1,7,5,false}},{generated,{0,7,5,true}}};
        quote=inn::Price({generated,0,generated},offers);
        Check(quote.valid&&quote.total==14&&quote.inventoryQuantities.empty(),"Generated supply incorrectly required food stock");
        store=Store{};store.forbidStock=true;phase=food::Phase::committing;
        Check(inn::Purchase(phase,quote,store),"Generated-only purchase failed");
        Check(store.gold==86&&store.stockReads==0&&store.stockWrites==0,"Generated purchase touched vendor food inventory");
        quote=inn::Price({generated,0,10},offers);
        Check(quote.valid&&quote.inventoryQuantities.size()==1&&quote.inventoryQuantities.at(10)==1,"Mixed sources merged generated and vendor quantities");
        store=Store{};phase=food::Phase::committing;
        Check(inn::Purchase(phase,quote,store)&&store.stock[10]==1&&store.gold==86,"Mixed purchase deducted wrong inventory source");
        offers[10].stock=0;
        Check(!inn::Price({generated,0,10},offers).valid&&inn::Price({generated,0,generated},offers).valid,"Vendor stock outage affected generated dishes");
        offers[generated].generated=false;
        Check(!inn::Price({generated,0,0},offers).valid,"Invalid source discriminator accepted");
        offers={{generated,{999,7,5,true}}};
        Check(inn::PickDaily(1,1,offers)==std::array<inn::Id,3>{},"Inventory daily special included generated food");

        offers={{10,{1,12,5}},{11,{1,17,1}},{20,{2,15,2}},{21,{1,13,2}},{30,{1,16,4}}};
        const auto day=inn::CalendarDay(201,7,17);
        const auto daily=inn::PickDaily(123,day,offers);
        Check(daily==inn::PickDaily(123,day,offers),"Daily menu rerolled within the same inn/date");
        Check(daily[0]&&daily[1]&&daily[2]&&inn::Price(daily,offers).valid,"Daily menu does not fill available courses");
        std::set<std::array<inn::Id,3>> menus;
        for(unsigned n=0;n<40;++n)menus.insert(inn::PickDaily(123,day+n,offers));
        Check(menus.size()>1,"Date did not vary daily combinations");
        menus.clear();for(unsigned n=0;n<40;++n)menus.insert(inn::PickDaily(n,day,offers));
        Check(menus.size()>1,"Inn did not vary daily combinations");
        const auto normal=inn::Price(daily,offers);
        Check(inn::MenuPrice(daily,offers,true,daily).total==normal.total/2+normal.total%2,"Daily special half price incorrect");
        Check(inn::MenuPrice(daily,offers,false,daily).total==normal.total,"Separate order received daily discount");
        auto changed=daily;changed[1]=0;
        Check(!inn::MenuPrice(changed,offers,true,daily).valid,"Modified special retained discount");
        offers[daily[0]].stock=0;
        Check(!inn::MenuPrice(daily,offers,true,daily).valid,"Sold-out daily special accepted");
        offers={{10,{1,1,5}},{11,{1,2,1}},{20,{1,3,2}}};
        for(unsigned n=0;n<50;++n) {
            const auto bundle=inn::PickDaily(n,day,offers);
            Check(bundle[0]==11&&bundle[2]==10&&inn::Price(bundle,offers).valid,"Shared main/side stock prevented a complete special");
        }
        offers={{10,{1,1,1}}};
        Check(inn::MenuPrice({10,0,0},offers,true,{10,0,0}).total==1,"Half price made a one-gold meal free");
        Check(inn::PickDaily(1,day,{} )==std::array<inn::Id,3>{},"Empty stock created food");
        Check(inn::CalendarDay(201,7,18)>day,"Day key did not advance at midnight");

        inn::BarterInputs low{15,0,2,3.3f,1.05f},high{100,0,2,3.3f,1.05f};
        Check(inn::BuyPrice(100,inn::SpeechFactor(high),high.minBuyFactor)<inn::BuyPrice(100,inn::SpeechFactor(low),low.minBuyFactor),"Speech did not improve price");
        auto fortify=low;fortify.fortify=20;
        Check(inn::SpeechFactor(fortify)<inn::SpeechFactor(low),"Fortify Barter had no effect");
        auto capped=high;capped.speech=150;
        Check(inn::SpeechFactor(capped)==inn::SpeechFactor(high),"Speech cap exceeded");
        Check(inn::BuyPrice(100,.2f,1.05f)==105,"Minimum barter factor ignored");
        Check(inn::BuyPrice(0,2,1.05f)==0,"Free food acquired a fabricated value");
        Check(inn::BuyPrice(std::numeric_limits<int>::max(),3,1.05f)==-1,"Barter overflow accepted");
        Check(inn::BuyPrice(2,std::numeric_limits<float>::quiet_NaN(),1.05f)==-1,"NaN barter value accepted");

        inn::Surface table{1,0,{0,0,0},0,1,{-100,-50,100,50,60}};
        Check(inn::OnSurface(table,{0,0,60},0),"Foreign food on tabletop excluded");
        Check(inn::OnSurface(table,{0,0,72},-12),"Bowl bottom did not rest on tabletop");
        Check(!inn::OnSurface(table,{0,0,0},0),"Floor food was cleared");
        Check(!inn::OnSurface(table,{0,0,120},0),"Shelf food was cleared");
        Check(!inn::OnSurface(table,{140,0,60},0),"Adjacent table was cleared");
        auto rotated=table;rotated.yaw=1.5707963f;
        Check(inn::OnSurface(rotated,{0,-90,60},0)&&!inn::OnSurface(rotated,{90,0,60},0),"Rotated table boundary wrong");
        auto corner=table;corner.profile=33;corner.bounds={-30,-96,96,30,78};
        Check(!inn::OnSurface(corner,{70,-70,78},0)&&inn::OnSurface(corner,{0,-70,78},0),"L counter cutout was treated as a surface");
        auto bar=table;bar.profile=25;auto continuation=bar;continuation.reference=2;continuation.origin.x=200;
        Check(inn::ConnectedCounters(bar,continuation),"Touching bar segments not joined");
        continuation.origin.x=205;Check(!inn::ConnectedCounters(bar,continuation),"Gap between counters joined");
        Check(!inn::ConnectedCounters(table,table),"Ordinary adjacent tables joined");
        auto extended=table;extended.profile=105;
        Check(!inn::ConnectedCounters(extended,extended),"New ordinary table profile mistaken for counter");
        auto circle=extended;circle.shape=inn::SurfaceShape::ellipse;circle.bounds={-60,-60,60,60,65};
        Check(circle.ContainsXY({59,0,65})&&!circle.ContainsXY({55,55,65}),"Round table bounding-box corner accepted");
        Check(std::abs(circle.EdgeDistance({100,0,65})-40)<.001f,"Circle edge distance wrong");
        const auto square=inn::PlacementBounds(circle.bounds,circle.shape);
        Check(circle.ContainsXY({square[0],square[1],65},0)&&square[2]>42,"Safe circular support incorrect");
        Check(inn::TableModelKey("MESHES/Furniture/Noble/NobleTableRaound01.NIF")=="furniture\\noble\\nobletableraound01.nif","Model identity is not case/slash independent");
        for(float hx:{12.f,60.f,200.f})for(float hy:{8.f,40.f,100.f})for(int degrees=0;degrees<180;degrees+=7) {
            const float angle=degrees*3.14159265f/180.f;const auto support=inn::RotatedSupport(hx,hy,angle);
            for(float x:{-support[0],support[0]})for(float y:{-support[1],support[1]})
                Check(std::abs(x*std::cos(angle)-y*std::sin(angle))<=hx+.001f&&std::abs(x*std::sin(angle)+y*std::cos(angle))<=hy+.001f,"Rotated support projects outside actual table");
        }
        Check(inn::TablewareModel("clutter\\glazed\\glazedplate01.nif"),"Plate model missed");
        Check(inn::TablewareModel("custom\\BOWL_large.nif"),"Foreign bowl model missed");
        Check(!inn::TablewareModel("furniture\\DiningTablePlate.nif"),"Whole dining table classified as plate");
        for(const auto model:{"Clutter\\GlazedCup01.nif","Clutter\\GlazedGoblet01.nif",
            "Clutter\\Dining Set\\BasicTankard01.nif","Clutter\\Silver\\SilverGoblet02.nif",
            "Clutter\\Dwemer\\DweCup03.nif","Clutter\\Sovngarde\\SovGoblet01.nif",
            "custom/NobleCopperChalice_01.nif","custom/CUP_LARGE.nif","custom/WoodenMug01.nif"})
            Check(inn::TablewareModel(model)&&inn::DrinkwareModel(model),"Standalone drinking vessel excluded from cleanup");
        for(const auto model:{"furniture/Cupboard01.nif","furniture/DisplayShelfGoblet.nif",
            "furniture/CupTable01.nif","clutter/Cupcake01.nif","clutter/SmugglerChest01.nif"})
            Check(!inn::TablewareModel(model),"Furniture or unrelated object classified as a cup");
        Check(inn::OnSurface(table,{0,0,72},-12)&&!inn::OnSurface(table,{0,0,0},-12),"Cup cleanup must respect table height");
        Check(!inn::RemovePlate(true,2),"Eating only main removed order plate");
        for(const auto model:{"Clutter/Dining Set/BasicKnife01.nif","Clutter/Dining Set/BasicFork01.nif","Clutter/Dwemer/DweSpoon.nif","custom/WoodenSpoon01.nif","Oaristys/Dishes/NordKnife.nif"})
            Check(inn::UtensilModel(model),"Dining utensil excluded from cleanup");
        for(const auto model:{"Clutter/Common/PitchFork01.nif","Weapons/ButcherKnife.nif","fossils/Spoonwing.nif","Furniture/CupboardSpoon.nif","Furniture/DiningTableKnife.nif"})
            Check(!inn::UtensilModel(model),"Non-dining tool or furniture treated as cutlery");
        for(const auto model:{"Clutter/Wine/WineBottle02A.nif","Clutter/Wine/WineBottle02AEmpty.nif","Clutter/Wine/WineBottle01BEmpty.nif","custom/WineBottleWhiteStatic.nif","Clutter/Ingredients/Mead01.nif"})
            Check(inn::BeverageBottleModel(model),"Full/empty wine or mead bottle missed");
        Check(inn::BeverageBottleModel("Clutter\\Wine\\WineSanSpicedwineEmpty01.nif"),"Solitude spiced-wine empty bottle missed");
        Check(!inn::BeverageBottleModel("Furniture/WineBottleRack.nif"),"Entire wine rack was classified as a bottle");
        for(const auto path:{"Clutter/MeadBarrel01.nif","Clutter/Wine/WineCask01.nif","custom/BlackBriarMeadKeg01.nif"})
            Check(!inn::BeverageBottleModel(path)&&inn::BulkContainerModel(path),"Bulk barrel/cask misclassified as a drink bottle");
        Check(!inn::cleanup::Eligible(false,false,true,false),"Default cleanup removed ordinary decorations");
        Check(inn::cleanup::Eligible(true,false,true,false),"All-items cleanup did not include tabletop decorations");
        Check(inn::cleanup::Eligible(false,true,true,false),"Default cleanup rejected food/tableware");
        Check(!inn::cleanup::Eligible(true,true,true,true),"All-items cleanup removed the table/fixture");
        Check(!inn::cleanup::Eligible(true,false,false,false),"All-items cleanup accepted an actor/door");
        Check(inn::FixedStructureModel("RBMorthalMeshes/MorthalMoorsideInn16EXTRACTinterior.nif"),"Composite building was not protected");
        Check(!inn::FixedStructureModel("Clutter/MeadBarrel01.nif"),"User-selected all-items mode cannot remove tabletop barrel");
        Check(inn::FoodDisplayModel("_BYOH/Clutter/Food/Dumpling01.nif")&&inn::FoodDisplayModel("custom/Food/Jiaozi01.nif"),"Mod/static dumpling model missed");
        Check(!inn::FoodDisplayModel("Furniture/DumplingTable.nif"),"Whole display table was classified as a dumpling");
        Check(inn::RemovePlate(true,0),"Finishing all order food did not remove plate");
        Check(inn::RemovePlate(false,2),"Standing did not remove plate");
        for(const auto size:{std::pair(1920.f,1080.f),std::pair(1280.f,720.f),std::pair(2560.f,1440.f)})for(float bars:{0.f,.12f,.2f,.3f}) {
            const auto frame=inn::FitMenu(size.first,size.second,bars);
            Check(frame.y>=frame.safeTop+11&&frame.y+frame.height<=frame.safeBottom-11,"Menu overlaps cinematic bars");
        }

        inn::Serving serving{1,2,3,4,5,6,7,0,1,100,200,30};
        Check(inn::CanClear(serving,3,5,false,100,200,30),"Own table rejected");
        Check(!inn::CanClear(serving,8,5,false,100,200,30),"Other cell cleared");
        Check(!inn::CanClear(serving,3,8,false,100,200,30),"Other table cleared");
        Check(!inn::CanClear(serving,3,5,true,100,200,30),"Inventory item cleared");
        Check(!inn::CanClear(serving,3,5,false,120,200,30),"Moved player item cleared");
        {
            const std::array<std::uint32_t,1> sameTable{5},otherTable{8};
            const std::array<std::uint32_t,3> connectedTables{8,9,5};
            Check(inn::PaidAtTable(serving,40,3,sameTable),"Changing chairs at the same table lost paid-meal protection");
            Check(inn::PaidAtTable(serving,4,3,{}),"Temporarily missing captured table lost same-chair paid-meal protection");
            Check(!inn::PaidAtTable(serving,40,3,otherTable),"Unrelated occupied table blocked passive cleanup");
            Check(!inn::PaidAtTable(serving,4,30,sameTable),"Paid meal from another cell blocked passive cleanup");
            auto unpaid=serving;unpaid.committed=0;
            Check(!inn::PaidAtTable(unpaid,4,3,sameTable),"Uncommitted placement was treated as protected paid food");
            Check(inn::PaidAtTable(serving,40,3,connectedTables),"Paid meal on the final connected table escaped protection");
        }
        serving.committed=0;
        Check(!inn::CanClear(serving,3,5,false,100,200,30),"Unpaid reference cleared as served");
        for(auto version:inn::supportedRuntimes){Check(inn::Supported(version),"Target runtime rejected");Check(!inn::ExperimentalVR(version),"Flat runtime marked experimental VR");}
        Check(inn::Supported({1,4,15,0})&&inn::ExperimentalVR({1,4,15,0}),"Exact experimental VR runtime rejected");
        for(auto version:{inn::RuntimeVersion{1,4,14,0},inn::RuntimeVersion{1,4,16,0},inn::RuntimeVersion{1,4,15,1}})
            Check(!inn::Supported(version)&&!inn::ExperimentalVR(version),"Unknown VR runtime accepted");
        Check(!inn::Supported({1,7,999,0}),"Unknown future patch accepted");
        Check(!inn::Supported({1,9,32,0}),"Classic Skyrim accepted");
        Check(food::MoveGrid(1,4,2,0,1)==3,"Controller down navigation");
        Check(food::MoveGrid(2,3,2,1,0)==2,"Incomplete final row navigation");
        Check(argc==2,"Missing catalog fixture");
        inn::UiLayout ui;
        Check(ui.Load(std::filesystem::path(argv[1])/"PullUpAChairOrderingUI.ini").loaded,"Shared UI config missing");
        Check(std::isfinite(ui.innFont)&&ui.innFont>0&&std::isfinite(ui.dishFont)&&ui.dishFont>0&&ui.windowWidth>0,"Invalid shared layout configuration");
        const auto temporary=std::filesystem::temp_directory_path()/"inn-layout-parser-test.ini";
        {std::ofstream out(temporary);out<<"\xEF\xBB\xBF[Fonts]\nInnName=40\nDishName=nan\nDishPrice=999\nControls=12 junk\n[Window]\nOffsetX=80\nWidth=1000\nHeight=700\n[Header]\nInnY=-8\n[Unknown]\nTest=3\n";}
        const auto loaded=ui.Load(temporary);std::filesystem::remove(temporary);
        Check(loaded.loaded&&loaded.ignored==3&&loaded.clamped==1,"Invalid UI setting handling");
        Check(ui.innFont==40&&ui.dishFont==38&&ui.priceFont==36&&ui.controlsFont==28,"UI parser defaults and range handling");
        Check(ui.offsetX==80&&ui.windowWidth==1000&&ui.innY==-8,"UI offsets not parsed");
        const auto configured=inn::FitMenu(1920,1080,.12f,1,ui.windowWidth,ui.windowHeight,ui.offsetX,ui.offsetY);
        Check(configured.x==540&&configured.width==1000,"Configured width and position not applied");
        const auto clamped=inn::FitMenu(1280,720,.2f,1,1600,1000,800,-400);
        Check(clamped.y>=clamped.safeTop+11&&clamped.x+clamped.width<=1280-11,"Configured offset escaped safe area");
        Check(!ui.Load(temporary).loaded&&ui.innFont==46&&ui.offsetX==0,"Missing config did not restore defaults");
        inn::Catalog catalog;catalog.Load(argv[1]);
        Check(catalog.baseFoodCount==87 && catalog.foods.size()>=87&&catalog.tables.size()>=100&&catalog.props.size()==41,"Geometry catalog counts");
        Check(catalog.tableAliases.size()>=150,"Missing table base registrations");
        Check(catalog.embeddedAliases.size()==4,"Missing Moorside embedded regions");
        std::vector<inn::Surface> moorside;
        for(const auto& part:catalog.embeddedAliases) {
            moorside.push_back({0x860198F7,part.index,{0,0,0},0,1,catalog.tables[part.index],catalog.tableShapes[part.index],catalog.tableCounters[part.index],part.segment});
        }
        Check(inn::OnSurface(moorside[2],{147.6017f,190.3755f,827.7916f},0),"Screenshot white wine outside embedded counter");
        Check(!inn::OnSurface(moorside[0],{147.6017f,190.3755f,827.7916f},0)&&!inn::OnSurface(moorside[1],{147.6017f,190.3755f,827.7916f},0),"Another dining table owns bar wine");
        Check(inn::ConnectedCounters(moorside[2],moorside[3]),"Two contiguous embedded bar regions not joined");
        Check(!inn::ConnectedCounters(moorside[0],moorside[1]),"Two dining tables inside one mesh were merged");
        Check(!inn::OnSurface(moorside[2],{120,275,827.79f},0),"Moorside counter cutout treated as a tabletop");
        Check(inn::OnSurface(moorside[0],{-198.0077f,242.8271f,811.895f},0),"Existing west-table cheese excluded");
        bool nobleRound=false,moorsideRound=false;
        for(const auto& alias:catalog.tableAliases) {
            Check(alias.index>=0&&alias.index<static_cast<int>(catalog.tables.size()),"Table alias points outside catalog");
            if(alias.plugin=="skyrim.esm"&&alias.local==0xC29DE) {
                nobleRound=true;Check(alias.shape==inn::SurfaceShape::ellipse&&!alias.counter,"Reported round table has wrong surface kind");
            }
            if(alias.plugin=="redbag's morthal.esp"&&alias.local==0x26BE7) {
                moorsideRound=true;
                Check(alias.shape==inn::SurfaceShape::ellipse&&!alias.counter,"Moorside round table still uses its inscribed placement rectangle for cleanup");
                inn::Surface surface{0x026BE8,alias.index,{-238.85464f,624.72119f,749.67749f},5.6331825f,1.f,catalog.tables[alias.index],alias.shape,false,0};
                // Actual RedBag's Morthal ref 035075 (white wine 03133C).
                const inn::Point wine{-262.24353f,608.73871f,811.86591f};
                auto old=surface;old.shape=inn::SurfaceShape::rectangle;old.bounds={-26.70039f,-26.80499f,26.85516f,26.80506f,62.f};
                Check(!inn::OnSurface(old,wine,-.0002f),"Moorside fixture no longer reproduces the old missed-bottle boundary");
                Check(inn::OnSurface(surface,wine,-.0002f),"Real Moorside white wine is outside the corrected round tabletop");
                auto floorWine=wine;floorWine.z=surface.origin.z;
                Check(!inn::OnSurface(surface,floorWine,-.0002f),"Moorside cleanup reaches floor items");
                Check(!inn::OnSurface(surface,{147.6017f,190.3755f,827.7916f},-.0002f),"Moorside side table claimed a different table's bottle");
                const auto place=inn::PlacementBounds(surface.bounds,surface.shape);
                auto localSurface=surface;localSurface.origin={0,0,0};localSurface.yaw=0;
                for(float x:{place[0],place[2]})for(float y:{place[1],place[3]})
                    Check(localSurface.ContainsXY({x,y,62.f},0.f),"Moorside serving rectangle exceeds the round support");
                Check(place[2]-place[0]<55.f&&place[3]-place[1]<55.f,"Moorside serving incorrectly uses the full circular bounding box");
                for(unsigned i=0;i<4;++i)Check(std::abs(place[i]-old.bounds[i])<.01f,"Moorside cleanup fix changed the established safe serving area");
                Check(!localSurface.ContainsXY({surface.bounds[2],surface.bounds[3],62.f}),"Moorside round cleanup admits its bounding-box corner");
            }
        }
        Check(nobleRound,"Reported NobleTableRound01 missing");
        Check(moorsideRound,"Moorside 026BE7 round table missing");
        for(const auto& food:catalog.foods) {
            Check(food.width>0&&food.depth>0&&food.radius>0,"Invalid food footprint");
            Check(food.bounds[2]<=food.bounds[5],"Inverted vertical food bounds");
            Check(food.mask==0||(food.mask&(1<<food.course)),"Default course outside allowed slots");
        }
        const std::set<std::pair<std::string,std::uint32_t>> soups{
            {"dragonborn.esm",0x3CD5B},{"hearthfires.esm",0x353D},
            {"skyrim.esm",0xEBA01},{"skyrim.esm",0xEBA02},{"skyrim.esm",0xF4314},
            {"skyrim.esm",0xF431B},{"skyrim.esm",0xF431C},{"skyrim.esm",0xF431D},{"skyrim.esm",0xF431E},
            {"pull up a chair ordering.esp",0x80F},{"pull up a chair ordering.esp",0x812},
            {"pull up a chair ordering.esp",0x81E},{"pull up a chair ordering.esp",0x823},
            {"pull up a chair ordering.esp",0x824},{"pull up a chair ordering.esp",0x826},
            {"pull up a chair ordering.esp",0x829},{"pull up a chair ordering.esp",0x82D}};
        int soupCount=0;
        for(const auto& food:catalog.foods)if(soups.contains({food.plugin,food.local})) {
            ++soupCount;Check(food.course==2&&food.mask==4,"Soup catalog category not Side-only");
        }
        Check(soupCount==17,"Missing soup profile");
        inn::Localization en,zh;
        Check(en.Load(std::filesystem::path(argv[1])/"locales","en"),"English missing");
        Check(zh.Load(std::filesystem::path(argv[1])/"locales","zh"),"Chinese fallback missing");
        Check(std::string(en.Text("eat"))=="Eat","English lookup");
        Check(std::string(zh.Text("eat"))=="食用","Chinese UTF-8 lookup");
        Check(std::string(en.Text("unknown"))=="[missing translation]","Missing key fallback");
        const auto retainedEnglish=en.Text("eat");
        en.UseChinese(true);
        Check(en.IsChinese()&&std::string(en.Text("eat"))=="食用","English startup cannot switch to Chinese");
        Check(std::string(retainedEnglish)=="Eat","Language switch invalidated a previously returned English pointer");
        Check(en.Retranslate("Eat")=="食用","Cached notice did not follow the new language");
        const auto retainedChinese=en.Text("eat");
        en.UseChinese(false);
        Check(!en.IsChinese()&&std::string(en.Text("eat"))=="Eat","Chinese cannot switch back to English");
        Check(std::string(retainedChinese)=="食用","Language switch invalidated a previously returned Chinese pointer");
        Check(en.Retranslate("食用")=="Eat"&&en.Retranslate("unknown external text")=="unknown external text"&&en.Retranslate("").empty(),"Retranslation changed empty/external text or missed a cached notice");
        Check(std::string(zh.Text("bundle_price")).find("{}")==std::string::npos,"House promo still contains a price field");
        Check(std::string(en.Text("bundle_price")).find("{}")==std::string::npos,"English house promo still contains a price field");
        Check(std::string(zh.InnText("bundle_price","unknown_inn"))==zh.Text("bundle_price"),"Unmapped inn does not inherit localized promo");
        const auto promoFolder=std::filesystem::path(argv[0]).parent_path()/"inn-promo-test-fixture";
        std::filesystem::create_directories(promoFolder);
        {std::ofstream out(promoFolder/"zh.tsv");out<<"bundle_price\t默认推荐\nbundle_price.vilemyr\t登山前吃饱\nbundle_price.bannered_mare\t白漫招牌\nbundle_price.moorside\t\nbundle_price.frostfruit\t原样 {文字}\n";}
        {std::ofstream out(promoFolder/"en.tsv");out<<"bundle_price\tHouse pick\nbundle_price.vilemyr\tClimb with a full stomach\nbundle_price.bannered_mare\tWhiterun favourite\nbundle_price.moorside\tAn English-only override\nmultiline\tFirst\\nSecond\\tThird\\\\literal\n";}
        inn::Localization promoZh,promoEn;
        Check(promoZh.Load(promoFolder,"zh")&&promoEn.Load(promoFolder,"en"),"Promo fixture did not load");
        Check(std::string(promoEn.Text("multiline"))=="First\nSecond\tThird\\literal","Escaped multiline locale did not decode for the native UI");
        Check(std::string(promoZh.InnText("bundle_price","vilemyr"))=="登山前吃饱","Vilemyr promo did not resolve");
        Check(std::string(promoZh.InnText("bundle_price","bannered_mare"))=="白漫招牌","Inns share an override by mistake");
        Check(std::string(promoEn.InnText("bundle_price","vilemyr"))=="Climb with a full stomach","English inn promo did not resolve");
        Check(std::string(promoZh.InnText("bundle_price","moorside"))=="默认推荐","Blank Chinese override should use Chinese default");
        Check(std::string(promoZh.InnText("bundle_price",""))=="默认推荐","Empty inn identity fallback");
        Check(std::string(promoZh.InnText("bundle_price","frostfruit"))=="原样 {文字}","Promo braces must remain literal");
        promoZh.UseChinese(false);
        Check(std::string(promoZh.InnText("bundle_price","moorside"))=="An English-only override","Live language switch did not select the English inn promo");
        promoZh.UseChinese(true);
        Check(std::string(promoZh.InnText("bundle_price","moorside"))=="默认推荐","Switching back lost the Chinese blank-override fallback");
        std::filesystem::remove(promoFolder/"zh.tsv");std::filesystem::remove(promoFolder/"en.tsv");std::filesystem::remove(promoFolder);
        std::cout<<checks<<" core checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
