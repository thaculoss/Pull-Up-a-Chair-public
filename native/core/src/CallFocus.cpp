#include "PCH.h"
#include "CallFocus.h"
#include "CallFocusPolicy.h"
#include "CallService.h"
#include "TableDiscovery.h"
#include "Localization.h"
#include "RuntimePolicy.h"
#include "SettingsStore.h"
#include <SKSE/InputMap.h>
namespace inn::call::focus {
namespace {
// Optional public BTPS V0 ABI: only release/acquire our own widget-hide request.
// Selection settings, scoring and every other caller's hide requests stay intact.
class BTPSApiV0 {
public:
    virtual bool GetSelectionEnabled() noexcept=0;
    virtual bool GetWidget3DEnabled() noexcept=0;
    virtual void HideSelectionWidget(std::string source) noexcept=0;
    virtual void ShowSelectionWidget(std::string source) noexcept=0;
};
BTPSApiV0* btps{};
constexpr auto owner="PullUpAChairOrdering.TableCall";
constexpr auto fieldName="PullUpAChairOrderingTableCall";
bool compatibilityHint{}; // Loaded once before UI tasks are registered.
PromptKind lastPromptKind=PromptKind::hidden; // Game thread only.
std::atomic_uint64_t uiGeneration{1};
std::atomic_uint64_t drawnGeneration{};
std::atomic_uint64_t hiddenGeneration{1};
bool widgetHidden{}; // UI thread only.
PromptPublication publication; // Game thread only.
std::mutex targetMutex;
struct Snapshot {RE::ObjectRefHandle target;RE::FormID seat{},cell{};std::uint64_t time{};} snapshot;
RE::FormID cachedSeat{},cachedCell{},lastTarget{};
std::uint64_t lastScan{};
std::vector<Surface> surfaces;
struct RenderState {
    RE::GFxMovieView* movie{};
    std::string text,font,source;
    bool configured{},embedded{},metricsLogged{};
    unsigned fontAttempt{};
    std::uint64_t writtenAt{};
} render; // UI thread only; reset when the HUD movie/text field is recreated.
struct FurnitureText {
    static bool Call(RE::TESBoundObject* base,RE::TESObjectREFR* ref,RE::BSString& text) {
        // Creating a text field alone is not proof that a replacement can be drawn.
        const auto drawn=drawnGeneration.load();
        if(drawn&&drawn==uiGeneration.load()&&ref&&FrameworkHeld()&&Target(RE::PlayerCharacter::GetSingleton()).get()==ref){text="";return false;}
        return next(base,ref,text);
    }
    static inline REL::Relocation<decltype(Call)> next;
};

void ReleaseWidget() {
    drawnGeneration=0;
    if(widgetHidden&&btps)btps->ShowSelectionWidget(owner);
    widgetHidden=false;
}
bool Number(RE::GFxValue& object,const char* name,double& result) {
    RE::GFxValue value;
    if(!object.GetMember(name,&value)||!value.IsNumber()||!std::isfinite(value.GetNumber()))return false;
    result=value.GetNumber();return true;
}
bool ConfigureFont(RE::GFxMovieView* movie,RE::GFxValue& root,RE::GFxValue& field) {
    RE::GFxValue format,native,embedded,font;
    bool inherited=false;
    if(render.fontAttempt==0) {
        root.GetMember("RolloverText",&native);
        if(!native.IsObject())movie->GetVariable(&native,"HUDMovieBaseInstance.RolloverText");
        if(!native.IsObject())movie->GetVariable(&native,"HUDMovieBaseInstance.Rollover.RolloverText");
        if(native.IsObject()) {
            native.Invoke("getTextFormat",&format,nullptr,0);
            if(!format.IsObject()||!format.GetMember("font",&font)||!font.IsString()||!font.GetString()[0]) {
                native.Invoke("getNewTextFormat",&format,nullptr,0);
                if(format.IsObject())format.GetMember("font",&font);
            }
            inherited=format.IsObject()&&font.IsString()&&font.GetString()[0];
            if(inherited)native.GetMember("embedFonts",&embedded);
        }
    }
    if(!inherited) {
        movie->CreateObject(&format,"TextFormat");font=RE::GFxValue("$EverywhereFont");
        if(!format.IsObject()||!format.SetMember("font",font))return false;
        // A mapped font library can be searched in device-font mode as well.
        // Preserve the native field's mode first; do not force local embedding.
        embedded=RE::GFxValue(render.fontAttempt==2);
    }
    if(!embedded.IsBool())embedded=RE::GFxValue(false);
    render.font=font.GetString();render.embedded=embedded.GetBool();
    render.source=inherited?"native RolloverText":"mapped game font";
    return format.SetMember("size",RE::GFxValue(22.0))&&format.SetMember("leading",RE::GFxValue(0.0))&&
        format.SetMember("color",RE::GFxValue(16777215.0))&&format.SetMember("align",RE::GFxValue("center"))&&
        field.SetMember("embedFonts",embedded)&&field.SetMember("multiline",RE::GFxValue(true))&&
        field.SetMember("wordWrap",RE::GFxValue(false))&&field.SetMember("selectable",RE::GFxValue(false))&&
        field.SetMember("textAutoSize",RE::GFxValue("shrink"))&&
        field.Invoke("setNewTextFormat",nullptr,&format,1)&&field.Invoke("setTextFormat",nullptr,&format,1);
}
void Draw(std::string text,std::uint64_t generation,bool suppressOriginal) {
    if(generation!=uiGeneration.load())return;
    if(text.empty())ReleaseWidget();
    auto ui=RE::UI::GetSingleton();auto hud=ui?ui->GetMenu<RE::HUDMenu>():nullptr;
    if(!hud||!hud->uiMovie) {
        ReleaseWidget();return;
    }
    auto movie=hud->uiMovie;RE::GFxValue root,field;
    const auto fail=[&](const char* stage){
        if(field.IsObject())field.SetMember("_visible",RE::GFxValue(false));
        ReleaseWidget();
        static std::string lastFailure;
        if(!text.empty()&&lastFailure!=stage){lastFailure=stage;logger::warn("DAK table prompt: draw failed at {}; original prompt retained",stage);}
    };
    const auto rootPath=compatibilityHint?"_root":"HUDMovieBaseInstance";
    if(!movie->GetVariable(&root,rootPath)||!root.IsObject()) {
        fail("HUD root");return;
    }
    root.GetMember(fieldName,&field);
    if(text.empty()) {
        const bool existed=field.IsObject();
        if(existed) {
            // Clear the content as well as visibility so a HUD refresh cannot
            // resurrect the old Gamepad A label while removal is pending.
            if(field.IsDisplayObject()) {
                field.SetText("");
                RE::GFxValue::DisplayInfo hidden;hidden.SetVisible(false);hidden.SetAlpha(0);
                field.SetDisplayInfo(hidden);
            }
            field.SetMember("text",RE::GFxValue(""));
            field.SetMember("_visible",RE::GFxValue(false));
            field.Invoke("removeTextField",nullptr,nullptr,0);
        }
        render={};
        RE::GFxValue remaining;
        if(!root.GetMember(fieldName,&remaining)||!remaining.IsObject()) {
            if(generation==uiGeneration.load()) {
                hiddenGeneration=generation;
                if(existed)logger::info("DAK table prompt: clear confirmed generation={}; owned text field removed",generation);
            }
        }else {
            static std::uint64_t warned{};
            if(warned!=generation){warned=generation;logger::warn("DAK table prompt: clear pending generation={}; field still exists, retrying",generation);}
        }
        return;
    }
    if(render.movie!=movie.get()||!field.IsObject()){render={};render.movie=movie.get();}
    if(!text.empty()&&!field.IsObject()) {
        RE::GFxValue depth;
        if(!root.Invoke("getNextHighestDepth",&depth,nullptr,0)||!depth.IsNumber()){fail("depth");return;}
        RE::GFxValue args[]{RE::GFxValue(fieldName),depth,RE::GFxValue(0.0),RE::GFxValue(0.0),RE::GFxValue(620.0),RE::GFxValue(144.0)};
        if(!root.Invoke("createTextField",nullptr,args,6)||!root.GetMember(fieldName,&field)||!field.IsObject()) {
            fail("createTextField");return;
        }
        RE::GFxValue shadow,filters;
        movie->CreateObject(&shadow,"flash.filters.DropShadowFilter");
        if(shadow.IsObject()) {
            shadow.SetMember("distance",RE::GFxValue(1.0));shadow.SetMember("strength",RE::GFxValue(3.0));
            shadow.SetMember("blurX",RE::GFxValue(3.0));shadow.SetMember("blurY",RE::GFxValue(3.0));
            movie->CreateArray(&filters);filters.PushBack(shadow);field.SetMember("filters",filters);
        }
    }
    if(!field.IsDisplayObject()){fail("text display object");return;}
    const auto frame=movie->GetVisibleFrameRect();
    if(!std::isfinite(frame.left)||!std::isfinite(frame.top)||!std::isfinite(frame.right)||!std::isfinite(frame.bottom)||frame.right<=frame.left||frame.bottom<=frame.top){fail("viewport");return;}
    // Convert screen coordinates through the actual parent transform. The
    // optional hint lives on the HUD movie root, outside moved/hidden rollover
    // containers, and leaves the original activation widgets alone.
    const auto expectedY=(frame.top+frame.bottom)*.5+(compatibilityHint?52.0:-12.0);
    RE::GFxValue center;movie->CreateObject(&center);
    if(!center.IsObject()||!center.SetMember("x",RE::GFxValue((frame.left+frame.right)*.5))||
       !center.SetMember("y",RE::GFxValue(expectedY))||
       !root.Invoke("globalToLocal",nullptr,&center,1)){fail("stage-to-HUD transform");return;}
    double x{},y{};
    if(!Number(center,"x",x)||!Number(center,"y",y)){fail("local coordinates");return;}
    if(!render.configured) {
        if(!ConfigureFont(movie.get(),root,field)){fail("native font setup");return;}
        render.configured=true;render.text.clear();
    }
    if(render.text!=text) {
        // Use the ActionScript text property. Do not reassign identical text at
        // every refresh and keep invalidating a newly created field's layout.
        if(!field.SetMember("text",RE::GFxValue(text.c_str()))){fail("text property write");return;}
        render.text=text;render.writtenAt=GetTickCount64();render.metricsLogged=false;
    }
    RE::GFxValue::DisplayInfo display;
    display.SetPosition(x-310.0,y);display.SetAlpha(100);display.SetVisible(true);
    if(!field.SetDisplayInfo(display)){fail("display position/visibility");return;}
    // Round-trip the field origin through the actual parent transform before
    // suppressing any other prompt. The coordinates must fit the visible stage.
    RE::GFxValue actual;movie->CreateObject(&actual);
    double localX{},localY{},stageX{},stageY{};
    if(!actual.IsObject()||!Number(field,"_x",localX)||!Number(field,"_y",localY)||
       !actual.SetMember("x",RE::GFxValue(localX+310.0))||!actual.SetMember("y",RE::GFxValue(localY))||
       !root.Invoke("localToGlobal",nullptr,&actual,1)||!Number(actual,"x",stageX)||!Number(actual,"y",stageY)||
       std::abs(stageX-(frame.left+frame.right)*.5)>2||std::abs(stageY-expectedY)>2){fail("screen position readback");return;}
    RE::GFxValue actualText;double width{},height{};
    const bool readText=field.GetMember("text",&actualText);
    const auto length=!readText?0:actualText.IsString()?std::char_traits<char>::length(actualText.GetString()):
        actualText.IsStringW()?std::char_traits<wchar_t>::length(actualText.GetStringW()):0;
    const bool readWidth=Number(field,"textWidth",width),readHeight=Number(field,"textHeight",height);
    const bool laidOut=length&&readWidth&&readHeight&&width>0&&height>0;
    if(!render.metricsLogged) {
        render.metricsLogged=true;
        logger::info("DAK table prompt: metrics attempt={} source={} font={} embedded={} text_read={} text_type={} length={} width_read={} width={:.2f} height_read={} height={:.2f} stage=({:.2f},{:.2f}) ready={}",
            render.fontAttempt,render.source,render.font,render.embedded,readText,static_cast<unsigned>(actualText.GetType()),length,readWidth,width,readHeight,height,stageX,stageY,laidOut);
    }
    if(!laidOut) {
        ReleaseWidget();
        // Leave the new field visible so Scaleform can advance its layout.
        // Original prompts stay intact during this short initialization period.
        if(GetTickCount64()-render.writtenAt<250)return;
        if(render.fontAttempt<2) {
            ++render.fontAttempt;render.configured=false;
            logger::info("DAK table prompt: trying font profile {} after pending layout",render.fontAttempt);
            return;
        }
        fail("text layout after font fallback");return;
    }
    if(generation!=uiGeneration.load()){fail("stale draw");return;}
    // Only the existing table replacement owns suppression. A general call
    // hint must never erase a food/NPC/BTPS activation prompt.
    drawnGeneration=suppressOriginal?generation:0;
    static bool logged=false;
    if(!logged){logged=true;logger::info("DAK table prompt: layout verified; stage=({:.2f},{:.2f}) local=({:.2f},{:.2f}) text=({:.2f},{:.2f}) source={} embedded={} parent={} compatibility={}; visual check still required",stageX,stageY,localX,localY,width,height,render.source,render.embedded,rootPath,compatibilityHint);}
    const bool hide=suppressOriginal&&btps&&btps->GetSelectionEnabled()&&btps->GetWidget3DEnabled();
    if(hide!=widgetHidden&&btps) {
        if(hide)btps->HideSelectionWidget(owner);else btps->ShowSelectionWidget(owner);
        widgetHidden=hide;
    }
}
void Publish(std::string text,bool suppressOriginal=false) {
    const auto now=GetTickCount64();
    if(!publication.Request(text,now,hiddenGeneration.load()))return;
    const auto generation=publication.generation;uiGeneration=generation;
    if(text.empty())drawnGeneration=0;
    SKSE::GetTaskInterface()->AddUITask([text=std::move(text),generation,suppressOriginal]{Draw(text,generation,suppressOriginal);});
}
RE::NiCamera* SceneCamera(RE::NiAVObject* object,int depth=0) {
    if(!object||depth>4)return nullptr;
    if(auto camera=netimmerse_cast<RE::NiCamera*>(object))return camera;
    if(auto node=object->AsNode())for(const auto& child:node->GetChildren())if(auto camera=SceneCamera(child.get(),depth+1))return camera;
    return nullptr;
}
RE::TESObjectREFR* HitReference(RE::NiAVObject* object) {
    for(int depth=0;object&&depth<8;++depth,object=object->parent)if(auto ref=object->GetUserData())return ref;
    return nullptr;
}
bool ClearRay(RE::PlayerCharacter* player,const Hit& hit,const RE::NiPoint3& origin,const RE::NiPoint3& direction) {
    auto cell=player->GetParentCell();auto world=cell?cell->GetbhkWorld():nullptr;if(!world)return false;
    const auto end=origin+direction*(hit.distance+2.f);
    const float scale=RE::bhkWorld::GetWorldScale();
    RE::bhkPickData pick{};pick.rayInput.from=RE::hkVector4(origin*scale);pick.rayInput.to=RE::hkVector4(end*scale);
    pick.ray=pick.rayInput.to-pick.rayInput.from;
    RE::CFilter playerFilter{};player->GetCollisionFilterInfo(playerFilter);
    pick.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kItemPick);
    pick.rayInput.filterInfo.SetSystemGroup(playerFilter.GetSystemGroup());
    world->PickObject(pick);
    auto object=pick.rayOutput.HasHit()?RE::TESHavokUtilities::FindCollidableObject(*pick.rayOutput.rootCollidable):nullptr;
    const auto ref=HitReference(object);
    return Unobstructed(hit,pick.rayOutput.HasHit(),ref?ref->GetFormID():0,pick.rayOutput.hitFraction*(hit.distance+2.f));
}
std::uint32_t ActivateKey() {
    const auto controls=RE::ControlMap::GetSingleton();const auto input=RE::BSInputDeviceManager::GetSingleton();
    if(!controls)return 0;
    const auto device=input&&input->IsGamepadEnabled()?RE::INPUT_DEVICE::kGamepad:RE::INPUT_DEVICE::kKeyboard;
    const auto key=controls->GetMappedKey("Activate",device);
    return key==0xFF?0:device==RE::INPUT_DEVICE::kGamepad?SKSE::InputMap::GamepadMaskToKeycode(key):key;
}
void ClearTarget() {
    {std::lock_guard lock(targetMutex);snapshot={};}
    if(lastTarget){logger::info("DAK tabletop focus released ref={:08X}",lastTarget);lastTarget=0;}
}
}
void Initialize() {
    std::string config;
    settingsstore::Read(config,L".\\Data\\SKSE\\Plugins\\PullUpAChairDAKHUD.ini");
    compatibilityHint=settingsstore::Integer(config,"Display","CompatibilityHint",0)==1;
    if(!FurnitureText::next.address()) {
        REL::Relocation<std::uintptr_t> furniture{RE::VTABLE_TESFurniture[0]};
        FurnitureText::next=furniture.write_vfunc(activateTextSlot,FurnitureText::Call);
    }
    if(auto module=GetModuleHandleW(L"BetterThirdPersonSelection.dll")) {
        using Request=void*(*)(std::uint32_t);
        if(auto request=reinterpret_cast<Request>(GetProcAddress(module,"RequestPluginAPI")))btps=static_cast<BTPSApiV0*>(request(0));
    }
    logger::info("DAK tabletop prompt revision=5 compatibility_hint={} parent={}; acknowledged field removal; inherited native font; BTPS widget API={}",compatibilityHint,compatibilityHint?"_root":"HUDMovieBaseInstance",btps!=nullptr);
}
void UpdateTarget(RE::PlayerCharacter* player,bool enabled) {
    auto seat=player?player->GetOccupiedFurniture().get():RE::NiPointer<RE::TESObjectREFR>{};
    auto cell=player?player->GetParentCell():nullptr;
    if(!enabled||!seat||!cell){ClearTarget();return;}
    const auto now=GetTickCount64();
    if(cachedSeat!=seat->GetFormID()||cachedCell!=cell->GetFormID()||now-lastScan>=500) {
        cachedSeat=seat->GetFormID();cachedCell=cell->GetFormID();lastScan=now;surfaces=discovery::Surfaces(seat.get(),180.f);
    }
    auto playerCamera=RE::PlayerCamera::GetSingleton();
    auto camera=playerCamera?SceneCamera(playerCamera->cameraRoot.get()):nullptr;
    RE::NiPoint3 origin,direction;
    if(!camera||!camera->WindowPointToRay(500,500,origin,direction,1000.f,1000.f)){ClearTarget();return;}
    direction.Unitize();const auto p=seat->GetPosition();
    auto hit=PickTable(surfaces,{p.x,p.y,p.z},{origin.x,origin.y,origin.z},{direction.x,direction.y,direction.z});
    if(!hit||!ClearRay(player,*hit,origin,direction)){ClearTarget();return;}
    auto ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(hit->reference);
    if(!ref||ref->GetParentCell()!=cell||ref->IsDisabled()||ref->IsMarkedForDeletion()){ClearTarget();return;}
    {std::lock_guard lock(targetMutex);snapshot={ref->GetHandle(),seat->GetFormID(),cell->GetFormID(),now};}
    if(lastTarget!=hit->reference){lastTarget=hit->reference;logger::info("DAK tabletop focus ref={:08X} seat={:08X} first_person={}; raw camera ray owns call target",lastTarget,seat->GetFormID(),playerCamera->IsInFirstPerson());}
}
void UpdatePrompt(bool enabled,bool cameraTable,bool canDispatch) {
    const auto kind=ChoosePrompt(compatibilityHint,enabled,cameraTable,canDispatch);
    if(kind!=lastPromptKind){
        if(compatibilityHint)logger::info("DAK compatibility hint visible={} camera_table={} call_allowed={}; input and target selection unchanged",kind!=PromptKind::hidden,cameraTable,canDispatch);
        lastPromptKind=kind;
    }
    if(kind==PromptKind::hidden){Publish({});return;}
    const auto action=fmt::format("[{}]",KeyName(ActivateKey()));
    if(kind==PromptKind::compatible)Publish(fmt::format("{} {}",action,T("call_table_action")));
    else Publish(fmt::format("+\n{} {}\n{}",action,T("call_table_action"),T("call_table_name")),true);
}
RE::NiPointer<RE::TESObjectREFR> Target(RE::PlayerCharacter* player) {
    if(!player||!player->GetParentCell())return {};
    auto seat=player->GetOccupiedFurniture().get();if(!seat)return {};
    std::lock_guard lock(targetMutex);
    if(!Fresh(snapshot.seat,snapshot.cell,seat->GetFormID(),player->GetParentCell()->GetFormID(),snapshot.time,GetTickCount64()))return {};
    return snapshot.target.get();
}
void Reset() {
    cachedSeat=cachedCell=0;lastScan=0;surfaces.clear();
    publication.Invalidate();ClearTarget();lastPromptKind=PromptKind::hidden;Publish({});
}
}
