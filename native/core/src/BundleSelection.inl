const std::array<inn::Id,3>& BundleItems(int index){return index==0?session.specialty:session.daily;}
int FirstVisibleBundle() {
    return session.hideEmptySpecialty&&std::none_of(session.specialty.begin(),session.specialty.end(),
        [](auto id){return id!=0;})?1:0;
}
std::map<inn::Id,inn::Offer> BundleOffers() {
    std::map<inn::Id,inn::Offer> offers;
    for(const auto& row:session.items)offers[inn::RowKey(row)]={row.count,row.price,row.mask,row.generated};
    return offers;
}
inn::Quote QuoteBundle(int index){return inn::BundlePrice(BundleItems(index),BundleOffers(),index==1);}
bool IsBundleSelected(int index){return inn::BundleSelected(session.selected,session.dailyMode,BundleItems(index),index==1);}
void SelectBundle(int index) {
    if(!inn::ToggleBundle(session.selected,session.dailyMode,BundleItems(index),index==1,BundleOffers()))return;
    session.tab=3;session.bundleHighlighted=index;
    session.status=T(IsBundleSelected(index)?(index==0?"specialty_selected":"daily_selected"):"choose");UpdateQuote();
}
void SelectDaily(){SelectBundle(1);}
void SelectSpecialty(){SelectBundle(0);}
std::string BundleItemName(inn::Id id) {
    const auto row=std::find_if(session.items.begin(),session.items.end(),[id](const auto& item){return inn::RowKey(item)==id;});
    return row!=session.items.end()?row->name:T("bundle_item_unavailable");
}
