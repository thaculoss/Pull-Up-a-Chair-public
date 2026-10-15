int FoodIndex(RE::StaticFunctionTag*,RE::BGSListForm*,RE::TESForm* form) {
    if(form)if(auto i=profiles.find(form->GetFormID());i!=profiles.end())return i->second;
    return -1;
}
float ProfileValue(RE::StaticFunctionTag*,int index,int field) {
    if(index<0||index>=static_cast<int>(inn::catalog.foods.size()))return field==0?-1.0f:0.0f;
    const auto& p=inn::catalog.foods[index];
    switch(field){case 0:return float(p.course);case 1:return p.bounds[2];case 2:return p.width;case 3:return p.depth;case 4:return p.radius;case 5:return p.scale;case 6:return p.support;default:return 0;}
}
std::vector<float> ProfileBounds(RE::StaticFunctionTag*,int index) {
    if(index<0||index>=static_cast<int>(inn::catalog.foods.size()))return std::vector<float>(6);
    const auto& row=inn::catalog.foods[index].bounds;return {row.begin(),row.end()};
}
std::vector<float> TableSpec(RE::StaticFunctionTag*,int index) {
    if(index<0||index>=static_cast<int>(inn::catalog.tables.size()))return std::vector<float>(5);
    auto bounds=inn::catalog.tables[index];
    for(const auto& part:inn::catalog.embeddedAliases)if(part.index==index){bounds=part.placement;break;}
    const auto row=inn::PlacementBounds(bounds,inn::catalog.tableShapes[index]);return {row.begin(),row.end()};
}
std::vector<RE::TESObjectREFR*> FindTables(RE::StaticFunctionTag*,RE::TESObjectREFR* center,float radius){return inn::discovery::Nearby(center,radius);}
float TableMinSeatHeight(RE::StaticFunctionTag*,int index){return inn::catalog.MinimumSeatHeight(index);}
int TableIndex(RE::StaticFunctionTag*,RE::TESForm* base){return inn::discovery::Index(base?base->As<RE::TESBoundObject>():nullptr);}
int TableProfileAt(RE::StaticFunctionTag*,RE::TESObjectREFR* center,int slot){return inn::discovery::ProfileAt(center,slot);}
bool IsCounter(RE::StaticFunctionTag*,int index){return index>=0&&index<static_cast<int>(inn::catalog.tables.size())&&inn::catalog.tableCounters[index];}
bool IsRoundTable(RE::StaticFunctionTag*,int index) {
    if(index<0||index>=static_cast<int>(inn::catalog.tables.size())||inn::catalog.tableShapes[index]!=inn::SurfaceShape::ellipse)return false;
    const auto& b=inn::catalog.tables[index];const float x=b[2]-b[0],y=b[3]-b[1];return std::abs(x-y)<std::min(x,y)*.03f;
}
std::vector<float> PropBounds(RE::StaticFunctionTag*,int index) {
    if(index<0||index>=static_cast<int>(inn::catalog.props.size()))return std::vector<float>(6);
    const auto& row=inn::catalog.props[index];return {row.begin(),row.end()};
}
bool NeedsPlate(RE::StaticFunctionTag*,RE::TESForm* form) {
    const auto index=FoodIndex(nullptr,nullptr,form);
    return index>=0&&inn::catalog.foods[index].plate;
}
float ServingScale(RE::StaticFunctionTag*,RE::TESForm* form,int course) {
    const auto index=FoodIndex(nullptr,nullptr,form);
    if(index<0||(course!=0&&course!=2))return 1.f;
    const auto& food=inn::catalog.foods[index];
    if(!(food.mask&(1<<course)))return 1.f;
    const auto scale=inn::serving::CourseScale(course,food.width,food.depth,food.plate,food.scale,inn::serving::GetSettings());
    if(scale<1)logger::info("Compact serving plan base={:08X} course={} food_scale={} original_half_width={} original_half_depth={} plate={}",form->GetFormID(),course,scale,food.width,food.depth,course==0&&food.plate);
    return scale;
}
float MainServingScale(RE::StaticFunctionTag*,RE::TESForm* form){return ServingScale(nullptr,form,0);}
std::vector<float> SelectionScales(RE::StaticFunctionTag*,int token) {
    std::lock_guard lock(gate);
    if(token!=session.token||session.phase!=food::Phase::selected)return {};
    const auto settings=inn::serving::GetSettings();std::vector<float> result(3,1.f);
    for(int course:{0,2})if(auto index=profiles.find(inn::BaseForm(session.selected[course]));index!=profiles.end()) {
        const auto& food=inn::catalog.foods[index->second];
        if(food.mask&(1<<course))result[course]=inn::serving::CourseScale(course,food.width,food.depth,food.plate,food.scale,settings);
        if(result[course]<1)logger::info("Order scale snapshot token={} course={} base={:08X} scale={}",token,course,inn::BaseForm(session.selected[course]),result[course]);
    }
    return result;
}
bool TrackServing(RE::StaticFunctionTag*,int token,RE::TESObjectREFR* ref,RE::TESObjectREFR* seat,RE::TESObjectREFR* table,RE::BGSLocation* location,int kind) {
    std::lock_guard lock(gate);
    return token==session.token&&session.phase==food::Phase::selected&&inn::tables::Track(token,ref,seat,table,location,kind);
}
bool IsDiagnosticEnabled(RE::StaticFunctionTag*) {
    return GetPrivateProfileIntW(L"General",L"VerboseLogging",0,L".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini")!=0;
}
std::string Text(RE::StaticFunctionTag*,std::string key){return T(key);}
void ClearTable(RE::StaticFunctionTag*,RE::Actor* server){inn::tables::RequestClean(server);}
void RestoreTable(RE::StaticFunctionTag*,RE::Actor* server){inn::tables::RestoreTable(server);}
bool Bind(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("Begin","PullUpAChairOrdering",Begin);
    vm->RegisterFunction("GetStatus","PullUpAChairOrdering",State);
    vm->RegisterFunction("Selection","PullUpAChairOrdering",Selection);
    vm->RegisterFunction("Commit","PullUpAChairOrdering",Commit);
    vm->RegisterFunction("Cancel","PullUpAChairOrdering",Cancel);
    vm->RegisterFunction("FoodIndex","PullUpAChairOrdering",FoodIndex);
    vm->RegisterFunction("ProfileValue","PullUpAChairOrdering",ProfileValue);
    vm->RegisterFunction("ProfileBounds","PullUpAChairOrdering",ProfileBounds);
    vm->RegisterFunction("TableSpec","PullUpAChairOrdering",TableSpec);
    vm->RegisterFunction("TableMinSeatHeight","PullUpAChairOrdering",TableMinSeatHeight);
    vm->RegisterFunction("FindTables","PullUpAChairOrdering",FindTables);
    vm->RegisterFunction("TableIndex","PullUpAChairOrdering",TableIndex);
    vm->RegisterFunction("TableProfileAt","PullUpAChairOrdering",TableProfileAt);
    vm->RegisterFunction("IsCounter","PullUpAChairOrdering",IsCounter);
    vm->RegisterFunction("IsRoundTable","PullUpAChairOrdering",IsRoundTable);
    vm->RegisterFunction("PropBounds","PullUpAChairOrdering",PropBounds);
    vm->RegisterFunction("NeedsPlate","PullUpAChairOrdering",NeedsPlate);
    vm->RegisterFunction("MainServingScale","PullUpAChairOrdering",MainServingScale);
    vm->RegisterFunction("ServingScale","PullUpAChairOrdering",ServingScale);
    vm->RegisterFunction("SelectionScales","PullUpAChairOrdering",SelectionScales);
    vm->RegisterFunction("TrackServing","PullUpAChairOrdering",TrackServing);
    vm->RegisterFunction("IsDiagnosticEnabled","PullUpAChairOrdering",IsDiagnosticEnabled);
    vm->RegisterFunction("Text","PullUpAChairOrdering",Text);
    vm->RegisterFunction("ClearTable","PullUpAChairOrdering",ClearTable);
    vm->RegisterFunction("RestoreTable","PullUpAChairOrdering",RestoreTable);
    vm->RegisterFunction("CanWalkForService","PullUpAChairOrdering",+[](RE::StaticFunctionTag*,RE::Actor* server){return inn::service::CanWalkForService(server);});return true;
}
