#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
namespace inn::service::reservation {
struct Definition {std::uint32_t scene,quest,firstAlias,secondAlias;};
// Mandatory ActorDialogue event aliases, not the scenes' unique-actor aliases.
// WITavern's already-filled, long-running innkeeper/server aliases are excluded.
inline constexpr std::array<Definition,38> definitions{{
    {0x023E84,0x023E83,4,5},
    {0x023E8B,0x023E86,4,5},
    {0x023E8D,0x023E8C,4,5},
    {0x02604C,0x025EFE,0,1},
    {0x02604D,0x025EED,0,1},
    {0x02604E,0x025EF7,0,1},
    {0x02604F,0x025EFB,0,1},
    {0x026051,0x025F02,0,1},
    {0x026052,0x025F05,0,1},
    {0x026053,0x025F03,0,1},
    {0x026054,0x025EF3,0,1},
    {0x026065,0x025EFF,0,1},
    {0x0318C3,0x0318C2,0,1},
    {0x0318C5,0x0318C4,0,1},
    {0x031943,0x031942,0,1},
    {0x031945,0x031944,0,1},
    {0x031947,0x031946,0,1},
    {0x031949,0x031948,0,1},
    {0x03194B,0x03194A,0,1},
    {0x03194D,0x03194C,0,1},
    {0x03D21B,0x03D21A,0,1},
    {0x03D21D,0x03D21C,0,1},
    {0x0590CF,0x0590CE,0,1},
    {0x0590D1,0x0590D0,0,1},
    {0x05B298,0x05B297,0,1},
    {0x05B29A,0x05B299,0,1},
    {0x05B29C,0x05B29B,0,1},
    {0x05B29E,0x05B29D,0,1},
    {0x05B2A0,0x05B29F,0,1},
    {0x05B2A2,0x05B2A1,0,1},
    {0x06771C,0x06771B,2,3},
    {0x06771E,0x06771D,2,3},
    {0x067720,0x06771F,2,3},
    {0x067722,0x067721,2,3},
    {0x067724,0x067723,2,3},
    {0x067726,0x067725,2,3},
    {0x0695F2,0x0695F1,6,7},
    {0x0695F4,0x0695F3,6,7},
}};
// These are disk/descriptor tags. BGSRefAlias::Load replaces ALFD with the
// matching member's array index; never compare these tags to forcedEventData.
inline constexpr std::uint32_t firstActorTag=0x3152,secondActorTag=0x3252;
struct EventField {std::uint32_t tag;bool reference;};
struct ActorFields {std::uint32_t first,second;};
inline std::optional<ActorFields> ResolveActorFields(std::span<const EventField> fields){
    if(fields.empty()||fields.size()>64)return std::nullopt;
    std::optional<std::uint32_t> first,second;
    for(std::uint32_t index=0;index<fields.size();++index){
        const auto& field=fields[index];
        if(field.tag!=firstActorTag&&field.tag!=secondActorTag)continue;
        auto& slot=field.tag==firstActorTag?first:second;
        if(slot||!field.reference)return std::nullopt;
        slot=index;
    }
    if(!first||!second||*first==*second)return std::nullopt;
    return ActorFields{*first,*second};
}
inline constexpr bool CandidateShape(bool referenceAlias,bool fromEvent,bool actorDialogue,
    bool optional,bool hasConditions,std::uint32_t selector,std::uint32_t expectedSelector){
    return referenceAlias&&fromEvent&&actorDialogue&&!optional&&hasConditions&&selector==expectedSelector;
}
// The prefix must be AND, ahead of the old OR group. Never rewrite, copy or free
// an existing condition node. The receiving engine list owns the added node.
template<class Node,class Match,class Create>
bool PrefixOnce(Node*& head,Match matches,Create create){
    if(!head||matches(*head))return false;
    auto condition=create();if(!condition)return false;
    condition->next=head;head=condition;return true;
}
}
