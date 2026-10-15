#include "API.h"
#include "RenderManager.h"
#include "ThumbnailAppearance.h"
#include "ThumbnailBounds.h"
#include "RequiredUi.h"
#include "Policy.h"
#include "Navigation.h"
#include "OrderPolicy.h"
#include "DailyPolicy.h"
#include "BundlePolicy.h"
#include "DailyMenus.h"
#include "BarterPricing.h"
#include "MenuLayout.h"
#include "UiLayout.h"
#include "Artwork.h"
#include "TableCleanup.h"
#include "Specialties.h"
#include "Transaction.h"
#include "Catalog.h"
#include "Localization.h"
#include "InnServices.h"
#include "SupplierServices.h"
#include "TableManager.h"
#include "TableDiscovery.h"
#include "RuntimePolicy.h"
#include "ServingSize.h"
#include "CallService.h"
#include "RecordedSounds.h"
#include "CallSettingsUI.h"
#include "ServiceApproach.h"
#include "ServiceSceneReservation.h"
#include "TopicVisibility.h"
#include <Xinput.h>
#include <numbers>
namespace ImGui=ImGuiMCP;
using namespace ImGuiMCP;
using inn::T;
#include "MenuTypeface.h"
namespace {
constexpr auto cacheDir="Data/SKSE/Plugins/PullUpAChairOrdering/Cache";
struct Item {
    RE::FormID id{};
    std::string name,model,image,error;
    int count{},course{-1},mask{},price{},menuMask{-1};
    bool attempted{},generated{},textureAttempted{};
    ImTextureID texture{};
    inn::ThumbnailBounds imageBounds;
    bool imageBoundsAttempted{};
};
struct Session {
    int token{},price{};
    food::Phase phase{food::Phase::cancelled};
    RE::ObjectRefHandle seller,stock,seat,merchant;
    RE::FormID cell{},inn{};
    inn::suppliers::Identity supplier;
    std::string sellerName,status,innTextKey;
    inn::Artwork titleArt;
    ImTextureID titleArtTexture{};
    bool titleArtAttempted{};
    std::array<inn::Artwork,2> bundleArt;
    std::array<ImTextureID,2> bundleArtTextures{};
    std::array<bool,2> bundleArtAttempted{};
    std::vector<Item> items;
    std::array<inn::Id,3> selected{},daily{},specialty{};
    std::uint32_t day{};
    bool dailyMode{},confirmationQueued{},hideEmptySpecialty{};
    int highlighted{-1},tab{3},bundleHighlighted{0};
    std::array<char,128> search{};
};
std::mutex gate;
Session session;
std::atomic_bool thumbQueued{},ready{},purgeAssets{},validationQueued{};
SKSEMenuFramework::Model::WindowInterface* window{};
std::unordered_map<std::string,std::uint64_t> assetHashes;
std::unordered_map<RE::FormID,std::string> imageCache;
std::vector<std::string> disposeImages;
std::unordered_map<RE::FormID,int> profiles;
std::map<RE::FormID,inn::Artwork> venueArtwork;
std::map<RE::FormID,std::string> venueTextKeys;
std::map<RE::FormID,inn::Artwork> venueTitleArtwork;
bool rendererReady{};
food::PadRepeat padRepeat;
int padToken{};
bool padSeen{},scrollToHighlight{};
void SwitchMenuTab(int tab) {
    if(session.tab==tab)return;
    session.tab=tab;scrollToHighlight=true;
    const auto token=session.token;
    SKSE::GetTaskInterface()->AddTask([token]{
        std::lock_guard lock(gate);
        if(session.token==token&&(session.phase==food::Phase::open||session.phase==food::Phase::selected)&&window&&window->IsOpen)inn::recorded::PageTurn();
    });
}
#include "Gamepad.inl"
const char* Course(int n){return T(n==0?"main":n==1?"drink":n==2?"side":"other");}
bool IsFood(RE::TESBoundObject& object) {
    auto food=object.As<RE::AlchemyItem>();
    if(!food||food->IsPoison()||food->HasKeywordString("VendorItemFoodRaw"))return false;
    const auto profile=profiles.find(food->GetFormID());
    return profile!=profiles.end()&&inn::catalog.foods[profile->second].mask!=0;
}
bool SameContext() {
    auto player=RE::PlayerCharacter::GetSingleton();auto server=session.seller.get();
    auto location=inn::FindInn(player);
    return player&&server&&player->GetParentCell()&&server->GetParentCell()==player->GetParentCell()&&
        player->GetParentCell()->GetFormID()==session.cell&&player->GetOccupiedFurniture()==session.seat&&
        inn::IsSeated(player)&&!player->IsInCombat()&&!server->As<RE::Actor>()->IsDead()&&location&&location->GetFormID()==session.inn;
}
void RetireImages(){
    std::unordered_set<std::string> seen;
    for(auto& row:session.items)if(row.texture&&!row.image.empty()&&seen.insert(row.image).second)disposeImages.push_back(row.image);
    for(int i=0;i<2;++i)if(session.bundleArtTextures[i]&&!session.bundleArt[i].path.empty()&&seen.insert(session.bundleArt[i].path).second)
        disposeImages.push_back(session.bundleArt[i].path);
    if(session.titleArtTexture&&!session.titleArt.path.empty()&&seen.insert(session.titleArt.path).second)
        disposeImages.push_back(session.titleArt.path);
}
void CancelUnlocked(){
    const auto seller=session.seller;const auto token=session.token;
    session.phase=food::Phase::cancelled;if(window)window->IsOpen=false;
    const auto generation=inn::service::Generation();
    SKSE::GetTaskInterface()->AddTask([seller,token,generation]{if(generation!=inn::service::Generation())return;auto ref=seller.get();inn::service::OrderFinished(ref?ref->As<RE::Actor>():nullptr,token,false);});
}
inn::Quote CurrentQuote() {
    std::map<inn::Id,inn::Offer> offers;
    for(const auto& row:session.items)offers[inn::RowKey(row)]={row.count,row.price,row.mask,row.generated};
    return inn::MenuPrice(session.selected,offers,session.dailyMode,session.daily);
}
bool UpdateQuote(){const auto quote=CurrentQuote();session.price=quote.total;return quote.valid;}
#include "BundleSelection.inl"
int SelectedSlot(const Item& row) {
    return inn::SelectionSlot(row,session.tab);
}
bool IsSelected(const Item& row){return inn::RowSelected(session.selected,row,session.tab);}
void SelectItem(const Item& row) {
    session.dailyMode=false;session.status=T("choose");
    inn::ToggleRow(session.selected,row,session.tab);
    UpdateQuote();
}
#include "Thumbnails.inl"
#include "OrderSession.inl"
#include "NativeBindings.inl"
ImGuiIO* GalleryIO(){return ImGui::GetIO();}
#include "ParchmentMenu.inl"
void LoadTitleArtwork() {
    // Language chooses the existing title catalog; artwork and its layout are unchanged.
    venueTitleArtwork.clear();
    try {
        const auto titleFile=inn::InnTitleCatalogPath("Data/SKSE/Plugins/PullUpAChairOrdering/catalog",inn::strings.IsChinese());
        if(std::filesystem::exists(titleFile)) {
            inn::ArtworkCatalog titles;titles.Load(titleFile);
            const auto data=RE::TESDataHandler::GetSingleton();
            for(const auto& entry:titles.entries)if(entry.local)
                if(auto location=data->LookupForm<RE::BGSLocation>(entry.local,entry.plugin))
                    venueTitleArtwork[location->GetFormID()]=entry.image;
            logger::info("Inn title artwork: {} location mappings, catalog={}",venueTitleArtwork.size(),titleFile.string());
        }
    }catch(const std::exception& e){logger::warn("Inn title artwork unavailable: {}; retaining text titles",e.what());}
}
void RefreshLanguageArtwork() {
    std::lock_guard lock(gate);
    LoadTitleArtwork();
    session.status=inn::strings.Retranslate(session.status);
    if(session.titleArtTexture&&!session.titleArt.path.empty())disposeImages.push_back(session.titleArt.path);
    session.titleArt={};session.titleArtTexture={};session.titleArtAttempted=false;
    auto location=RE::TESForm::LookupByID<RE::BGSLocation>(session.inn);
    for(int depth=0;location&&depth<12;++depth,location=location->parentLoc)
        if(auto title=venueTitleArtwork.find(location->GetFormID());title!=venueTitleArtwork.end()) {
            session.titleArt=title->second;break;
        }
}
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if(message->type==SKSE::MessagingInterface::kDataLoaded) {
        inn::LoadLanguage();inn::LoadMenuLayoutSettings();inn::serving::LoadSettings();
        inn::uiLayout.Load("Data/SKSE/Plugins/PullUpAChairOrderingUI.ini");
        try {inn::catalog.Load("Data/SKSE/Plugins/PullUpAChairOrdering/catalog");}
        catch(const std::exception& e){logger::error("Catalog rejected: {}",e.what());return;}
        try {inn::specialties::Load("Data/SKSE/Plugins/PullUpAChairOrdering/catalog");}
        catch(const std::exception& e){logger::error("House specialties unavailable: {}",e.what());}
        inn::discovery::Initialize();
        inn::suppliers::Initialize();
        auto data=RE::TESDataHandler::GetSingleton();
        try {
            inn::innArtwork.Load("Data/SKSE/Plugins/PullUpAChairOrdering/catalog/inn_artwork.tsv");
            venueArtwork.clear();
            venueTextKeys.clear();
            for(const auto& entry:inn::innArtwork.entries)if(entry.local)
                if(auto location=data->LookupForm<RE::BGSLocation>(entry.local,entry.plugin)) {
                    venueArtwork[location->GetFormID()]=entry.image;
                    venueTextKeys[location->GetFormID()]=entry.key;
                }
            logger::info("Inn artwork: {} location mappings",venueArtwork.size());
        }catch(const std::exception& e){logger::warn("Inn artwork unavailable: {}",e.what());}

        LoadTitleArtwork();
        inn::call::ui::SetLanguageChangedHandler(+[]{SKSE::GetTaskInterface()->AddTask(RefreshLanguageArtwork);});

        profiles.clear();
        for(const auto& error:inn::catalog.foodPackErrors)logger::warn("Food pack isolated: {}",error);
        std::map<std::string,std::array<int,3>> packCounts;
        for(std::size_t i=0;i<inn::catalog.foods.size();++i) {
            const auto& p=inn::catalog.foods[i];
            auto form=data->LookupForm<RE::AlchemyItem>(p.local,p.plugin);
            if(!form){if(!p.pack.empty())++packCounts[p.pack][1];continue;}
            const auto model=form->GetModel();
            if(!p.MatchesModel(model?model:"")) {
                ++packCounts[p.pack][2];logger::warn("Food pack model mismatch pack={} form={:08X} expected={} actual={}",p.pack,form->GetFormID(),p.expectedModel,model?model:"");continue;
            }
            profiles[form->GetFormID()]=static_cast<int>(i);
            if(!p.pack.empty())++packCounts[p.pack][0];
        }
        for(const auto& [pack,counts]:packCounts)logger::info("Food pack {} active={} absent={} model_mismatch={}",pack,counts[0],counts[1],counts[2]);
        inn::service::reservation::Initialize();
        SKSE::GetTaskInterface()->AddTask(inn::tables::Install);
        auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
        if(!module||!GetProcAddress(module,"AddWindow")||!GetProcAddress(module,"LoadTexture")||!GetProcAddress(module,"DisposeTexture")){logger::error("SKSE Menu Framework image API unavailable");return;}
        for(auto name:requiredUiExports)if(!GetProcAddress(module,name)){logger::error("Missing menu API: {}",name);return;}
        menuFramework=module;
        inn::menufont::Initialize(module);
        window=SKSEMenuFramework::AddWindow(Draw,true);ready=window!=nullptr;
        inn::call::SetOrderGuard([]{
            std::lock_guard lock(gate);
            return (window&&window->IsOpen)||session.phase==food::Phase::open||session.phase==food::Phase::selected||session.phase==food::Phase::committing;
        });
        inn::call::Initialize();inn::call::ui::Initialize();inn::serving::InitializeSettingsPage();
        inn::service::BackendDataLoaded(ready.load());
        logger::info("Independent ordering UI ready={}, supported food records={}",ready.load(),profiles.size());
    } else if(message->type==SKSE::MessagingInterface::kPreLoadGame||message->type==SKSE::MessagingInterface::kNewGame) {
        inn::call::Reset();inn::call::ui::Reset();
        std::lock_guard lock(gate);RetireImages();CancelUnlocked();session.token=0;purgeAssets=true;
        validationQueued=false;
        if(message->type==SKSE::MessagingInterface::kNewGame){inn::topics::QueueSync();inn::tables::Revert();inn::daily::Revert();inn::suppliers::Revert();}
    } else if(message->type==SKSE::MessagingInterface::kPostLoadGame) {
        inn::topics::QueueSync();
        inn::tables::Recover();
        inn::service::Recover();
    }
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    const auto v=skse->RuntimeVersion();
    if(!inn::Supported({v.major(),v.minor(),v.patch(),v.build()}))return false;
    SKSE::Init(skse);auto path=SKSE::log::log_directory();if(!path)return false;*path/="PullUpAChairOrdering.log";
    auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("InnOrdering",sink));
    spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);
    SKSE::GetPapyrusInterface()->Register(Bind);
    auto io=SKSE::GetSerializationInterface();io->SetUniqueID(0x494F4332);
    io->SetSaveCallback([](SKSE::SerializationInterface* io){inn::tables::Save(io);inn::daily::Save(io);inn::cleanup::Save(io);inn::suppliers::Save(io);inn::service::Save(io);});io->SetLoadCallback(inn::tables::Load);
    io->SetRevertCallback([](SKSE::SerializationInterface*){inn::tables::Revert();inn::daily::Revert();inn::suppliers::Revert();inn::service::PrepareLoad();});
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    logger::info("Pull Up a Chair Ordering 0.9.9 beta vrtrial1 runtime={}; Solitude empty spiced-wine bottles included in tabletop cleanup; New Gnisis hides its empty specialty card; game-thread gold preflight before serving; precise commit rejection diagnostics retained; vanilla low tables, Chinese menu font and DAK HUD compatibility retained; diningmodule1: optional interaction API v1, core has no food/plate vfunc hooks; removed serving receipts persist across saves; mode={}; gameplay validation required",v.string(),inn::ExperimentalVR({v.major(),v.minor(),v.patch(),v.build()})?"experimental VR (unverified)":"flat runtime");
    return true;
}
