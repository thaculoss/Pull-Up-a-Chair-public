#include "SupplierPolicy.h"
#include <array>
#include <iostream>
#include <stdexcept>
int checks{};
void Check(bool value,const char* error){++checks;if(!value)throw std::runtime_error(error);}
int main(){
    try{
        using namespace inn::suppliers;
        Check(HasAuthority(true,101,101,false),"Alias successor wrongly requires old vendor faction membership");
        Check(!HasAuthority(true,101,100,true),"Non-owner faction member bypassed the inn role");
        Check(!HasAuthority(true,0,100,true),"Missing role silently accepted a faction member");
        Check(HasAuthority(false,0,100,true),"Canonical owner without a quest role rejected");
        Check(!HasAuthority(false,0,100,false),"Unrelated merchant accepted by faction fallback");
        Check(BelongsToInn(10,10,0),"Durable owner home without a chest location rejected");
        Check(BelongsToInn(10,0,10),"Durable chest location rejected");
        Check(!BelongsToInn(10,10,11),"Old cached owner accepted a chest now belonging to another inn");
        Check(!BelongsToInn(10,11,0),"Visiting owner associated with the wrong inn");
        Check(!BelongsToInn(10,0,0),"Provider without durable association accepted");
        std::array<Candidate,3> rows{{{{10,100,200,300,0},true,true,true,true,true},
                                    {{10,101,200,300,10},true,true,true,false,true},
                                    {{11,102,201,301,0},true,true,true,true,true}}};
        Check(Select(10,rows)->id.merchant==100,"Primary inn owner not selected");
        Check(Select(11,rows)->id.stock==301,"Another inn borrowed the first chest");
        Check(!Select(12,rows),"Unregistered inn borrowed a nearby merchant");
        // Server identity, distance, current cell and loaded 3D are intentionally
        // absent: changing any of these must not select another provider.
        const auto original=Select(10,rows)->id;
        Check(SameSupplier(original,Select(10,rows)->id,true),"Remote owner changed supplier");
        rows[0].alive=false;
        Check(!Select(10,rows),"Unqualified replacement used another business");
        rows[1].member=true; // The inn's main role alias now designates this owner.
        Check(Select(10,rows)->id.merchant==101&&Select(10,rows)->id.stock==300,"Replacement did not inherit inn chest");
        Check(!SameSupplier(original,Select(10,rows)->id,true),"Changed price identity accepted during payment");
        rows[1].innkeeper=false;Check(!Select(10,rows),"Actor without the innkeeper role accepted");
        rows[1].innkeeper=true;rows[1].enabled=false;Check(!Select(10,rows),"Disabled innkeeper accepted");
        rows[1].enabled=true;rows[1].hasStock=false;rows[1].id.stock=0;
        const auto specialties=Select(10,rows)->id;
        Check(specialties.merchant==101,"Chestless authored specials lost stable pricing owner");
        Check(SameSupplier(specialties,specialties,false),"Generated order incorrectly requires vendor food stock");
        Check(!SameSupplier(specialties,specialties,true),"Vendor food order accepted absent chest");
        rows[1].hasStock=true;rows[1].id.stock=333;
        Check(!SameSupplier(original,rows[1].id,true),"Changed faction chest accepted during payment");
        auto old=rows[1].id;auto changed=old;changed.stock=334;
        Check(!SameSupplier(old,changed,true),"Redirected stock not revalidated");
        Check(SameSupplier(old,changed,false),"Generated-only payment wrongly depends on a food chest");
        changed=old;changed.faction=777;Check(!SameSupplier(old,changed,false),"Different vendor faction accepted");
        changed=old;changed.inn=11;Check(!SameSupplier(old,changed,false),"Changed inn accepted");
        rows[0]={{10,100,200,300,100},true,true,true,true,true};
        rows[1].id.priority=0;Check(Select(10,rows)->id.merchant==101,"Learned owner overrides registered role");
        rows[0].id.priority=0;Check(Select(10,rows)->id.merchant==100,"Tie-breaking depends on traversal order");
        std::swap(rows[0],rows[1]);Check(Select(10,rows)->id.merchant==100,"Different server scan order changed quote owner");
        std::cout<<checks<<" stable supplier checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
