#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
namespace inn {
// One schema supplies defaults, validation bounds, the INI template and editor labels.
// Distances and font sizes are measured at the menu's reference scale.
#define INN_UI_FIELDS(X) \
 X(windowWidth,1280,800,1800,"Window","Width","菜单宽度") \
 X(windowHeight,836,500,1400,"Window","Height","菜单高度") \
 X(scalePercent,100,50,140,"Window","ScalePercent","整体缩放 (%)") \
 X(offsetX,0,-800,800,"Window","OffsetX","整个菜单左右偏移") \
 X(offsetY,0,-400,400,"Window","OffsetY","整个菜单上下偏移") \
 X(letterboxPercent,-1,-1,40,"Window","LetterboxPercent","上下黑边比例 (-1 自动)") \
 X(paddingX,48,4,80,"Spacing","PaddingX","菜单水平内边距") \
 X(paddingY,28,4,80,"Spacing","PaddingY","菜单垂直内边距") \
 X(itemGapX,12,0,40,"Spacing","ItemGapX","控件水平间距") \
 X(itemGapY,6,0,30,"Spacing","ItemGapY","控件垂直间距") \
 X(innFont,46,12,64,"Fonts","InnName","酒馆名字号") \
 X(totalFont,34,10,48,"Fonts","Total","合计金额字号") \
 X(statusFont,24,10,36,"Fonts","Status","顶部提示字号") \
 X(tabFont,32,10,40,"Fonts","Tabs","分类字号") \
 X(dishFont,38,12,52,"Fonts","DishName","菜名字号") \
 X(bundleTitleFont,38,12,48,"Fonts","BundleTitle","套餐名称字号") \
 X(bundleItemsFont,36,10,36,"Fonts","BundleItems","套餐内含物品字号") \
 X(priceFont,32,10,36,"Fonts","DishPrice","菜品价格字号") \
 X(cardStatusFont,26,10,36,"Fonts","CardStatus","卡片选择提示字号") \
 X(tooltipFont,34,10,48,"Fonts","Tooltip","浮动提示字号") \
 X(summaryLabelFont,26,10,36,"Fonts","SummaryLabel","底部分类标签字号") \
 X(summaryNameFont,30,10,40,"Fonts","SummaryName","底部已选菜名字号") \
 X(buttonFont,30,10,40,"Fonts","Actions","确认取消按钮字号") \
 X(controlsFont,28,10,32,"Fonts","Controls","操作说明字号") \
 X(innX,0,-400,400,"Header","InnX","酒馆名左右偏移") \
 X(innY,0,-100,100,"Header","InnY","酒馆名上下偏移") \
 X(innSignSize,120,48,220,"Header","SignSize","旅馆招牌图片尺寸") \
 X(totalX,0,-400,400,"Header","TotalX","合计金额左右偏移") \
 X(totalY,0,-100,100,"Header","TotalY","合计金额上下偏移") \
 X(statusX,0,-200,200,"Header","StatusX","顶部提示左右偏移") \
 X(statusY,0,-100,100,"Header","StatusY","顶部提示上下偏移") \
 X(tabsX,0,-400,400,"Sections","TabsX","分类栏左右偏移") \
 X(tabsY,0,-150,150,"Sections","TabsY","分类栏上下偏移") \
 X(gridX,0,-400,400,"Sections","GridX","菜品卡片区左右偏移") \
 X(gridY,0,-200,200,"Sections","GridY","菜品卡片区上下偏移") \
 X(summaryX,0,-400,400,"Sections","SummaryX","已选菜品区左右偏移") \
 X(summaryY,0,-150,150,"Sections","SummaryY","已选菜品区上下偏移") \
 X(actionsX,0,-400,400,"Sections","ActionsX","确认取消区左右偏移") \
 X(actionsY,0,-150,150,"Sections","ActionsY","确认取消区上下偏移") \
 X(controlsX,0,-400,400,"Sections","ControlsX","操作说明左右偏移") \
 X(controlsY,0,-100,100,"Sections","ControlsY","操作说明上下偏移") \
 X(tabWidth,240,80,240,"Sizes","TabWidth","分类按钮宽度") \
 X(tabHeight,44,24,72,"Sizes","TabHeight","分类按钮高度") \
 X(tabGap,12,0,48,"Sizes","TabGap","分类按钮间距") \
 X(gridHeight,406,180,800,"Sizes","GridHeight","菜品卡片区最大高度") \
 X(footerReserve,244,100,400,"Sizes","FooterReserve","底部区域预留高度") \
 X(cardGap,40,0,50,"Sizes","CardGap","两列卡片间距") \
 X(bundleHeight,384,200,700,"Sizes","BundleHeight","套餐卡片高度") \
 X(bundleItemsY,132,40,300,"Card","BundleItemsY","套餐内含物品距顶部") \
 X(cardHeight,200,120,360,"Sizes","CardHeight","单张卡片高度") \
 X(cardPadding,14,0,30,"Sizes","CardPadding","卡片内边距") \
 X(imageSize,128,80,300,"Card","ImageSize","菜品图片尺寸") \
 X(imageX,0,-80,200,"Card","ImageX","图片左右偏移") \
 X(imageY,0,-80,100,"Card","ImageY","图片上下偏移") \
 X(textGap,24,0,80,"Card","TextGap","图片与文字间距") \
 X(textX,0,-100,200,"Card","TextX","卡片文字整体左右偏移") \
 X(nameY,8,0,180,"Card","NameY","菜名距卡片顶部") \
 X(priceY,78,0,260,"Card","PriceY","价格距卡片顶部") \
 X(cardStatusY,114,0,300,"Card","StatusY","选择提示距卡片顶部") \
 X(summaryHeight,96,40,150,"Sizes","SummaryHeight","已选菜品区域高度") \
 X(confirmWidth,260,180,700,"Sizes","ConfirmWidth","确认按钮宽度") \
 X(cancelWidth,150,80,320,"Sizes","CancelWidth","取消按钮宽度") \
 X(actionHeight,58,24,80,"Sizes","ActionHeight","确认取消按钮高度")
struct UiLayout {
#define INN_UI_MEMBER(member,value,min,max,section,key,label) float member=float(value);
 INN_UI_FIELDS(INN_UI_MEMBER)
#undef INN_UI_MEMBER
 struct Field {float UiLayout::* member;float value,min,max;const char* section;const char* key;};
 static constexpr auto Fields() {
#define INN_UI_SPEC(member,value,min,max,section,key,label) Field{&UiLayout::member,float(value),float(min),float(max),section,key},
  return std::array{INN_UI_FIELDS(INN_UI_SPEC)};
#undef INN_UI_SPEC
 }
 struct Result {bool loaded{};int ignored{},clamped{};};
 static std::string Trim(std::string text) {
  const auto first=text.find_first_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
 }
 Result Load(const std::filesystem::path& path) {
  *this=UiLayout{};Result result;std::ifstream input(path);if(!input)return result;
  result.loaded=true;std::string line,section;
  while(std::getline(input,line)) {
   if(line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);
   line=Trim(line.substr(0,line.find_first_of(";#")));if(line.empty())continue;
   if(line.front()=='['&&line.back()==']'){section=Trim(line.substr(1,line.size()-2));continue;}
   const auto equal=line.find('=');if(equal==std::string::npos){++result.ignored;continue;}
   const auto key=Trim(line.substr(0,equal)),text=Trim(line.substr(equal+1));bool found=false;
   for(const auto& field:Fields())if(section==field.section&&key==field.key) {
    found=true;try {
     std::size_t used{};const auto value=std::stof(text,&used);
     if(used!=text.size()||!std::isfinite(value)){++result.ignored;break;}
     const auto bounded=std::clamp(value,field.min,field.max);
     this->*field.member=bounded;if(bounded!=value)++result.clamped;
    }catch(...){++result.ignored;}
    break;
   }
   if(!found)++result.ignored;
  }
  return result;
 }
};
inline UiLayout uiLayout;
}
