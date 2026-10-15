// Called under the UI order lock. Read inventory only on the game thread;
// keep the menu open until the selected order passes the gold/context check.
void RequestConfirmationUnlocked() {
    if(session.phase!=food::Phase::open||session.confirmationQueued)return;
    session.confirmationQueued=true;
    const auto token=session.token;
    const auto selected=session.selected;
    const auto generation=inn::service::Generation();
    SKSE::GetTaskInterface()->AddTask([token,selected,generation]{
        std::lock_guard lock(gate);
        if(generation!=inn::service::Generation()||token!=session.token)return;
        session.confirmationQueued=false;
        if(session.phase!=food::Phase::open||!window||!window->IsOpen||session.selected!=selected)return;
        const auto quote=CurrentQuote();
        if(!quote.valid){session.status=T("stock_changed");return;}
        if(!SameContext()){CancelUnlocked();return;}
        auto player=RE::PlayerCharacter::GetSingleton();
        auto gold=RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
        if(!player||!gold){session.status=T("unavailable");return;}
        const auto available=player->GetItemCount(gold);
        if(available<quote.total) {
            session.status=T("gold_missing");
            logger::info("Order confirmation rejected order={} reason=insufficient_gold price={} gold={}; menu retained, no serving created",token,quote.total,available);
            return;
        }
        session.phase=food::Phase::selected;window->IsOpen=false;
        logger::info("Order confirmation accepted order={} price={} gold={}; preparing serving",token,quote.total,available);
    });
}
void Snapshot(int token) {
    std::lock_guard lock(gate);
    if(session.token!=token||session.phase!=food::Phase::open)return;
    const auto layout=inn::uiLayout.Load("Data/SKSE/Plugins/PullUpAChairOrderingUI.ini");
    logger::info("UI layout loaded={} ignored={} clamped={}",layout.loaded,layout.ignored,layout.clamped);
    auto server=session.seller.get();
    if(!SameContext()||!server){CancelUnlocked();return;}
    auto actor=server->As<RE::Actor>();
    if(const auto common=inn::innArtwork.Find("recommendation"))session.bundleArt[1]=common->image;
    auto artworkLocation=server->GetCurrentLocation();
    for(int depth=0;artworkLocation&&depth<12;++depth,artworkLocation=artworkLocation->parentLoc)
        if(auto image=venueArtwork.find(artworkLocation->GetFormID());image!=venueArtwork.end()){
            session.bundleArt[0]=image->second;
            if(auto key=venueTextKeys.find(artworkLocation->GetFormID());key!=venueTextKeys.end())session.innTextKey=key->second;
            break;
        }

    const auto supplier=inn::suppliers::Resolve(actor);auto merchant=supplier.merchant;
    if(!merchant){session.status=T("supplier_unavailable");logger::warn("No qualified inn supplier inn={:08X}; refusing server-specific fallback",session.inn);return;}
    session.supplier=supplier.id;
    session.merchant=merchant->GetHandle();session.day=inn::daily::Today();
    auto chest=supplier.stock;
    if(chest)session.stock=chest->GetHandle();
    int pricingFailures=0;
    auto append=[&](RE::TESBoundObject* object,int count,int course,int mask,bool generated) {
        if(!object||!object->GetPlayable()||!profiles.contains(object->GetFormID()))return;
        Item row;row.id=object->GetFormID();row.name=object->GetName();row.count=count;
        const auto& profile=inn::catalog.foods.at(profiles.at(row.id));
        row.course=generated?profile.course:course;row.menuMask=profile.mask;
        row.mask=mask|(generated?profile.mask:0);row.generated=generated;row.price=inn::BuyCost(object,merchant);
        if(row.price<0){++pricingFailures;logger::warn("Offer omitted: invalid price form={:08X} generated={}",row.id,generated);return;}
        if(auto model=object->As<RE::TESModel>();model&&model->GetModel())row.model=model->GetModel();
        session.items.push_back(std::move(row));
    };
    if(chest)for(auto& [object,data]:chest->GetInventory(IsFood)) {
        if(!object||data.first<=0||!data.second)continue;
        const auto& profile=inn::catalog.foods.at(profiles.at(object->GetFormID()));
        append(object,data.first,profile.course,profile.mask,false);
    }
    // Authored house specials are a separate supply source; zero vendor stock is irrelevant.
    const auto specialties=inn::specialties::ForInn(inn::FindInn(server.get()));
    for(int slot=0;slot<3;++slot)if(specialties[slot]) {
        session.specialty[slot]=inn::SourceKey(specialties[slot],true);
        append(RE::TESForm::LookupByID<RE::AlchemyItem>(specialties[slot]),0,slot,1<<slot,true);
    }
    std::sort(session.items.begin(),session.items.end(),[](const auto& a,const auto& b){return std::tie(a.course,a.generated,a.name)<std::tie(b.course,b.generated,b.name);});
    session.highlighted=session.items.empty()?-1:0;
    std::map<inn::Id,inn::Offer> offers;
    for(const auto& row:session.items)offers[inn::RowKey(row)]={row.count,row.price,row.mask,row.generated};
    session.daily=inn::daily::Menu(inn::FindInn(server.get()),session.day,offers);
    session.status=T(session.items.empty()?(pricingFailures?"pricing_unavailable":"no_stock"):"choose");UpdateQuote();
    const auto generated=std::count_if(session.items.begin(),session.items.end(),[](const auto& row){return row.generated;});
    logger::info("Snapshot order={} inn={:08X} chest={:08X} merchant={:08X} stock_types={} generated_types={} day={} invalid_prices={}",token,session.inn,chest?chest->GetFormID():0,merchant->GetFormID(),session.items.size()-generated,generated,session.day,pricingFailures);
}
int Begin(RE::StaticFunctionTag*,RE::Actor* server,RE::BGSListForm* list,std::vector<int>,int) {
    if(!ready||!inn::service::BackendReady()||!window||!server||!list)return 0;
    auto player=RE::PlayerCharacter::GetSingleton();auto location=inn::FindInn(player);
    if(!inn::IsSeated(player)||!player->GetParentCell()||!location)return 0;
    inn::LoadMenuLayoutSettings();
    std::lock_guard lock(gate);
    if(session.phase==food::Phase::open||session.phase==food::Phase::selected||session.phase==food::Phase::committing)return 0;
    RetireImages();session={};session.token=inn::tables::NextOrder();session.phase=food::Phase::open;
    session.seller=server->GetHandle();session.inn=location->GetFormID();session.sellerName=location->GetName();
    if(auto data=RE::TESDataHandler::GetSingleton())
        session.hideEmptySpecialty=location==data->LookupForm<RE::BGSLocation>(0x0209F0,"Skyrim.esm");
    for(int depth=0;location&&depth<12;++depth,location=location->parentLoc)
        if(auto title=venueTitleArtwork.find(location->GetFormID());title!=venueTitleArtwork.end()) {
            session.titleArt=title->second;break;
        }
    session.cell=player->GetParentCell()->GetFormID();session.seat=player->GetOccupiedFurniture();
    session.status=T("reading_stock");window->IsOpen=true;
    const auto token=session.token;
    const auto seller=session.seller;
    const auto generation=inn::service::Generation();
    SKSE::GetTaskInterface()->AddTask([token,seller,generation]{
        if(generation!=inn::service::Generation()||!inn::service::BackendReady())return;
        {std::lock_guard check(gate);if(session.token!=token||session.phase!=food::Phase::open)return;}
        auto ref=seller.get();inn::service::OrderStarted(ref?ref->As<RE::Actor>():nullptr,token);Snapshot(token);
    });return token;
}
int State(RE::StaticFunctionTag*,int token) {
    std::lock_guard lock(gate);
    if(!token)return 0;
    // A Papyrus serving stack can resume after loading between payment and its
    // final acknowledgement. Preserve its paid result instead of discarding paid food.
    if(token!=session.token)return inn::tables::RecoveredStatus(token);
    if(session.phase==food::Phase::open&&window&&!window->IsOpen)CancelUnlocked();
    if(session.phase==food::Phase::open&&!validationQueued.exchange(true)) {
        SKSE::GetTaskInterface()->AddTask([token]{
            std::lock_guard stateLock(gate);
            if(token==session.token&&session.phase==food::Phase::open&&!SameContext())CancelUnlocked();
            validationQueued=false;
        });
    }
    return static_cast<int>(session.phase);
}
std::vector<RE::TESForm*> Selection(RE::StaticFunctionTag*,int token) {
    std::lock_guard lock(gate);std::vector<RE::TESForm*> result;
    if(token==session.token&&session.phase==food::Phase::selected)
        for(auto id:session.selected)result.push_back(id?RE::TESForm::LookupByID(inn::BaseForm(id)):nullptr);
    return result;
}
void Cancel(RE::StaticFunctionTag*,int token) {
    std::lock_guard lock(gate);
    if(token==session.token&&session.phase!=food::Phase::committed) {
        CancelUnlocked();SKSE::GetTaskInterface()->AddTask([token]{inn::tables::Cancel(token);});
    }
}
void CommitOnGameThread(int token) {
    std::lock_guard lock(gate);
    if(token!=session.token||session.phase!=food::Phase::committing)return;
    auto fail=[&](const char* reason){
        auto p=RE::PlayerCharacter::GetSingleton();auto s=session.seller.get();
        auto seat=p?p->GetOccupiedFurniture().get():RE::NiPointer<RE::TESObjectREFR>{};
        auto quotedSeat=session.seat.get();
        auto location=p?inn::FindInn(p):nullptr;
        logger::warn("Commit rejected order={} reason={} inn={:08X} quoted_cell={:08X} current_cell={:08X} quoted_seat={:08X} current_seat={:08X} current_inn={:08X} seated={} combat={} server={:08X} server_cell={:08X} quoted_price={} selected={:08X},{:08X},{:08X}",token,reason,session.inn,session.cell,p&&p->GetParentCell()?p->GetParentCell()->GetFormID():0,quotedSeat?quotedSeat->GetFormID():0,seat?seat->GetFormID():0,location?location->GetFormID():0,p&&inn::IsSeated(p),p&&p->IsInCombat(),s?s->GetFormID():0,s&&s->GetParentCell()?s->GetParentCell()->GetFormID():0,session.price,inn::BaseForm(session.selected[0]),inn::BaseForm(session.selected[1]),inn::BaseForm(session.selected[2]));
        CancelUnlocked();inn::tables::Cancel(token);RE::SendHUDMessage::ShowHUDMessage(T(std::string_view(reason)=="insufficient_gold"?"gold_missing":"stock_changed"));
    };
    auto player=RE::PlayerCharacter::GetSingleton();auto server=session.seller.get();auto merchant=session.merchant.get();
    auto gold=RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    if(!player||!server||!merchant||!gold){fail("missing_participant_or_gold_form");return;}
    if(!SameContext()){fail("context_changed");return;}
    if(session.dailyMode&&session.day!=inn::daily::Today()){fail("daily_day_changed");return;}
    std::map<inn::Id,inn::Offer> offers;
    for(const auto& row:session.items) {
        const auto key=inn::RowKey(row);offers[key]={row.count,row.price,row.mask,row.generated};
        if(std::find(session.selected.begin(),session.selected.end(),key)!=session.selected.end()) {
            auto object=RE::TESForm::LookupByID<RE::TESBoundObject>(row.id);
            const auto currentPrice=inn::BuyCost(object,merchant->As<RE::Actor>());
            if(currentPrice!=row.price){logger::warn("Commit price changed order={} base={:08X} quoted={} current={}",token,row.id,row.price,currentPrice);CancelUnlocked();inn::tables::Cancel(token);RE::SendHUDMessage::ShowHUDMessage(T("price_changed"));return;}
        }
    }
    auto quote=inn::MenuPrice(session.selected,offers,session.dailyMode,session.daily);
    if(!quote.valid){fail("quoted_selection_invalid");return;}
    const auto currentSupplier=inn::suppliers::Resolve(server->As<RE::Actor>());
    if(!inn::suppliers::SameSupplier(session.supplier,currentSupplier.id,!quote.inventoryQuantities.empty())){
        logger::warn("Commit supplier mismatch order={} merchant={:08X}/{:08X} faction={:08X}/{:08X} stock={:08X}/{:08X}",token,session.supplier.merchant,currentSupplier.id.merchant,session.supplier.faction,currentSupplier.id.faction,session.supplier.stock,currentSupplier.id.stock);
        fail("supplier_changed");return;
    }
    RE::TESObjectREFR* stock=nullptr;
    // Generated-only orders never resolve, read or mutate the merchant food chest.
    if(!quote.inventoryQuantities.empty()) {
        auto chest=session.stock.get();stock=chest.get();
        if(!stock||currentSupplier.stock!=stock){fail("stock_reference_changed");return;}
        for(auto& [key,offer]:offers)if(!offer.generated)offer.stock=0;
        for(const auto& [object,data]:stock->GetInventory(IsFood))if(object&&offers.contains(object->GetFormID()))offers[object->GetFormID()].stock=data.first;
        const auto requested=quote.inventoryQuantities;
        quote=inn::MenuPrice(session.selected,offers,session.dailyMode,session.daily);
        if(!quote.valid) {
            for(auto [key,count]:requested)logger::warn("Commit current stock order={} base={:08X} requested={} available={}",token,inn::BaseForm(key),count,offers.at(key).stock);
            fail("live_stock_quote_invalid");return;
        }
    }
    std::vector<RE::FormID> foods;for(auto id:session.selected)if(id)foods.push_back(inn::BaseForm(id));
    bool plate=false;
    if(auto i=profiles.find(inn::BaseForm(session.selected[0]));i!=profiles.end())plate=inn::catalog.foods[i->second].plate;
    const auto availableGold=player->GetItemCount(gold);const bool sameContext=SameContext();
    if(!inn::CanPurchase(quote,availableGold,sameContext)){
        logger::warn("Commit payment context order={} quote_valid={} price={} gold={} context={}",token,quote.valid,quote.total,availableGold,sameContext);
        fail(!sameContext?"context_changed_before_purchase":availableGold<quote.total?"insufficient_gold":"purchase_quote_invalid");return;
    }
    if(quote.total!=session.price){fail("quote_total_changed");return;}
    if(!inn::tables::Prepared(token,foods,plate)){fail("servings_not_prepared");return;}
    struct Store {
        RE::TESObjectREFR* chest;RE::TESObjectREFR* payee;RE::PlayerCharacter* player;RE::TESBoundObject* gold;int token;
        RE::TESBoundObject* Object(inn::Id id){return RE::TESForm::LookupByID<RE::TESBoundObject>(inn::BaseForm(id));}
        int Stock(inn::Id id){const auto count=chest->GetInventoryCounts()[Object(id)];logger::info("Purchase stock sample order={} chest={:08X} base={:08X} count={}",token,chest->GetFormID(),inn::BaseForm(id),count);return count;}
        int Gold(){const auto count=player->GetItemCount(gold);logger::info("Purchase gold sample order={} count={}",token,count);return count;}
        void TakeStock(inn::Id id,int count){logger::info("Purchase remove stock order={} base={:08X} requested={}",token,inn::BaseForm(id),count);chest->RemoveItem(Object(id),count,RE::ITEM_REMOVE_REASON::kRemove,nullptr,nullptr);}
        void ReturnStock(inn::Id id,int count){logger::warn("Purchase restore stock order={} base={:08X} count={}",token,inn::BaseForm(id),count);chest->AddObjectToContainer(Object(id),nullptr,count,nullptr);}
        void Charge(int amount){logger::info("Purchase charge order={} amount={} payee={:08X}",token,amount,payee->GetFormID());player->RemoveItem(gold,amount,RE::ITEM_REMOVE_REASON::kRemove,nullptr,payee);}
        void Refund(int amount){logger::warn("Purchase refund order={} amount={}",token,amount);payee->RemoveItem(gold,amount,RE::ITEM_REMOVE_REASON::kRemove,nullptr,player);}
    } store{stock,stock?stock:merchant.get(),player,gold,token};
    if(!inn::Purchase(session.phase,quote,store)){fail("stock_or_payment_mutation_rejected");return;}
    inn::tables::Commit(token);
    inn::service::OrderFinished(server->As<RE::Actor>(),token,true);
    logger::info("Committed order={} inn={:08X} food_types={} vendor_food_types={} price={}",token,session.inn,quote.quantities.size(),quote.inventoryQuantities.size(),quote.total);
}
bool Commit(RE::StaticFunctionTag*,int token) {
    std::lock_guard lock(gate);
    if(token!=session.token||session.phase!=food::Phase::selected){logger::warn("Commit dispatch rejected token={} current_token={} phase={}",token,session.token,static_cast<int>(session.phase));return false;}
    if(!CurrentQuote().valid){logger::warn("Commit dispatch rejected token={} reason=quoted_selection_invalid",token);return false;}
    session.phase=food::Phase::committing;
    SKSE::GetTaskInterface()->AddTask([token]{CommitOnGameThread(token);});return true;
}
