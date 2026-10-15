#include "ServiceSceneReservationPolicy.h"
#include "InnScenePolicy.h"
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
using namespace inn::service;
namespace {
unsigned checks{};
void Check(bool value,const char* description){++checks;if(!value){std::cerr<<description<<'\n';std::exit(1);}}
struct Node {Node* next{};bool value{},orNext{},reservation{};};
bool Evaluate(Node* head){
    bool group=false;
    for(auto node=head;node;node=node->next){
        group=group||node->value;
        if(!node->orNext||!node->next){if(!group)return false;group=false;}
    }
    return true;
}
}
int main(){
    using namespace reservation;
    std::set<std::uint32_t> quests,scenesSeen;
    Check(definitions.size()==38,"Reviewed vanilla scene coverage changed");
    for(const auto& row:definitions){
        Check(row.quest&&row.quest!=0x0DEE92,"Long-running WITavern aliases must remain unchanged");
        Check(scenes::Allowed(row.scene,row.quest),"Reservation must match an approved scene and parent");
        Check(row.firstAlias!=row.secondAlias,"Event candidate IDs must differ");
        Check(quests.insert(row.quest).second&&scenesSeen.insert(row.scene).second,"Duplicated quest/scene reservation");
    }
    // Independently decoded from SkyrimSE 1.5.97: ADIA registers Location L1,
    // Reference R1, Reference R2 in that order (see release evidence).
    const std::array<EventField,3> se1597{{{0x314C,false},{firstActorTag,true},{secondActorTag,true}}};
    const auto seFields=ResolveActorFields(se1597);
    Check(seFields&&seFields->first==1&&seFields->second==2,"Actual SE 1.5.97 ADIA descriptor fixture failed");
    Check(CandidateShape(true,true,true,false,true,1,seFields->first)&&CandidateShape(true,true,true,false,true,2,seFields->second),"Actual loaded SE 1.5.97 actor fields rejected");
    // Load converts disk tags to positions in the registered event's member
    // array. Exercise moved fields too: the resolver must not hardcode 0/1.
    for(std::uint32_t first=0;first<8;++first)for(std::uint32_t second=0;second<8;++second){
        if(first==second)continue;
        std::vector<EventField> members(8,{0,false});
        members[first]={firstActorTag,true};members[second]={secondActorTag,true};
        const auto fields=ResolveActorFields(members);
        Check(fields&&fields->first==first&&fields->second==second,"Engine member positions were confused with disk tags");
        Check(CandidateShape(true,true,true,false,true,first,fields->first),"Loaded first actor index rejected");
        Check(CandidateShape(true,true,true,false,true,second,fields->second),"Loaded second actor index rejected");
        Check(!CandidateShape(true,true,true,false,true,firstActorTag,fields->first),"Old raw R1 tag passed loaded index check");
        Check(!CandidateShape(true,true,true,false,true,secondActorTag,fields->second),"Old raw R2 tag passed loaded index check");
        Check(!CandidateShape(true,true,true,false,true,first,fields->second),"Swapped actor fields passed");
        members[first].reference=false;
        Check(!ResolveActorFields(members),"Non-reference R1 descriptor accepted");
    }
    const std::array<EventField,2> ordinary{{{firstActorTag,true},{secondActorTag,true}}};
    Check(ResolveActorFields(ordinary).has_value(),"Two registered actor fields rejected");
    Check(!ResolveActorFields({}),"Empty registry accepted");
    const std::array<EventField,3> duplicate{{{firstActorTag,true},{secondActorTag,true},{firstActorTag,true}}};
    Check(!ResolveActorFields(duplicate),"Duplicated actor tag accepted");
    const std::array<EventField,1> incomplete{{{firstActorTag,true}}};
    Check(!ResolveActorFields(incomplete),"Missing R2 descriptor accepted");
    std::vector<EventField> oversized(65,{0,false});oversized[0]=ordinary[0];oversized[1]=ordinary[1];
    Check(!ResolveActorFields(oversized),"Unbounded registry accepted");
    for(unsigned state=0;state<64;++state){
        const bool ref=state&1,event=state&2,adia=state&4,optional=state&8,conditions=state&16,selector=state&32;
        Check(CandidateShape(ref,event,adia,optional,conditions,selector?0U:1U,0U)==
            (ref&&event&&adia&&!optional&&conditions&&selector),"Unexpected candidate shape accepted");
    }
    // Regression: placing the reservation inside the old GetIsID OR group lets
    // a reserved matching NPC through. Prefix must preserve both original nodes
    // and AND the reservation against the complete, unmodified OR group.
    for(unsigned state=0;state<8;++state){
        Node second{nullptr,(state&2)!=0,true,false};
        Node first{&second,(state&1)!=0,true,false};
        Node gate{nullptr,(state&4)==0,false,true};
        Node* head=&first;unsigned allocations=0;
        auto match=[](const Node& node){return node.reservation&&!node.orNext;};
        auto create=[&](){++allocations;return &gate;};
        Check(PrefixOnce(head,match,create),"Initial reservation was not added");
        Check(head==&gate&&gate.next==&first&&first.next==&second&&second.next==nullptr,"Original condition links changed");
        Check(first.orNext&&second.orNext,"Original OR flags changed");
        Check(Evaluate(head)==((state&4)==0&&((state&1)||(state&2))),"Reservation bypassed by existing OR group");
        Check(!PrefixOnce(head,match,create)&&allocations==1,"Repeated initialization duplicated reservation");
    }
    Node* absent=nullptr;unsigned allocations=0;
    Check(!PrefixOnce(absent,[](const Node&){return false;},[&]()->Node*{++allocations;return nullptr;})&&allocations==0,"Empty unreviewed condition list was modified");
    std::cout<<checks<<" scene reservation checks passed\n";
}
