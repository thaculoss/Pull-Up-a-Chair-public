// Bilingual parchment menu shared by the native plugin and offline UI renderer.
// All colors and spacing are scoped to Draw().
const ImVec4 menuInkColor{0.115f,0.075f,0.035f,1.0f};
struct ParchmentStyle {
    int colors{},variables{};
    explicit ParchmentStyle(float scale) {
        auto color=[&](ImGuiCol index,ImVec4 value){ImGui::PushStyleColor(index,value);++colors;};
        auto number=[&](ImGuiStyleVar index,float value){ImGui::PushStyleVar(index,value);++variables;};
        auto vector=[&](ImGuiStyleVar index,ImVec2 value){ImGui::PushStyleVar(index,value);++variables;};
        color(ImGuiCol_Text,menuInkColor);
        color(ImGuiCol_TextDisabled,menuInkColor);
        color(ImGuiCol_WindowBg,{0.88f,0.81f,0.65f,1.0f});
        color(ImGuiCol_ChildBg,{0,0,0,0});
        color(ImGuiCol_PopupBg,{0.91f,0.85f,0.71f,1.0f});
        color(ImGuiCol_Border,{0.35f,0.23f,0.12f,0.48f});
        color(ImGuiCol_BorderShadow,{0,0,0,0});
        color(ImGuiCol_FrameBg,{0.35f,0.23f,0.12f,0.07f});
        color(ImGuiCol_FrameBgHovered,{0.35f,0.23f,0.12f,0.14f});
        color(ImGuiCol_FrameBgActive,{0.35f,0.23f,0.12f,0.20f});
        color(ImGuiCol_Button,{0.35f,0.23f,0.12f,0.04f});
        color(ImGuiCol_ButtonHovered,{0.35f,0.23f,0.12f,0.17f});
        color(ImGuiCol_ButtonActive,{0.35f,0.23f,0.12f,0.28f});
        color(ImGuiCol_Header,{0.35f,0.23f,0.12f,0.12f});
        color(ImGuiCol_HeaderHovered,{0.35f,0.23f,0.12f,0.20f});
        color(ImGuiCol_HeaderActive,{0.35f,0.23f,0.12f,0.28f});
        color(ImGuiCol_CheckMark,{0.28f,0.16f,0.07f,1.0f});
        color(ImGuiCol_Separator,{0.35f,0.23f,0.12f,0.32f});
        color(ImGuiCol_SeparatorHovered,{0.35f,0.23f,0.12f,0.45f});
        color(ImGuiCol_SeparatorActive,{0.35f,0.23f,0.12f,0.60f});
        color(ImGuiCol_ScrollbarBg,{0.35f,0.23f,0.12f,0.06f});
        color(ImGuiCol_ScrollbarGrab,{0.35f,0.23f,0.12f,0.32f});
        color(ImGuiCol_ScrollbarGrabHovered,{0.35f,0.23f,0.12f,0.47f});
        color(ImGuiCol_ScrollbarGrabActive,{0.35f,0.23f,0.12f,0.60f});
        color(ImGuiCol_TextSelectedBg,{0.48f,0.30f,0.12f,0.25f});
        color(ImGuiCol_NavHighlight,{0.35f,0.20f,0.075f,0.90f});
        number(ImGuiStyleVar_WindowRounding,0);
        number(ImGuiStyleVar_WindowBorderSize,0);
        number(ImGuiStyleVar_ChildRounding,2*scale);
        number(ImGuiStyleVar_ChildBorderSize,0);
        number(ImGuiStyleVar_FrameRounding,0);
        number(ImGuiStyleVar_FrameBorderSize,1*scale);
        number(ImGuiStyleVar_ScrollbarSize,13*scale);
        number(ImGuiStyleVar_DisabledAlpha,0.60f);
        vector(ImGuiStyleVar_WindowPadding,{inn::uiLayout.paddingX*scale,inn::uiLayout.paddingY*scale});
        vector(ImGuiStyleVar_FramePadding,{8*scale,5*scale});
        vector(ImGuiStyleVar_ItemSpacing,{inn::uiLayout.itemGapX*scale,inn::uiLayout.itemGapY*scale});
        vector(ImGuiStyleVar_ItemInnerSpacing,{6*scale,5*scale});
    }
    ~ParchmentStyle(){ImGui::PopStyleVar(variables);ImGui::PopStyleColor(colors);}
    ParchmentStyle(const ParchmentStyle&)=delete;
    ParchmentStyle& operator=(const ParchmentStyle&)=delete;
};

void MenuFont(float pixels,float scale) {
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetWindowFontScale(pixels*scale/ImGui::GetFontSize());
}
void MenuCentered(const std::string& text,float pixels,float scale,float offsetX=0,float offsetY=0) {
    MenuFont(pixels,scale);
    auto width=ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SetCursorPosX(std::max(inn::uiLayout.paddingX*scale,(ImGui::GetWindowSize().x-width)*0.5f)+offsetX*scale);
    auto pos=ImGui::GetCursorPos();pos.y+=offsetY*scale;ImGui::SetCursorPos(pos);
    ImGui::TextUnformatted(text.c_str());
    pos=ImGui::GetCursorPos();pos.y-=offsetY*scale;ImGui::SetCursorPos(pos);
}
float MenuTitleHeight() {
    if(session.titleArt.path.empty())return 0;
    if(!session.titleArtAttempted) {
        session.titleArtAttempted=true;
        session.titleArtTexture=SKSEMenuFramework::LoadTexture(session.titleArt.path);
        if(session.titleArtTexture)logger::info("Inn title artwork loaded; replacing this location's text heading");
        else logger::warn("Inn title artwork unavailable; using localized text heading");
    }
    if(!session.titleArtTexture)return 0;
    const auto& layout=inn::uiLayout;
    const float titleScale=inn::strings.IsChinese()?1.75f:2.0f;
    const float imageScale=inn::InnTitleDisplayScale(session.titleArt,inn::strings.IsChinese());
    const float reservedWidth=inn::strings.IsChinese()?2*(layout.innSignSize+24.f):layout.innSignSize+28.f;
    return std::min(layout.innFont*titleScale*imageScale,(layout.windowWidth-2*layout.paddingX-reservedWidth)/session.titleArt.aspect);
}
float MenuInnTitleRowHeight(float height) {
    const auto normal=height?inn::InnTitleRowHeight(height,session.titleArt,inn::strings.IsChinese()):inn::uiLayout.innFont*1.3f;
    return !session.bundleArt[0].path.empty()?std::max(normal,inn::uiLayout.innSignSize):normal;
}
void MenuInnTitle(float height,float scale) {
    const auto& layout=inn::uiLayout;
    const auto& sign=session.bundleArt[0];
    if(!session.bundleArtAttempted[0]&&!sign.path.empty()) {
        session.bundleArtAttempted[0]=true;
        session.bundleArtTextures[0]=SKSEMenuFramework::LoadTexture(sign.path);
    }
    const auto origin=ImGui::GetCursorPos();
    const bool centeredTitle=height&&inn::strings.IsChinese();
    const float rowHeight=MenuInnTitleRowHeight(height);
    const float signSize=layout.innSignSize*scale;
    const float signSpace=session.bundleArtTextures[0]?signSize+24*scale:0;
    MenuFont(layout.innFont,scale);
    const float width=height?height*session.titleArt.aspect*scale:
        std::min(ImGui::CalcTextSize(session.sellerName.c_str()).x,
            ImGui::GetWindowSize().x-2*layout.paddingX*scale-signSpace);
    const float left=(ImGui::GetWindowSize().x-width-signSpace)*.5f+layout.innX*scale-
        (centeredTitle?signSpace*.5f:0.f);
    const float top=origin.y+layout.innY*scale;
    if(session.bundleArtTextures[0]) {
        const auto fit=inn::FitArtwork(signSize,sign.quarterTurns%2?1/sign.aspect:sign.aspect);
        const auto windowPos=ImGui::GetWindowPos();
        const ImVec2 a{windowPos.x+left+fit.x,windowPos.y+top+(rowHeight*scale-signSize)*.5f+fit.y};
        const auto uv=inn::ArtworkUVs(sign);
        ImGui::ImDrawListManager::AddImageQuad(ImGui::GetWindowDrawList(),session.bundleArtTextures[0],
            a,{a.x+fit.width,a.y},{a.x+fit.width,a.y+fit.height},{a.x,a.y+fit.height},
            {uv[0][0],uv[0][1]},{uv[1][0],uv[1][1]},{uv[2][0],uv[2][1]},{uv[3][0],uv[3][1]},IM_COL32(255,255,255,255));
    }
    if(height) {
        const auto bodyCenter=inn::InnTitleBodyCenter(session.titleArt,inn::strings.IsChinese());
        ImGui::SetCursorPos({left+signSpace,top+(rowHeight*.5f-height*bodyCenter)*scale});
        const auto& uv=session.titleArt.uv;
        ImGui::Image(session.titleArtTexture,{width,height*scale},{uv[0],uv[1]},{uv[2],uv[3]},menuInkColor);
    } else {
        ImGui::SetCursorPos({left+signSpace,top+(rowHeight-layout.innFont)*.5f*scale});
        ImGui::TextUnformatted(session.sellerName.c_str());
    }
    ImGui::SetCursorPos({origin.x,origin.y+(rowHeight+layout.itemGapY)*scale});
}
// Move a section visually without shifting the following sections' normal flow.
struct MenuSectionOffset {
    ImVec2 origin,delta;
    MenuSectionOffset(float x,float y,float scale):origin(ImGui::GetCursorPos()),delta{x*scale,y*scale} {
        ImGui::SetCursorPos({origin.x+delta.x,origin.y+delta.y});
    }
    ~MenuSectionOffset(){auto pos=ImGui::GetCursorPos();ImGui::SetCursorPos({origin.x,pos.y-delta.y});}
};
void MenuPaper() {
    static bool attempted{};
    static ImTextureID texture{};
    if(!attempted) {
        attempted=true;
        texture=SKSEMenuFramework::LoadTexture("Data/Interface/PullUpAChairOrdering/parchment.png");
        if(texture)logger::info("Parchment menu background loaded; style is local to PullUpAChairOrdering");
        else logger::warn("Parchment background unavailable; using plain paper fallback");
    }
    if(!texture)return;
    auto draw=ImGui::GetWindowDrawList();auto pos=ImGui::GetWindowPos();auto size=ImGui::GetWindowSize();
    ImVec2 end{pos.x+size.x,pos.y+size.y};
    ImGui::ImDrawListManager::PushClipRect(draw,pos,end,false);
    ImGui::ImDrawListManager::AddImage(draw,texture,pos,end,{0,0},{1,1},IM_COL32(255,255,255,255));
    // A feathered cream wash quiets the reading area while retaining the paper edge.
    const float scale=size.x/inn::uiLayout.windowWidth;
    for(int inset=0;inset<12;++inset) {
        const float x=(18+inset*2)*scale,y=(20+inset*2)*scale;
        ImGui::ImDrawListManager::AddRectFilled(draw,{pos.x+x,pos.y+y},{end.x-x,end.y-y},
            IM_COL32(244,232,203,11),0,0);
    }
    ImGui::ImDrawListManager::PopClipRect(draw);
}
void MenuCardBorder(bool focused,bool selected,float scale) {
    // Called after EndChild, on the grid's draw list. Keep the grid clip so
    // partly visible cards cannot paint over the fixed summary and footer.
    auto draw=ImGui::GetWindowDrawList();auto pos=ImGui::GetItemRectMin();auto end=ImGui::GetItemRectMax();
    end.x-=1*scale;end.y-=1*scale;
    if(selected) {
        ImGui::ImDrawListManager::AddRect(draw,{pos.x+4*scale,pos.y+4*scale},
            {end.x-4*scale,end.y-4*scale},IM_COL32(41,27,14,180),0,0,1.2f*scale);
    }
    if(focused) {
        // Four short corners identify controller focus independently of selection.
        const float length=18*scale,stroke=2.f*scale;
        const auto ink=IM_COL32(41,27,14,255);
        for(int side=0;side<2;++side)for(int edge=0;edge<2;++edge) {
            const float x=side?end.x-length:pos.x,y=edge?end.y-stroke:pos.y;
            ImGui::ImDrawListManager::AddRectFilled(draw,{x,y},{x+length,y+stroke},ink,0,0);
            const float vx=side?end.x-stroke:pos.x,vy=edge?end.y-length:pos.y;
            ImGui::ImDrawListManager::AddRectFilled(draw,{vx,vy},{vx+stroke,vy+length},ink,0,0);
        }
    }
}
void MenuDishName(std::string text,float width) {
    float limit=ImGui::GetFontSize()*2.0f+1.0f;
    std::string displayed=text;
    while(!text.empty()&&ImGui::CalcTextSize(displayed.c_str(),nullptr,false,width).y>limit) {
        auto end=text.size()-1;
        while(end>0&&(static_cast<unsigned char>(text[end])&0xC0)==0x80)--end;
        text.resize(end);displayed=text+"…";
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
    ImGui::TextWrapped("%s",displayed.c_str());ImGui::PopTextWrapPos();
}
void MenuCenteredName(const std::string& text,float maximumWidth,float font,float scale,float centerX) {
    MenuFont(font,scale);
    const float textWidth=std::min(maximumWidth,ImGui::CalcTextSize(text.c_str()).x);
    ImGui::SetCursorPosX(centerX-textWidth*.5f);
    MenuDishName(text,textWidth);
}
void MenuMealPictures(const std::array<inn::Id,3>& dishes,float x,float y,float size,float scale) {
    if(size<=0)return;
    // Disjoint regions reserve a large main upper-left, dessert below it,
    // and a tall drink at the right. Alpha bounds keep slim foods legible.
    constexpr std::array<ImVec4,3> boxes{{
        {0.00f,0.00f,0.66f,0.60f},
        {0.70f,0.02f,0.30f,0.96f},
        {0.10f,0.66f,0.52f,0.34f}
    }};
    const auto windowPos=ImGui::GetWindowPos();
    for(int slot=0;slot<3;++slot)if(auto key=dishes[slot]) {
        const auto box=boxes[slot];
        const float left=x+box.x*size,top=y+box.y*size;
        const float imageWidth=box.z*size,imageHeight=box.w*size;
        auto row=std::find_if(session.items.begin(),session.items.end(),[key](const Item& item){return inn::RowKey(item)==key;});
        if(row!=session.items.end()&&!row->texture&&!row->image.empty()&&!row->textureAttempted) {
            row->textureAttempted=true;
            row->texture=SKSEMenuFramework::LoadTexture(row->image);
        }
        if(row!=session.items.end()&&row->texture) {
            if(!row->imageBoundsAttempted) {
                row->imageBoundsAttempted=true;
                row->imageBounds=inn::ReadThumbnailBounds(row->image);
            }
            const auto& bounds=row->imageBounds;
            const float displayWidth=std::min(imageWidth,imageHeight*bounds.aspect);
            const float displayHeight=displayWidth/bounds.aspect;
            const float displayX=left+(imageWidth-displayWidth)*.5f;
            const float displayY=top+(imageHeight-displayHeight)*.5f;
            // Drawing directly keeps the whole meal card as a single click target.
            ImGui::ImDrawListManager::AddImage(ImGui::GetWindowDrawList(),row->texture,
                {windowPos.x+displayX,windowPos.y+displayY},
                {windowPos.x+displayX+displayWidth,windowPos.y+displayY+displayHeight},
                {bounds.uv[0],bounds.uv[1]},{bounds.uv[2],bounds.uv[3]},IM_COL32(255,255,255,255));
        } else {
            const char* label=row==session.items.end()||!row->error.empty()||row->textureAttempted?
                T("preview_unavailable"):T("preview_pending");
            MenuFont(inn::uiLayout.cardStatusFont,scale);
            const auto text=ImGui::CalcTextSize(label,nullptr,false,imageWidth);
            ImGui::SetCursorPos({left,top+(imageHeight-text.y)*.5f});
            MenuDishName(label,imageWidth);
        }
    }
}
void MenuBundleCard(int index,float width,float height,float scale) {
    const auto& layout=inn::uiLayout;
    const auto quote=QuoteBundle(index);
    const auto& dishes=BundleItems(index);
    const char* title=T(index==0?"bundle_house_title":"bundle_daily_title");
    const std::string price=!quote.valid?T("bundle_unavailable"):index==0?
        inn::strings.InnText("bundle_price",session.innTextKey):
        fmt::format(fmt::runtime(T("bundle_discount_price")),quote.total);
    bool focused=session.bundleHighlighted==index,selected=IsBundleSelected(index);
    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{layout.cardPadding*scale,layout.cardPadding*scale});
    if(ImGui::BeginChild("bundle_card",{width,height},ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoNavInputs|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        const auto origin=ImGui::GetCursorPos();
        ImGui::BeginDisabled(!quote.valid);
        const bool clicked=ImGui::InvisibleButton("##bundle",{width-2*layout.cardPadding*scale,height-2*layout.cardPadding*scale});
        const bool hovered=ImGui::IsItemHovered();
        ImGui::EndDisabled();
        if(clicked){SelectBundle(index);focused=true;selected=IsBundleSelected(index);}
        const float contentWidth=width-2*layout.cardPadding*scale;
        auto centered=[&](const std::string& text,float font,float y,float maximumWidth=0.f) {
            MenuFont(font,scale);
            float textWidth=std::min(maximumWidth>0?maximumWidth:contentWidth,ImGui::CalcTextSize(text.c_str()).x);
            ImGui::SetCursorPos({(width-textWidth)*.5f,y});
            MenuDishName(text,textWidth);
        };
        centered(title,layout.bundleTitleFont,origin.y+layout.nameY*scale);
        const float priceY=ImGui::GetCursorPos().y+2*scale;
        MenuFont(layout.priceFont,scale);
        const float priceWidth=ImGui::CalcTextSize(price.c_str()).x;
        const float priceX=(width-priceWidth)*.5f;
        MenuFont(layout.priceFont,scale);ImGui::SetCursorPos({priceX,priceY});ImGui::TextUnformatted(price.c_str());
        const auto afterPrice=ImGui::GetCursorPos();
        ImGui::SetCursorPos(afterPrice);
        const float metadataEnd=afterPrice.y;
        const float statusY=height-(layout.cardStatusFont+24)*scale;
        const float textWidth=std::max(40*scale,contentWidth-36*scale);
        const float textX=(width-textWidth)*.5f;
        const float bodyTop=metadataEnd+14*scale;
        const float bodyHeight=std::max(0.f,statusY-bodyTop-14*scale);
        const float pictureSize=std::min(200*scale,bodyHeight);
        const float columnGap=24*scale;
        const float namesX=textX+pictureSize+columnGap;
        const float namesWidth=std::max(40*scale,textWidth-pictureSize-columnGap);
        float itemsFont=layout.bundleItemsFont+(inn::strings.IsChinese()?1.f:0.f);
        float listHeight=0;
        std::array<float,3> nameHeights{},namePositions{};
        std::array<int,3> nameSlots{};
        int nameCount=0;
        float namePitch=0;
        auto measureList=[&] {
            MenuFont(itemsFont,scale);nameCount=0;
            for(int slot=0;slot<3;++slot)if(auto id=dishes[slot]) {
                nameHeights[slot]=std::min(ImGui::CalcTextSize(BundleItemName(id).c_str(),nullptr,false,namesWidth).y,
                    2*itemsFont*scale+1);
                nameSlots[nameCount++]=slot;
            }
            if(!nameCount)return 0.f;
            // Equal center spacing spans the main and dessert centers. A
            // wrapped name increases every row's pitch, preserving the rhythm.
            namePitch=nameCount>1?pictureSize*.53f/(nameCount-1):0;
            for(int row=1;row<nameCount;++row)namePitch=std::max(namePitch,
                (nameHeights[nameSlots[row-1]]+nameHeights[nameSlots[row]])*.5f+layout.itemGapY*scale);
            return (nameCount-1)*namePitch+(nameHeights[nameSlots[0]]+nameHeights[nameSlots[nameCount-1]])*.5f;
        };
        listHeight=measureList();
        while(itemsFont>18&&listHeight>pictureSize) {
            itemsFont-=1;listHeight=measureList();
        }
        if(nameCount) {
            const float firstHalf=nameHeights[nameSlots[0]]*.5f;
            const float lastHalf=nameHeights[nameSlots[nameCount-1]]*.5f;
            const float span=(nameCount-1)*namePitch;
            const float firstCenter=std::clamp(pictureSize*.565f-span*.5f,firstHalf,
                std::max(firstHalf,pictureSize-span-lastHalf));
            for(int row=0;row<nameCount;++row) {
                const int slot=nameSlots[row];
                namePositions[slot]=firstCenter+row*namePitch-nameHeights[slot]*.5f;
            }
        }
        const float pictureY=metadataEnd+4*scale;
        const float artworkTop=bodyTop+std::max(0.f,(bodyHeight-pictureSize)*.5f);
        const auto windowPos=ImGui::GetWindowPos();
        auto draw=ImGui::GetWindowDrawList();
        ImGui::ImDrawListManager::AddRectFilled(draw,
            {windowPos.x+textX,windowPos.y+pictureY-4*scale},
            {windowPos.x+textX+textWidth,windowPos.y+pictureY-3*scale},IM_COL32(77,49,24,65),0,0);
        MenuMealPictures(dishes,textX,artworkTop,pictureSize,scale);
        MenuFont(itemsFont,scale);
        bool any=false;
        for(int slot=0;slot<3;++slot)if(auto id=dishes[slot]) {
            any=true;ImGui::SetCursorPos({namesX,artworkTop+namePositions[slot]});MenuDishName(BundleItemName(id),namesWidth);
#ifdef INN_MENU_PREVIEW
            PreviewCaptureMealName(index,slot);
#endif
        }
        if(!any) {
            ImGui::SetCursorPos({namesX,artworkTop+(pictureSize-itemsFont*scale)*.5f});
            MenuDishName(T("bundle_items_empty"),namesWidth);
        }
        if(quote.valid) {
            const char* action=selected?T(focused?"bundle_remove_pad":"bundle_selected"):
                T(focused?"bundle_select_pad":"bundle_select");
            MenuFont(layout.cardStatusFont,scale);
            const auto size=ImGui::CalcTextSize(action);
            if(selected) {
                const float badgeWidth=std::min(contentWidth,size.x+28*scale);
                ImGui::ImDrawListManager::AddRectFilled(draw,
                    {windowPos.x+(width-badgeWidth)*.5f,windowPos.y+statusY-7*scale},
                    {windowPos.x+(width+badgeWidth)*.5f,windowPos.y+statusY+size.y+7*scale},
                    IM_COL32(62,39,18,255),0,0);
                ImGui::PushStyleColor(ImGuiCol_Text,{0.98f,0.93f,0.82f,1});
            }
            centered(action,layout.cardStatusFont,statusY);
            if(selected)ImGui::PopStyleColor();
        }
        if(hovered) {
            ImGui::BeginTooltip();MenuFont(layout.tooltipFont,scale);ImGui::PushTextWrapPos(560*scale);
            ImGui::TextWrapped("%s",title);ImGui::TextWrapped("%s",price.c_str());
            for(auto id:dishes)if(id)ImGui::TextWrapped("- %s",BundleItemName(id).c_str());
            ImGui::PopTextWrapPos();ImGui::EndTooltip();
        }
    }
    ImGui::EndChild();MenuCardBorder(focused,selected,scale);ImGui::PopStyleVar();ImGui::PopID();
}

void __stdcall Draw() {
    std::lock_guard lock(gate);
    for(auto& image:disposeImages)SKSEMenuFramework::DisposeTexture(image);disposeImages.clear();
    if(session.phase!=food::Phase::open){window->IsOpen=false;return;}
    auto io=GalleryIO();
    inn::menufont::Scope menuTypeface(inn::strings.IsChinese(),io);
    const auto& layout=inn::uiLayout;
    const float titleHeight=MenuTitleHeight();
    // Give the taller handwriting its own room without reducing the card grid.
    // The sign belongs to the inn heading, not to either meal's contents.
    const float headingHeight=MenuInnTitleRowHeight(titleHeight);
    const float titleExtra=std::max(0.f,headingHeight-layout.innFont);
    const float bars=layout.letterboxPercent<0?inn::menuBarFraction:layout.letterboxPercent/100;
    const auto frame=inn::FitMenu(io->DisplaySize.x,io->DisplaySize.y,bars,inn::menuScale*layout.scalePercent/100,
                                layout.windowWidth,layout.windowHeight+titleExtra,layout.offsetX,layout.offsetY);
    const float scale=frame.scale;
    ParchmentStyle style(scale);
    ImGui::SetNextWindowSize({frame.width,frame.height},ImGuiCond_Always);
    ImGui::SetNextWindowPos({frame.x,frame.y},ImGuiCond_Always,{0,0});
    bool opened=true;
    auto flags=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoNavInputs|ImGuiWindowFlags_NoTitleBar|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
    if(ImGui::Begin((std::string(T("title"))+"###PullUpAChairOrdering").c_str(),&opened,flags)) {
        MenuPaper();
        MenuInnTitle(titleHeight,scale);
        const bool notice=!session.status.empty()&&session.status!=T("choose")&&
            session.status!=T("specialty_selected")&&session.status!=T("daily_selected");
        if(notice) {
        const std::string& caption=session.status;
        MenuFont(layout.statusFont,scale);
        const float captionWidth=std::min(ImGui::CalcTextSize(caption.c_str()).x,ImGui::GetContentRegionAvail().x);
        {MenuSectionOffset offset(layout.statusX,layout.statusY,scale);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x-captionWidth)*.5f+layout.statusX*scale);
        MenuDishName(caption,captionWidth);}
        }
        MenuFont(layout.tabFont,scale);
        constexpr int categoryCount=4;
        float tabWidth=layout.tabWidth*scale,tabHeight=layout.tabHeight*scale,tabGap=layout.tabGap*scale;
        float tabsWidth=categoryCount*tabWidth+(categoryCount-1)*tabGap;
        {MenuSectionOffset offset(layout.tabsX,layout.tabsY,scale);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x-tabsWidth)*0.5f+layout.tabsX*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
        ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,0,0,0});
        for(int c:inn::menuTabs) {
            if(c!=3)ImGui::SameLine(0,tabGap);
            const char* label=c==3?T("specialty"):Course(c);
            if(ImGui::Button(label,{tabWidth,tabHeight})){
                SwitchMenuTab(c);
            }
            if(session.tab==c) {
                auto lo=ImGui::GetItemRectMin();auto hi=ImGui::GetItemRectMax();
                float textWidth=ImGui::CalcTextSize(label).x;
                float left=(lo.x+hi.x-textWidth)*0.5f;
                ImGui::ImDrawListManager::AddRectFilled(ImGui::GetWindowDrawList(),
                    {left,hi.y-3*scale},{left+textWidth,hi.y},IM_COL32(41,27,14,255),0,0);
            }
        }
        ImGui::PopStyleColor(3);ImGui::PopStyleVar();}
        auto pad=PollPad();
        if(pad&0x100)SwitchMenuTab(inn::MoveMenuTab(session.tab,-1));
        if(pad&0x200)SwitchMenuTab(inn::MoveMenuTab(session.tab,1));
        if((pad&0x8000)&&QuoteBundle(1).valid)SelectDaily();
        std::vector<int> visibleItems;
        for(std::size_t i=0;i<session.items.size();++i) {
            auto& row=session.items[i];
            if(session.tab==3)continue;
            if(session.tab>=0&&session.tab<3&&!(inn::BrowseMask(row)&(1<<session.tab)))continue;
            if(session.search[0]&&row.name.find(session.search.data())==std::string::npos)continue;
            visibleItems.push_back(int(i));
        }
        if(session.tab!=3&&!visibleItems.empty()&&std::find(visibleItems.begin(),visibleItems.end(),session.highlighted)==visibleItems.end())session.highlighted=visibleItems.front();
        ImGui::Separator();
        float remaining=std::min(layout.gridHeight*scale,std::max(150*scale,ImGui::GetContentRegionAvail().y-layout.footerReserve*scale));
        const float gridWidth=ImGui::GetContentRegionAvail().x;
        {MenuSectionOffset offset(layout.gridX,layout.gridY,scale);
        if(ImGui::BeginChild("food_grid",{gridWidth,remaining},0,ImGuiWindowFlags_NoNavInputs)) {
            auto gridPosition=ImGui::GetWindowPos();auto gridSize=ImGui::GetWindowSize();
            ImGui::PushClipRect(gridPosition,{gridPosition.x+gridSize.x,gridPosition.y+gridSize.y},true);
            MenuFont(layout.dishFont,scale);
            constexpr int columns=2;
            float gap=layout.cardGap*scale,cardWidth=(ImGui::GetContentRegionAvail().x-gap)/columns;
            float cardHeight=layout.cardHeight*scale,imageSize=layout.imageSize*scale;
            int visible=0;
            if(session.tab==3) {
                const int firstBundle=FirstVisibleBundle();
                session.bundleHighlighted=std::clamp(session.bundleHighlighted,firstBundle,1);
                if(pad&4)session.bundleHighlighted=firstBundle;
                if(pad&8)session.bundleHighlighted=1;
                if((pad&0x1000)&&!(pad&0x2000))SelectBundle(session.bundleHighlighted);
                if(firstBundle==1)ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(ImGui::GetContentRegionAvail().x-cardWidth)*.5f);
                for(int index=firstBundle;index<2;++index) {
                    if(index>firstBundle)ImGui::SameLine(0,gap);
                    MenuBundleCard(index,cardWidth,layout.bundleHeight*scale,scale);
                }
            } else {
            if((pad&15)&&!visibleItems.empty()) {
                int pos=int(std::find(visibleItems.begin(),visibleItems.end(),session.highlighted)-visibleItems.begin());
                int dx=(pad&4)?-1:(pad&8)?1:0,dy=(pad&1)?-1:(pad&2)?1:0;
                session.highlighted=visibleItems[food::MoveGrid(pos,int(visibleItems.size()),columns,dx,dy)];scrollToHighlight=true;
            }
            if((pad&0x1000)&&!(pad&0x2000)&&!visibleItems.empty()) {
                auto& row=session.items[session.highlighted];
                SelectItem(row);
            }
            for(auto i:visibleItems) {
                auto& row=session.items[i];
                if(visible++%columns)ImGui::SameLine(0,gap);
                ImGui::PushID(static_cast<int>(row.id));
                ImGui::PushID(row.generated?1:0);
                bool focused=session.highlighted==i;
                bool selected=IsSelected(row);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{layout.cardPadding*scale,layout.cardPadding*scale});
                if(ImGui::BeginChild("dish_card",{cardWidth,cardHeight},ImGuiChildFlags_AlwaysUseWindowPadding,
                        ImGuiWindowFlags_NoNavInputs|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
                    auto origin=ImGui::GetCursorPos();
                    bool clicked=ImGui::InvisibleButton("##dish",{cardWidth-2*layout.cardPadding*scale,cardHeight-2*layout.cardPadding*scale});
                    bool hovered=ImGui::IsItemHovered();
                    if(clicked) {
                        session.highlighted=static_cast<int>(i);focused=true;
                        SelectItem(row);selected=IsSelected(row);
                    }
                    if(!row.texture&&!row.image.empty())row.texture=SKSEMenuFramework::LoadTexture(row.image);
                    ImGui::SetCursorPos({origin.x+layout.imageX*scale,origin.y+(cardHeight-2*layout.cardPadding*scale-imageSize)*0.5f+layout.imageY*scale});
                    if(row.texture)ImGui::Image(row.texture,{imageSize,imageSize});
                    float textX=origin.x+imageSize+(layout.textGap+layout.textX)*scale;
                    float textWidth=std::max(20*scale,cardWidth-textX-layout.cardPadding*scale);
                    ImGui::SetCursorPos({textX,origin.y+layout.nameY*scale});
                    ImGui::PushStyleColor(ImGuiCol_Text,menuInkColor);
                    MenuFont(layout.dishFont,scale);MenuDishName(row.name,textWidth);
                    ImGui::PopStyleColor();
                    const float priceY=std::max(origin.y+layout.priceY*scale,ImGui::GetCursorPos().y+4*scale);
                    ImGui::SetCursorPos({textX,priceY});
                    ImGui::PushStyleColor(ImGuiCol_Text,menuInkColor);
                    MenuFont(layout.priceFont,scale);
                    if(row.generated)ImGui::TextUnformatted(fmt::format(fmt::runtime(T("price_generated")),row.price).c_str());
                    else ImGui::TextUnformatted(fmt::format(fmt::runtime(T("price_stock")),row.price,row.count).c_str());
                    ImGui::PopStyleColor();
                    ImGui::SetCursorPos({textX,std::max(origin.y+layout.cardStatusY*scale,ImGui::GetCursorPos().y+3*scale)});
                    MenuFont(layout.cardStatusFont,scale);
                    ImGui::PushStyleColor(ImGuiCol_Text,menuInkColor);
                    if(!row.error.empty())ImGui::TextUnformatted(T("preview_unavailable"));
                    else if(selected)ImGui::TextWrapped("%s",T("selected"));
                    else if(row.course<0)ImGui::TextUnformatted(T("other"));
                    else if(!row.texture)ImGui::TextUnformatted(T("preview_pending"));
                    else ImGui::TextUnformatted(fmt::format(fmt::runtime(T("select_as")),Course(SelectedSlot(row))).c_str());
                    ImGui::PopStyleColor();
                    MenuFont(layout.dishFont,scale);
                    bool titleTruncated=ImGui::CalcTextSize(row.name.c_str(),nullptr,false,textWidth).y>2*layout.dishFont*scale;
                    if(hovered&&(!row.error.empty()||titleTruncated)) {
                        ImGui::BeginTooltip();MenuFont(layout.tooltipFont,scale);
                        ImGui::PushTextWrapPos(560*scale);
                        ImGui::TextWrapped("%s",row.name.c_str());
                        if(!row.error.empty())ImGui::TextWrapped("%s",T("preview_unavailable"));
                        ImGui::PopTextWrapPos();ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();MenuCardBorder(focused,selected,scale);ImGui::PopStyleVar();ImGui::PopID();ImGui::PopID();
                if(focused&&scrollToHighlight){ImGui::SetScrollHereY(0.5f);scrollToHighlight=false;}
            }
            if(visibleItems.empty())ImGui::TextWrapped("%s",T("empty_category"));
            }
            ImGui::PopClipRect();
        }
        ImGui::EndChild();}MenuFont(layout.summaryNameFont,scale);ImGui::Separator();
        const bool anySelected=std::any_of(session.selected.begin(),session.selected.end(),[](auto id){return id!=0;});
        {MenuSectionOffset offset(layout.summaryX,layout.summaryY,scale);
        const auto summaryStart=ImGui::GetCursorPos();
        const float summaryWidth=ImGui::GetContentRegionAvail().x;
        if(!anySelected) {
            MenuCentered(T("menu_order_empty"),layout.summaryNameFont,scale,layout.summaryX,18);
        } else {
            const int bundle=IsBundleSelected(0)?0:IsBundleSelected(1)?1:-1;
            const std::string summary=bundle>=0?fmt::format(fmt::runtime(T("menu_order_bundle")),
                T(bundle==0?"bundle_house_title":"bundle_daily_title")):T("menu_order_custom");
            MenuCenteredName(summary,summaryWidth,layout.summaryLabelFont,scale,summaryStart.x+summaryWidth*.5f);
            std::string names;
            for(auto id:session.selected)if(id) {
                if(!names.empty())names+="  ·  ";
                names+=BundleItemName(id);
            }
            MenuCenteredName(names,summaryWidth,layout.summaryNameFont+(inn::strings.IsChinese()?1.f:0.f),scale,summaryStart.x+summaryWidth*.5f);
        }
        ImGui::SetCursorPos({summaryStart.x,summaryStart.y+(layout.summaryHeight+layout.itemGapY)*scale});
        }
        MenuFont(layout.buttonFont,scale);
        bool complete=UpdateQuote();
        {MenuSectionOffset offset(layout.actionsX,layout.actionsY,scale);
        const auto actionOrigin=ImGui::GetCursorPos();
        const float confirmStart=actionOrigin.x;
        const float cancelStart=std::max(confirmStart+(layout.confirmWidth+layout.itemGapX)*scale,ImGui::GetWindowSize().x-layout.paddingX*scale-layout.cancelWidth*scale);
        ImGui::SetCursorPosX(confirmStart);
        MenuFont(layout.buttonFont,scale);
        ImGui::PushStyleColor(ImGuiCol_Button,{0.29f,0.18f,0.085f,0.95f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.38f,0.24f,0.105f,1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0.22f,0.13f,0.055f,1.0f});
        ImGui::PushStyleColor(ImGuiCol_Text,{0.96f,0.90f,0.77f,1.0f});
        ImGui::BeginDisabled(!complete||session.confirmationQueued);
        if(ImGui::Button(T("menu_confirm"),{layout.confirmWidth*scale,layout.actionHeight*scale})||(complete&&!session.confirmationQueued&&(pad&0x4000)&&!(pad&0x2000)))RequestConfirmationUnlocked();
        ImGui::EndDisabled();ImGui::PopStyleColor(4);ImGui::SetCursorPos({cancelStart,actionOrigin.y});
        if(ImGui::Button(T("menu_cancel"),{layout.cancelWidth*scale,layout.actionHeight*scale}))CancelUnlocked();
        const auto afterActions=ImGui::GetCursorPos();
        MenuFont(layout.totalFont,scale);
        const auto total=fmt::format(fmt::runtime(T("total")),session.price);
        ImGui::SetCursorPos({confirmStart+(layout.confirmWidth+24+layout.totalX)*scale,
            actionOrigin.y+(std::max(0.f,(layout.actionHeight-layout.totalFont)*.5f)+layout.totalY)*scale});
        ImGui::TextUnformatted(total.c_str());
        ImGui::SetCursorPos(afterActions);}
        MenuFont(layout.controlsFont,scale);
        {MenuSectionOffset offset(layout.controlsX,layout.controlsY,scale);
        ImGui::PushStyleColor(ImGuiCol_Text,menuInkColor);
        ImGui::TextWrapped("%s",T("controls"));ImGui::PopStyleColor();}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)||(pad&0x2000))CancelUnlocked();
        if(session.phase==food::Phase::open)QueueThumbnail();
    }
    ImGui::End();if(!opened)CancelUnlocked();
}
