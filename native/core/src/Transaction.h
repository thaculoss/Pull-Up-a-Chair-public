#pragma once
#include "OrderPolicy.h"
#include "Policy.h"
#include <algorithm>
namespace inn {
// Store mutators run synchronously on the game thread; the caller holds the order lock.
template<class Store> bool Purchase(food::Phase& phase,const Quote& quote,Store& store) {
    if(phase!=food::Phase::committing||!quote.valid||store.Gold()<quote.total)return false;
    for(auto [id,count]:quote.inventoryQuantities)if(store.Stock(id)<count){phase=food::Phase::cancelled;return false;}
    std::map<Id,int> removed;
    auto restore=[&] {
        for(auto [id,count]:removed)if(count>0)store.ReturnStock(id,count);
        phase=food::Phase::cancelled;
    };
    for(auto [id,count]:quote.inventoryQuantities) {
        const auto before=store.Stock(id);
        store.TakeStock(id,count);
        const auto delta=before-store.Stock(id);
        removed[id]=std::clamp(delta,0,count);
        if(delta!=count){restore();return false;}
    }
    const auto gold=store.Gold();
    if(quote.total)store.Charge(quote.total);
    const auto paid=gold-store.Gold();
    if(paid!=quote.total) {
        if(paid>0)store.Refund(std::min(paid,quote.total));
        restore();return false;
    }
    phase=food::Phase::committed;
    return true;
}
}
