#pragma once
#include "SettingsUiLayout.h"
#include "SKSEMenuFramework.h"
#include "Localization.h"
namespace inn::settingsui {
inline bool Available() {
 const auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
 for(const auto name:{"igPushItemWidth","igPopItemWidth","igDummy","igGetContentRegionAvail","igSetWindowFontScale","igIsItemHovered","igBeginTooltip","igEndTooltip","igPushTextWrapPos","igPopTextWrapPos","igGetCursorPosX","igGetFontSize","igTextUnformatted"})
  if(!module||!GetProcAddress(module,name))return false;
 return true;
}
inline void Begin(){if(layout.fontScale!=1)ImGuiMCP::SetWindowFontScale(layout.fontScale);}
inline void End(){if(layout.fontScale!=1)ImGuiMCP::SetWindowFontScale(1);}
inline void Gap(){if(layout.sectionGap>0)ImGuiMCP::Dummy({0,layout.sectionGap});}
inline void Hint(const char* text) {
 if(layout.helpMode<1||layout.helpMode>=2||!ImGuiMCP::IsItemHovered())return;
 ImGuiMCP::BeginTooltip();
 ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetCursorPosX()+std::min(640.f,ImGuiMCP::GetFontSize()*28.f));
 ImGuiMCP::TextUnformatted(text);ImGuiMCP::PopTextWrapPos();ImGuiMCP::EndTooltip();
}
inline const char* ComboHint(std::string_view id,int value) {
 if(id=="innLanguage")return T("general_language_help");
 if(id=="callMode")return T(value==0?"call_hotkey_help":value==1?"call_framework_help":"call_skyprompt_help");
 if(id=="callHint")return T("call_hint_help");
 if(id=="promptSource")return T("call_skyprompt_help");
 if(id=="priority")return T("call_priority_help");
 if(id=="autoClear")return T("call_autoclear_help");
 if(id=="scope")return T("cleanup_scope_help");
 if(id=="compactMain"||id=="compactDesserts"||id=="mainTarget"||id=="dessertTarget")return T("serving_size_help");
 return nullptr;
}
inline bool Combo(const char* id,const char* label,int* value,const char* const items[],int count) {
 if(layout.labelsAbove>.5f)ImGuiMCP::TextWrapped("%s",label);
 const auto name=(layout.labelsAbove>.5f?std::string{}:std::string(label))+"###"+id;
 if(layout.controlWidth>0)ImGuiMCP::PushItemWidth(std::min(layout.controlWidth,ImGuiMCP::GetContentRegionAvail().x));
 const bool changed=ImGuiMCP::Combo(name.c_str(),value,items,count);
 if(layout.controlWidth>0)ImGuiMCP::PopItemWidth();
 if(const auto help=ComboHint(id,*value))Hint(help);
 return changed;
}
inline void Help(const char* id,const char* text) {
 if(layout.helpMode>=2)return;
 if(layout.helpMode<1){ImGuiMCP::TextWrapped("%s",text);return;}
 // Compact mode attaches help to its control or section, consuming no extra rows.
 (void)id;
}
}
