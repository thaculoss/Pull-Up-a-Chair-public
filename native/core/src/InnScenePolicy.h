#pragma once
#include <array>
#include <cstdint>
namespace inn::service::scenes {
// WITavern's ordinary server aliases supply these outside its scene actions.
// Never exempt every package merely because it uses the same parent quest.
inline constexpr bool AllowedAliasPackage(std::uint32_t id,std::uint32_t parent){
    return parent==0x0DEE92&&(id==0x0DEE97||id==0x0DEE98);
}
// Audited vanilla inn banter. Parent quest checks keep an override that binds a
// different storyline protected. Zero preserves the three original generic rules.
struct Definition {std::uint32_t id,parent;const char* name;};
inline constexpr std::array<Definition,41> ambient{{
    {0x023E84,0x023E83,"DialogueKynesgroveRoggiKjeldInnScene1"},
    {0x023E8B,0x023E86,"DialogueKynesgroveRoggiKjeldInnScene2"},
    {0x023E8D,0x023E8C,"DialogueKynesgroveRoggiKjeldInnScene3"},
    {0x02604C,0x025EFE,"MarkarthSilverFishInnScene14"},
    {0x02604D,0x025EED,"MarkarthSilverFishInnScene01"},
    {0x02604E,0x025EF7,"MarkarthSilverFishInnScene02"},
    {0x02604F,0x025EFB,"MarkarthSilverFishInnScene03"},
    {0x026051,0x025F02,"MarkarthSilverFishInnScene04"},
    {0x026052,0x025F05,"MarkarthSilverFishInnScene12"},
    {0x026053,0x025F03,"MarkarthSilverFishInnScene05"},
    {0x026054,0x025EF3,"MarkarthSilverFishInnScene06"},
    {0x026065,0x025EFF,"MarkarthSilverFishhInnScene13"},
    {0x0318C3,0x0318C2,"WinterholdInnScene01"},
    {0x0318C5,0x0318C4,"WinterholdInnScene02"},
    {0x031943,0x031942,"WinterholdInnScene03"},
    {0x031945,0x031944,"WinterholdInnScene04"},
    {0x031947,0x031946,"WinterholdInnScene05"},
    {0x031949,0x031948,"WinterholdInnScene06"},
    {0x03194B,0x03194A,"WinterholdInnScene07"},
    {0x03194D,0x03194C,"WinterholdInnScene08"},
    {0x03D21B,0x03D21A,"WindpeakInnScene02"},
    {0x03D21D,0x03D21C,"WindpeakInnScene03"},
    {0x0590CF,0x0590CE,"DialogueMarkarthSilverFishInnScene16View"},
    {0x0590D1,0x0590D0,"DialogueMarkarthSilverFishInnScene17View"},
    {0x05B298,0x05B297,"DialogueDawnstarWindpeakInnScene04View"},
    {0x05B29A,0x05B299,"DialogueDawnstarWindpeakInnScene05View"},
    {0x05B29C,0x05B29B,"DialogueDawnstarWindpeakInnScene06View"},
    {0x05B29E,0x05B29D,"DialogueDawnstarWindpeakInnScene07View"},
    {0x05B2A0,0x05B29F,"DialogueDawnstarWindpeakInnScene08View"},
    {0x05B2A2,0x05B2A1,"DialogueDawnstarWindpeakInnScene09View"},
    {0x06771C,0x06771B,"DialogueIvarsteadInnScene01SCN"},
    {0x06771E,0x06771D,"DialogueIvarsteadInnScene02SCN"},
    {0x067720,0x06771F,"DialogueIvarsteadInnScene03SCN"},
    {0x067722,0x067721,"DialogueIvarsteadInnScene04SCN"},
    {0x067724,0x067723,"DialogueIvarsteadInnScene05SCN"},
    {0x067726,0x067725,"DialogueIvarsteadInnScene06SCN"},
    {0x0695F2,0x0695F1,"DialogueOldHroldanHangedManInnScene05View"},
    {0x0695F4,0x0695F3,"DialogueOldHroldanHangedManInnScene06View"},
    {0x0DEE96,0x000000,"WITavernGreeting"},
    {0x0DEE9C,0x000000,"WITavernSceneAScene"},
    {0x0DEE9E,0x000000,"WITavernPlayerSits"},
}};
inline constexpr bool Allowed(std::uint32_t id,std::uint32_t parent){
    for(const auto& row:ambient)if(row.id==id)return !row.parent||row.parent==parent;
    return false;
}
inline constexpr const char* Name(std::uint32_t id){
    for(const auto& row:ambient)if(row.id==id)return row.name;
    return "protected_scene";
}
}
