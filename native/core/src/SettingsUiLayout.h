#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
namespace inn::settingsui {
// Shared by both settings renderers and the offline editor; defaults retain the existing UI.
#define INN_SETTINGS_UI_FIELDS(X) \
 X(controlWidth,0,0,1000,"Layout","ControlWidth","控件宽度（0 自动）","Control width (0 auto)") \
 X(fontScale,1,.6,1.5,"Layout","FontScale","页面字号比例","Page font scale") \
 X(sectionGap,0,0,48,"Layout","SectionGap","分组间距","Section gap") \
 X(labelsAbove,0,0,1,"Layout","LabelsAbove","标签放在控件上方","Labels above controls") \
 X(helpMode,1,0,2,"Layout","HelpMode","说明显示方式","Help display") \
 X(callFirst,1,0,1,"General","CallFirst","呼叫分组在前","Call section first") \
 X(callExpanded,1,0,1,"General","CallExpanded","呼叫分组默认展开","Call section expanded") \
 X(cleanupExpanded,0,0,1,"General","CleanupExpanded","清理分组默认展开","Cleanup section expanded") \
 X(showCallMode,1,0,1,"Controls","CallMode","显示呼叫方式","Show call method") \
 X(showCallHint,1,0,1,"Controls","CallHint","显示按键提示选项","Show hotkey hint option") \
 X(showPromptSource,1,0,1,"Controls","PromptSource","显示提示键来源","Show prompt key source") \
 X(showBinding,1,0,1,"Controls","Binding","显示键位录制区","Show binding controls") \
 X(showPriority,1,0,1,"Controls","Priority","显示服务优先级","Show service priority") \
 X(showAutoClear,1,0,1,"Controls","AutoClear","显示自动清桌开关","Show automatic cleanup") \
 X(showScope,1,0,1,"Controls","Scope","显示清理范围","Show cleanup scope") \
 X(dessertsFirst,0,0,1,"Serving","DessertsFirst","甜点选项在前","Dessert options first") \
 X(stockHeight,300,100,900,"Stock","ListHeight","库存列表高度","Stock list height") \
 X(stockActions,1,0,1,"Stock","ShowActions","显示服务按钮","Show service buttons") \
 X(stockStatus,1,0,1,"Stock","ShowStatus","显示服务状态","Show service status") \
 X(stockOwners,1,0,1,"Stock","ShowOwners","显示服务和库存负责人","Show staff and stock keeper")
struct Layout {
#define INN_SETTINGS_UI_MEMBER(member,value,min,max,section,key,zh,en) float member=float(value);
 INN_SETTINGS_UI_FIELDS(INN_SETTINGS_UI_MEMBER)
#undef INN_SETTINGS_UI_MEMBER
 struct Field {float Layout::* member;float value,min,max;const char* section;const char* key;};
 static constexpr auto Fields() {
#define INN_SETTINGS_UI_FIELD(member,value,min,max,section,key,zh,en) Field{&Layout::member,float(value),float(min),float(max),section,key},
  return std::array{INN_SETTINGS_UI_FIELDS(INN_SETTINGS_UI_FIELD)};
#undef INN_SETTINGS_UI_FIELD
 }
 bool Load(const std::filesystem::path& path) {
  *this=Layout{};std::ifstream input(path);if(!input)return false;
  std::string line,section;
  const auto trim=[](std::string value){const auto a=value.find_first_not_of(" \t\r\n");return a==std::string::npos?std::string{}:value.substr(a,value.find_last_not_of(" \t\r\n")-a+1);};
  while(std::getline(input,line)) {
   if(line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);
   line=trim(line.substr(0,line.find_first_of(";#")));if(line.empty())continue;
   if(line.front()=='['&&line.back()==']'){section=trim(line.substr(1,line.size()-2));continue;}
   const auto equal=line.find('=');if(equal==std::string::npos)continue;
   const auto key=trim(line.substr(0,equal)),text=trim(line.substr(equal+1));
   for(const auto& field:Fields())if(section==field.section&&key==field.key) {
    try {std::size_t used{};const auto value=std::stof(text,&used);if(used==text.size()&&std::isfinite(value))this->*field.member=std::clamp(value,field.min,field.max);}catch(...){}
    break;
   }
  }
  return true;
 }
};
inline Layout layout;
inline bool Load(){return layout.Load("Data/SKSE/Plugins/PullUpAChairOrderingSettingsUI.ini");}
}
