// 中文阅读说明
// 离线菜单预览程序：提供模拟会话和图片加载，复用真正的羊皮纸菜单绘制。
// 阅读重点：它能检查布局和字体，但不运行游戏里的服务员、付款或上菜行为。

#include <windows.h>
#include <d3d11.h>
#include <DirectXTex.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <array>
#include <algorithm>
#include <string>
#include <cstring>
#include <stdexcept>
#include "Policy.h"
#include "Localization.h"
#include "OrderPolicy.h"
#include "DailyPolicy.h"
#include "BundlePolicy.h"
#include "MenuLayout.h"
#include "UiLayout.h"
#include "Artwork.h"
#include "ThumbnailBounds.h"

namespace inn
{
    float menuBarFraction = .12f, menuScale = 1.f;
}

using inn::T;
#include "Navigation.h"

namespace ImGui::ImDrawListManager
{
    void PushClipRect(ImDrawList *d, ImVec2 a, ImVec2 b, bool intersect)
    {
        d->PushClipRect(a, b, intersect);
    }

    void PopClipRect(ImDrawList *d)
    {
        d->PopClipRect();
    }

    void AddImage(ImDrawList *d, ImTextureID t, ImVec2 a, ImVec2 b, ImVec2 ua, ImVec2 ub, ImU32 c)
    {
        d->AddImage(t, a, b, ua, ub, c);
    }

    void AddImageQuad(ImDrawList *d,
                      ImTextureID t,
                      ImVec2 a,
                      ImVec2 b,
                      ImVec2 c,
                      ImVec2 e,
                      ImVec2 ua,
                      ImVec2 ub,
                      ImVec2 uc,
                      ImVec2 ue,
                      ImU32 color)
    {
        d->AddImageQuad(t, a, b, c, e, ua, ub, uc, ue, color);
    }

    void AddRect(ImDrawList *d,
                 ImVec2 a,
                 ImVec2 b,
                 ImU32 c,
                 float rounding,
                 ImDrawFlags flags,
                 float thickness)
    {
        d->AddRect(a, b, c, rounding, flags, thickness);
    }

    void
    AddRectFilled(ImDrawList *d, ImVec2 a, ImVec2 b, ImU32 c, float rounding, ImDrawFlags flags)
    {
        d->AddRectFilled(a, b, c, rounding, flags);
    }
}

namespace logger
{
    template <class... Args> void info(fmt::format_string<Args...> s, Args &&...args)
    {
        std::cout << fmt::format(s, std::forward<Args>(args)...) << '\n';
    }

    template <class... Args> void warn(fmt::format_string<Args...> s, Args &&...args)
    {
        std::cout << fmt::format(s, std::forward<Args>(args)...) << '\n';
    }
}

static ID3D11Device *previewDevice{};
static ID3D11DeviceContext *previewContext{};
static std::filesystem::path workspace;
static std::map<std::string, ID3D11ShaderResourceView *> textures;
static bool missingPaper{};
static bool missingTitle{};

static void Check(HRESULT result, const char *operation)
{
    if (FAILED(result))
        throw std::runtime_error(
            fmt::format("{} failed: {:08X}", operation, static_cast<unsigned>(result)));
}

static std::wstring Wide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    out.pop_back();
    return out;
}

namespace SKSEMenuFramework
{
    ImTextureID LoadTexture(const std::string &path)
    {
        if (missingTitle && path.ends_with("_title.png"))
            return nullptr;
        if (auto i = textures.find(path); i != textures.end())
            return i->second;
        auto actual = path == "Data/Interface/PullUpAChairOrdering/parchment.png"
                          ? workspace / L"assets/parchment.png"
                          : std::filesystem::path(Wide(path));
        if (path.starts_with("Data/Interface/PullUpAChairOrdering/InnSigns/"))
            actual = workspace / "assets/inn-signs" / std::filesystem::path(path).filename();
        if (missingPaper && path == "Data/Interface/PullUpAChairOrdering/parchment.png")
            return nullptr;
        DirectX::ScratchImage pixels;
        if (FAILED(DirectX::LoadFromWICFile(
                actual.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, nullptr, pixels)))
            return nullptr;
        ID3D11ShaderResourceView *view{};
        Check(DirectX::CreateShaderResourceView(previewDevice,
                                                pixels.GetImages(),
                                                pixels.GetImageCount(),
                                                pixels.GetMetadata(),
                                                &view),
              "Create thumbnail view");
        textures[path] = view;
        return view;
    }

    void DisposeTexture(const std::string &)
    {
    }
}

struct Item
{
    std::uint32_t id{};
    std::string name, model, image, error;
    int count{}, course{-1}, mask{}, price{}, menuMask{-1};
    bool attempted{}, automatic{}, generated{}, textureAttempted{};
    ImTextureID texture{};
    inn::ThumbnailBounds imageBounds;
    bool imageBoundsAttempted{};
};

struct Session
{
    int token{}, price{20};
    food::Phase phase{food::Phase::open};
    std::string sellerName, status, innTextKey;
    std::vector<Item> items;
    std::array<inn::Id, 3> selected{}, daily{}, specialty{};
    bool dailyMode{},confirmationQueued{},hideEmptySpecialty{};
    int highlighted{-1}, tab{3}, bundleHighlighted{0};
    std::array<char, 128> search{};
    std::array<inn::Artwork, 2> bundleArt;
    std::array<ImTextureID, 2> bundleArtTextures{};
    std::array<bool, 2> bundleArtAttempted{};
    inn::Artwork titleArt;
    ImTextureID titleArtTexture{};
    bool titleArtAttempted{};
};

std::mutex gate;
Session session;
std::vector<std::string> disposeImages;
bool scrollToHighlight{};

struct WindowState
{
    bool IsOpen{true};
};

WindowState previewWindow;
WindowState *window = &previewWindow;
std::uint32_t previewPad{};

std::uint32_t PollPad()
{
    return previewPad;
}

void CancelUnlocked()
{
    session.phase = food::Phase::cancelled;
    window->IsOpen = false;
}

void QueueThumbnail()
{
}

ImGuiIO *GalleryIO()
{
    return &ImGui::GetIO();
}

void SwitchMenuTab(int tab)
{
    if (session.tab != tab)
    {
        session.tab = tab;
        scrollToHighlight = true;
    }
}

const char *Course(int n)
{
    return T(n == 0 ? "main" : n == 1 ? "drink" : n == 2 ? "side" : "other");
}

inn::Quote CurrentQuote()
{
    std::map<inn::Id, inn::Offer> offers;
    for (const auto &row : session.items)
        offers[inn::RowKey(row)] = {row.count, row.price, row.mask, row.generated};
    return inn::MenuPrice(session.selected, offers, session.dailyMode, session.daily);
}

bool UpdateQuote()
{
    const auto quote = CurrentQuote();
    session.price = quote.total;
    return quote.valid;
}

#include "BundleSelection.inl"

int SelectedSlot(const Item &row)
{
    return inn::SelectionSlot(row, session.tab);
}

bool IsSelected(const Item &row)
{
    return inn::RowSelected(session.selected, row, session.tab);
}

void SelectItem(const Item &row)
{
    session.dailyMode = false;
    session.status = T("choose");
    inn::ToggleRow(session.selected, row, session.tab);
    UpdateQuote();
}

std::array<std::array<ImVec4, 3>, 2> previewMealNames{};

void PreviewCaptureMealName(int card, int slot)
{
    const auto start = ImGui::GetItemRectMin(), end = ImGui::GetItemRectMax();
    previewMealNames[card][slot] = {start.x, start.y, end.x, end.y};
}

#define PUAC_FONT_PREVIEW
#include "MenuTypeface.h"
#define INN_MENU_PREVIEW
// The renderer preview has no game world or inventory. The real confirmation
// callback is covered by confirmation_tests with an explicit game-task queue.
void RequestConfirmationUnlocked(){session.phase=food::Phase::selected;window->IsOpen=false;}
#include "ParchmentMenu.inl"
#undef INN_MENU_PREVIEW

// 按指定场景填充模拟菜品和会话，供离线布局检查。
void Populate(const std::string &scenario)
{
    const auto key = std::getenv("INN_PREVIEW_INN");
    session.hideEmptySpecialty = key && std::string_view(key) == "new_gnisis";
    const auto venue = inn::innArtwork.Find(key ? key : "vilemyr");
    const auto common = inn::innArtwork.Find("recommendation");
    if (venue)
    {
        session.sellerName =
            std::getenv("INN_PREVIEW_EN") ? venue->nameEnglish : venue->nameChinese;
        session.bundleArt[0] = venue->image;
        session.innTextKey = venue->key;
    }
    else if (session.hideEmptySpecialty)
        session.sellerName = std::getenv("INN_PREVIEW_EN") ? "New Gnisis Cornerclub" : "新格尼斯俱乐部";
    else
        session.sellerName = std::getenv("INN_PREVIEW_EN") ? "Unknown Inn" : "未知酒馆";
    if (common)
        session.bundleArt[1] = common->image;
    const auto titleFile = inn::InnTitleCatalogPath(workspace / "config", inn::strings.IsChinese());
    if (venue && std::filesystem::exists(titleFile))
    {
        inn::ArtworkCatalog titles;
        titles.Load(titleFile);
        for (const auto &entry : titles.entries)
            if (entry.plugin == venue->plugin && entry.local == venue->local)
            {
                session.titleArt = entry.image;
                break;
            }
    }
    missingTitle = scenario == "title-missing";
    session.status = T("choose");

    struct Sample
    {
        std::uint32_t id;
        const char *name;
        int count;
        int course;
        const char *image;
    };

    static const Sample samples[] = {
#include "samples.inc"
    };
    for (const auto &s : samples)
    {
        Item item;
        item.id = s.id;
        item.name = s.name;
        item.count = s.count;
        item.course = s.course;
        item.mask = 1 << s.course;
        item.price = 8 + (s.id % 15);
        item.image = s.image;
        session.items.push_back(item);
    }
    if (std::getenv("INN_PREVIEW_EN"))
    {
        const char *names[] = {"Cabbage Potato Soup",
                               "Elsweyr Fondue",
                               "Venison Stew",
                               "Roasted Rabbit Leg",
                               "Pheasant Roast",
                               "Wine",
                               "White Wine",
                               "Honningbrew Mead",
                               "Nord Mead",
                               "Alto Wine",
                               "Argonian Bloodwine",
                               "Goat Cheese Wheel",
                               "Sweet Roll"};
        int n = 0;
        for (auto &item : session.items)
            item.name = names[n++];
    }
    std::array<unsigned, 3> specialtyLocals{};
    std::map<unsigned, std::string> specialtyImages;
    inn::Catalog::Read(workspace / "preview/specialty-images.tsv",
                       [&](const auto &fields)
                       {
                           if (fields.size() >= 2)
                               specialtyImages[std::stoul(fields[0], nullptr, 16)] = fields[1];
                       });
    for (auto &row : session.items)
        if (specialtyImages.contains(row.id))
            row.image = specialtyImages.at(row.id);
    if (venue)
        inn::Catalog::Read(workspace / "config/specialties.tsv",
                           [&](const auto &fields)
                           {
                               if (fields[0] == venue->plugin &&
                                   std::stoul(fields[1], nullptr, 16) == venue->local)
                                   for (int slot = 0; slot < 3; ++slot)
                                       specialtyLocals[slot] =
                                           std::stoul(fields[slot + 3], nullptr, 16);
                           });
    for (int slot = 0; slot < 3; ++slot)
        if (specialtyLocals[slot])
        {
            const int index = slot == 0 ? 0 : slot == 1 ? 5 : 11;
            Item generated = session.items[index];
            generated.generated = true;
            generated.count = 0;
            generated.name = T(fmt::format("specialty_food_{:06X}", specialtyLocals[slot]));
            // A specialty must not inherit an unrelated base fixture's picture.
            generated.image = specialtyImages.contains(specialtyLocals[slot])
                                  ? specialtyImages.at(specialtyLocals[slot])
                                  : "";
            session.specialty[slot] = inn::RowKey(generated);
            session.items.push_back(generated);
        }
    session.highlighted = 0;
    if (scenario == "main")
        session.tab = 0;
    if (scenario == "drink")
        session.tab = 1;
    if (scenario == "side")
        session.tab = 2;
    session.daily = {session.items[0].id, session.items[5].id, session.items[11].id};
    if (scenario == "single-card-empty")
    {
        session.daily = {};
        session.items.clear();
        session.highlighted = -1;
        session.status = T("no_stock");
    }
    if (scenario == "daily")
        SelectDaily();
    if (scenario == "specialty")
        SelectSpecialty();
    if (scenario == "specialty-empty-stock")
    {
        for (auto &row : session.items)
            if (!row.generated)
                row.count = 0;
        SelectSpecialty();
    }
    if (scenario.starts_with("soup-"))
    {
        for (auto &row : session.items)
            if (row.id == 1000219 || row.id == 1000221)
            {
                row.course = 2;
                row.menuMask = 4;
                row.mask = row.generated ? 5 : 4;
            }
        if (scenario == "soup-main")
            session.tab = 0;
        if (scenario == "soup-side")
            session.tab = 2;
        if (scenario == "soup-specialty")
            SelectSpecialty();
        if (scenario == "soup-select")
        {
            session.tab = 2;
            SelectItem(session.items[0]);
        }
    }
    if (scenario == "sold-out")
        session.items[5].count = 0;
    if (scenario == "wide-bars")
        inn::menuBarFraction = .2f;
    if (scenario == "selected" || scenario == "long")
        SelectSpecialty();
    if (scenario == "long")
    {
        for (auto &row : session.items)
            if (row.id == session.items[0].id)
                row.name = std::getenv("INN_PREVIEW_EN")
                               ? "Slow-roasted venison with herbs, cheese and garden vegetables"
                               : "酒馆老板特制的香草奶酪与烤蔬菜炖鹿肉套餐主菜";
    }
    if (scenario == "empty")
    {
        session.tab = 0;
        session.search[0] = '?';
    }
    if (scenario == "pending")
    {
        session.tab = 0;
        for (auto &item : session.items)
            item.image.clear();
    }
    if (scenario == "fallback")
        missingPaper = true;
    if (scenario == "scroll")
    {
        session.tab = 0;
        session.highlighted = 4;
        scrollToHighlight = true;
    }
    if (scenario == "pad-select" || scenario == "pad-toggle")
    {
        session.tab = 1;
        session.highlighted = 5;
    }
    if (scenario == "mouse-select" || scenario == "wheel" || scenario == "pad-down")
        session.tab = 0;
    if (scenario == "long-main")
    {
        session.tab = 0;
        session.items[0].name =
            std::getenv("INN_PREVIEW_EN")
                ? "Slow-roasted venison with herbs, cheese and garden vegetables"
                : "酒馆老板特制的香草奶酪与烤蔬菜炖鹿肉套餐主菜";
        SelectItem(session.items[0]);
    }
    if (scenario == "notice")
        session.status = T("price_changed");
    if (scenario == "bundle-images-pending" || scenario == "bundle-images-failed")
    {
        for (auto &row : session.items)
        {
            row.image.clear();
            if (scenario == "bundle-images-failed")
            {
                row.error = "fixture image unavailable";
                row.attempted = true;
            }
        }
    }
    if (scenario == "bundle-images-partial")
    {
        for (auto &row : session.items)
            if (inn::RowKey(row) == session.specialty[1])
                row.image.clear();
    }
    if (scenario == "bundle-images-long")
    {
        for (auto &row : session.items)
            if (inn::RowKey(row) == session.specialty[0])
                row.name = std::getenv("INN_PREVIEW_EN")
                               ? "Clam chowder with fresh cream and garden herbs"
                               : "酒馆老板特制的鲜奶油香草蛤蜊浓汤";
        SelectSpecialty();
    }
}

ImGuiWindow *PreviewGrid()
{
    for (auto candidate : ImGui::GetCurrentContext()->Windows)
        if (std::strstr(candidate->Name, "food_grid") && !std::strstr(candidate->Name, "dish_card"))
            return candidate;
    return nullptr;
}

std::vector<ImGuiWindow *> PreviewCards()
{
    std::vector<ImGuiWindow *> cards;
    for (auto candidate : ImGui::GetCurrentContext()->Windows)
        if (candidate->Active && (std::strstr(candidate->Name, "dish_card") ||
                                  std::strstr(candidate->Name, "bundle_card")))
            cards.push_back(candidate);
    std::sort(cards.begin(),
              cards.end(),
              [](const auto a, const auto b)
              {
                  return a->Pos.y == b->Pos.y ? a->Pos.x < b->Pos.x : a->Pos.y < b->Pos.y;
              });
    return cards;
}

ImVec4 PreviewImageBounds(ImTextureID texture, const ImGuiWindow *card = nullptr)
{
    ImVec4 bounds{1.e9f, 1.e9f, -1.e9f, -1.e9f};
    bool found = false;
    for (const auto list : ImGui::GetDrawData()->CmdLists)
    {
        if (card && std::strcmp(list->_OwnerName, card->Name) != 0)
            continue;
        for (const auto &command : list->CmdBuffer)
            if (command.TextureId == texture)
                for (unsigned i = 0; i < command.ElemCount; ++i)
                {
                    const auto &vertex =
                        list->VtxBuffer[list->IdxBuffer[command.IdxOffset + i] + command.VtxOffset];
                    bounds.x = std::min(bounds.x, vertex.pos.x);
                    bounds.y = std::min(bounds.y, vertex.pos.y);
                    bounds.z = std::max(bounds.z, vertex.pos.x);
                    bounds.w = std::max(bounds.w, vertex.pos.y);
                    found = true;
                }
    }
    return found ? bounds : ImVec4{};
}

// 运行预览场景，绘制菜单并输出截图及检查结果。
int main(int argc, char **argv)
{
    try
    {
        if (argc < 6)
            throw std::runtime_error(
                "usage: menu_preview workspace width height scenario output.png");
        workspace = std::filesystem::path(Wide(argv[1]));
        const auto uiEnvironment = _wgetenv(L"INN_PREVIEW_UI");
        const auto layoutFile = uiEnvironment ? std::filesystem::path(uiEnvironment)
                                              : workspace / "config/LocalInnOrderingUI.ini";
        const auto layoutResult = inn::uiLayout.Load(layoutFile);
        const auto mainEnvironment = _wgetenv(L"INN_PREVIEW_MAIN");
        const auto mainFile = mainEnvironment ? std::filesystem::path(mainEnvironment)
                                              : workspace / "config/LocalInnOrdering.ini";
        inn::menuScale =
            GetPrivateProfileIntW(L"Menu", L"ScalePercent", 100, mainFile.c_str()) / 100.f;
        inn::menuBarFraction =
            GetPrivateProfileIntW(L"Menu", L"LetterboxHeight", 120, mainFile.c_str()) / 1000.f;
        if (inn::uiLayout.letterboxPercent >= 0)
            inn::menuBarFraction = inn::uiLayout.letterboxPercent / 100;
        const auto localeEnvironment = _wgetenv(L"INN_PREVIEW_LOCALES");
        const auto localeFolder = localeEnvironment ? std::filesystem::path(localeEnvironment)
                                                    : workspace / "config/locales";
        if (!inn::strings.Load(localeFolder, std::getenv("INN_PREVIEW_EN") ? "en" : "zh"))
            throw std::runtime_error("Preview locale unavailable");
        inn::innArtwork.Load(workspace / "config/inn_artwork.tsv");
        int width = std::stoi(argv[2]), height = std::stoi(argv[3]);
        std::string scenario = argv[4];
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        D3D_FEATURE_LEVEL level{};
        Check(D3D11CreateDevice(nullptr,
                                D3D_DRIVER_TYPE_WARP,
                                nullptr,
                                0,
                                nullptr,
                                0,
                                D3D11_SDK_VERSION,
                                &previewDevice,
                                &level,
                                &previewContext),
              "Create WARP device");
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D *target{};
        ID3D11RenderTargetView *targetView{};
        Check(previewDevice->CreateTexture2D(&desc, nullptr, &target), "Create render target");
        Check(previewDevice->CreateRenderTargetView(target, nullptr, &targetView),
              "Create render view");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DeltaTime = 1.0f / 60.0f;
        ImFontConfig fontConfig{};
        fontConfig.PixelSnapH = true;
        fontConfig.OversampleH = 1;
        fontConfig.OversampleV = 1;
        std::string fontFile = argc > 6 ? argv[6] : "C:/Windows/Fonts/msyh.ttc";
        io.Fonts->AddFontFromFileTTF(
            "C:/Windows/Fonts/msyh.ttc", 32, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
        io.Fonts->TexDesiredWidth = 16384;
        io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        if (!std::getenv("INN_PREVIEW_FONT_MISSING") &&
            !io.Fonts->AddFontFromFileTTF(
                fontFile.c_str(), 32, &fontConfig, io.Fonts->GetGlyphRangesChineseFull()))
            throw std::runtime_error("Preview font unavailable");
        inn::menufont::Initialize(
            std::getenv("INN_PREVIEW_FONT_DISABLED") ? "" : "PUAC-WenKai-Medium.ttf");
        ImGui::StyleColorsDark();
        ImGui_ImplDX11_Init(previewDevice, previewContext);
        Populate(scenario);
        for (int frame = 0; frame < 4; ++frame)
        {
            // PollPad returns press/repeat edges, not the held XInput bitfield.
            previewPad = scenario == "pad-down"                                      ? 2
                         : scenario == "pad-select" && frame == 1                    ? 0x1000
                         : scenario == "pad-toggle" && (frame == 1 || frame == 3)    ? 0x1000
                         : scenario == "bundle-pad-left" && frame == 1               ? 4
                         : scenario == "bundle-pad-left" && frame == 2               ? 0x1000
                         : scenario == "bundle-pad-right" && frame == 1              ? 8
                         : scenario == "bundle-pad-right" && frame == 2              ? 0x1000
                         : scenario == "bundle-toggle" && (frame == 1 || frame == 3) ? 0x1000
                         : scenario == "pad-tab" && frame == 1                       ? 0x200
                         : scenario == "pad-daily" && frame == 1                     ? 0x8000
                         : scenario == "bundle-tab-return" && frame == 1             ? 0x200
                         : scenario == "bundle-tab-return" && frame == 2             ? 0x100
                         : scenario == "bundle-tab-return" && frame == 3             ? 0x1000
                         : scenario == "single-card-empty" && frame == 1             ? 0x1000
                                                                                     : 0;
            if ((scenario == "mouse-select" || scenario == "bundle-mouse-left" ||
                 scenario == "bundle-mouse-right") &&
                frame >= 1)
            {
                auto cards = PreviewCards();
                const auto targetIndex = scenario == "bundle-mouse-left" ||
                    (session.tab == 3 && FirstVisibleBundle() == 1) ? 0 : 1;
                if (cards.size() <= targetIndex)
                    throw std::runtime_error("Missing target card");
                auto target = cards[targetIndex];
                io.MousePos = {target->Pos.x + target->Size.x * 0.70f,
                               target->Pos.y + target->Size.y * 0.5f};
                io.MouseDown[0] = frame == 1;
            }
            if (scenario == "wheel" && frame >= 1)
            {
                auto grid = PreviewGrid();
                if (!grid)
                    throw std::runtime_error("Missing scroll grid");
                io.MousePos = {grid->Pos.x + grid->Size.x * 0.5f,
                               grid->Pos.y + grid->Size.y * 0.5f};
                // New child windows settle their content size on the second frame.
                // Send the wheel after the grid has a valid scrolling range.
                io.MouseWheel = frame == 2 ? -4.0f : 0.0f;
            }
            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();
            auto saved = ImGui::GetStyle();
            int colorDepth = ImGui::GetCurrentContext()->ColorStack.Size;
            int styleDepth = ImGui::GetCurrentContext()->StyleVarStack.Size;
            auto savedFont = ImGui::GetFont();
            const auto fontDepth = ImGui::GetCurrentContext()->FontStack.Size;
            Draw();
            if (ImGui::GetFont() != savedFont ||
                ImGui::GetCurrentContext()->FontStack.Size != fontDepth)
                throw std::runtime_error("Menu font scope leaked");
            auto overlay = ImGui::GetForegroundDrawList();
            overlay->AddRectFilled({0, 0},
                                   {float(width), float(height) * inn::menuBarFraction},
                                   IM_COL32(0, 0, 0, 255));
            overlay->AddRectFilled({0, float(height) * (1 - inn::menuBarFraction)},
                                   {float(width), float(height)},
                                   IM_COL32(0, 0, 0, 255));
            if (std::memcmp(&saved, &ImGui::GetStyle(), sizeof(saved)) != 0)
                throw std::runtime_error("Global style changed after menu Draw");
            if (ImGui::GetCurrentContext()->ColorStack.Size != colorDepth ||
                ImGui::GetCurrentContext()->StyleVarStack.Size != styleDepth)
                throw std::runtime_error("Style stack leaked");
            ImGui::Render();
            constexpr float background[]{0.115f, 0.10f, 0.08f, 1};
            previewContext->OMSetRenderTargets(1, &targetView, nullptr);
            previewContext->ClearRenderTargetView(targetView, background);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
        auto menu = ImGui::FindWindowByName("###PullUpAChairOrdering");
        const bool editMode = std::getenv("INN_PREVIEW_EDIT") != nullptr;
        std::vector<std::string> layoutWarnings;
        auto checkLayout = [&](bool valid, const std::string &message)
        {
            if (!valid)
            {
                if (!editMode)
                    throw std::runtime_error(message);
                layoutWarnings.push_back(message);
            }
        };
        if (!menu)
            throw std::runtime_error("Menu missing");
        checkLayout(menu->Pos.x >= 0 && menu->Pos.y >= 0 && menu->Pos.x + menu->Size.x <= width &&
                        menu->Pos.y + menu->Size.y <= height,
                    "Menu outside display");
        checkLayout(menu->ScrollMax.y <= 1.0f,
                    fmt::format("Menu footer overflow: {}", menu->ScrollMax.y));
        checkLayout(menu->Pos.y >= height * inn::menuBarFraction + 10 &&
                        menu->Pos.y + menu->Size.y <= height * (1 - inn::menuBarFraction) - 10,
                    "Menu overlaps cinematic bars");
        auto grid = PreviewGrid();
        if (!grid)
            throw std::runtime_error("Grid missing");
        auto cards = PreviewCards();
        if (session.tab != 3 && scenario != "empty" && scenario != "scroll" &&
            scenario != "wheel" && scenario != "pad-down")
        {
            for (std::size_t i = 0; i < std::min(std::size_t(4), cards.size()); ++i)
                checkLayout(cards[i]->Pos.y + cards[i]->Size.y <= grid->Pos.y + grid->Size.y + 1,
                            "Four cards do not fit in the cinematic safe area");
        }
        if (scenario != "empty")
        {
            auto expected =
                session.tab == 3
                    ? 2 - FirstVisibleBundle()
                    : std::count_if(session.items.begin(),
                                    session.items.end(),
                                    [](const Item &item)
                                    {
                                        return bool(inn::BrowseMask(item) & (1 << session.tab));
                                    });
            if (cards.size() != expected)
                throw std::runtime_error("Filtered card count mismatch");
            for (std::size_t i = 0; i < cards.size(); ++i)
            {
                checkLayout(cards[i]->Size.x >= cards[i]->Size.y * (session.tab == 3 ? 1.2f : 2.0f),
                            "Card width is too narrow");
                if (i % 2)
                    checkLayout(std::abs(cards[i]->Pos.y - cards[i - 1]->Pos.y) <= 1.0f,
                                "Two-column row mismatch");
                checkLayout(cards[i]->ScrollMax.y <= 1.0f, "Dish content overflows");
            }
        }
        // Generated specialties are bundle-only; category navigation ends on stock.
        if (scenario == "pad-down" && session.highlighted != 4)
            throw std::runtime_error("D-pad left the merchant-stock main-course rows");
        if (scenario == "long-main" &&
            (session.tab != 0 || session.selected[0] != session.items[0].id))
            throw std::runtime_error("Long main-course fixture was not selected");
        if (scenario == "pad-select" && session.selected[1] != session.items[5].id)
            throw std::runtime_error("A did not select highlighted drink");
        if (scenario == "pad-toggle" && session.selected[1] != 0)
            throw std::runtime_error("Second A press did not remove the drink");
        if ((scenario == "daily" || scenario == "pad-daily") &&
            (!session.dailyMode || session.selected != session.daily || session.price != 22))
            throw std::runtime_error("Daily selection/half price mismatch");
        if ((scenario == "specialty" || scenario == "specialty-empty-stock") &&
            (!CurrentQuote().valid || session.selected != session.specialty ||
             !CurrentQuote().inventoryQuantities.empty() || session.price != 43 ||
             cards.size() != 2))
            throw std::runtime_error("Independent house-special supply failed");
        if (scenario == "pad-tab" && session.tab != 0)
            throw std::runtime_error("RB did not select main-course category");
        if (scenario == "soup-main" && (session.tab != 0 || cards.size() != 3))
            throw std::runtime_error("Soup leaked into main category");
        if (scenario == "soup-side" && (session.tab != 2 || cards.size() != 6))
            throw std::runtime_error("Soup absent from side category");
        if (scenario == "soup-select" &&
            (session.selected[0] || session.selected[2] != session.items[0].id ||
             !CurrentQuote().valid))
            throw std::runtime_error("Single soup did not use the side slot");
        if (scenario == "soup-specialty" &&
            (!CurrentQuote().valid || session.selected != session.specialty ||
             !IsBundleSelected(0)))
            throw std::runtime_error("Authored soup/drink/dessert combo changed");
        if ((scenario == "bundle-mouse-left" || scenario == "bundle-pad-left" ||
             scenario == "bundle-tab-return") && FirstVisibleBundle() == 0 &&
            (!IsBundleSelected(0) || session.dailyMode || session.price != 43))
            throw std::runtime_error("Whole specialty card did not select the full set");
        if ((scenario == "bundle-mouse-right" || scenario == "bundle-pad-right") &&
            (!IsBundleSelected(1) || !session.dailyMode || session.price != 22))
            throw std::runtime_error("Daily card lost its half-price set");
        if ((scenario == "bundle-mouse-left" || scenario == "bundle-pad-left" ||
             scenario == "bundle-tab-return") && FirstVisibleBundle() == 1 &&
            (!IsBundleSelected(1) || !session.dailyMode || session.price != 22))
            throw std::runtime_error("Single visible card did not retain the daily set and half price");
        if (scenario == "bundle-toggle" &&
            (session.selected != std::array<inn::Id, 3>{} || session.dailyMode))
            throw std::runtime_error("Second card selection did not cancel the full set");
        if (session.tab == 3 && cards.size() != 2 - FirstVisibleBundle())
            throw std::runtime_error("House page contains an unexpected set card");
        if (session.tab == 3 && FirstVisibleBundle() == 1)
        {
            if (session.bundleHighlighted != 1)
                throw std::runtime_error("Focus reached the hidden specialty card");
            checkLayout(std::abs(cards[0]->Pos.x + cards[0]->Size.x * .5f -
                (grid->Pos.x + grid->Size.x * .5f)) <= 1.f, "Single bundle card is not centered");
        }
        if (scenario == "single-card-empty" &&
            (QuoteBundle(1).valid || CurrentQuote().valid || !session.items.empty()))
            throw std::runtime_error("Empty stock created an orderable recommendation");
        if (scenario == "mouse-select" && session.selected[0] != session.items[1].id)
            throw std::runtime_error("Clicking card text did not select dish");
        if ((scenario == "wheel" || scenario == "scroll") && grid->Scroll.y <= 0)
            throw std::runtime_error("Grid did not scroll");
        if (scenario == "pad-down" && grid->Scroll.y <= 0)
            throw std::runtime_error("Gamepad focus did not scroll into view");
        DirectX::ScratchImage capture;
        Check(DirectX::CaptureTexture(previewDevice, previewContext, target, capture),
              "Capture preview");
        auto output = std::filesystem::path(Wide(argv[5]));
        Check(DirectX::SaveToWICFile(*capture.GetImage(0, 0, 0),
                                     DirectX::WIC_FLAGS_NONE,
                                     DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
                                     output.c_str()),
              "Save preview");
        D3D11_TEXTURE2D_DESC paperDesc = desc;
        paperDesc.Width = static_cast<UINT>(menu->Size.x);
        paperDesc.Height = static_cast<UINT>(menu->Size.y);
        paperDesc.BindFlags = 0;
        ID3D11Texture2D *paperTarget{};
        Check(previewDevice->CreateTexture2D(&paperDesc, nullptr, &paperTarget),
              "Create paper-only preview target");
        D3D11_BOX paperBox{static_cast<UINT>(menu->Pos.x),
                           static_cast<UINT>(menu->Pos.y),
                           0,
                           static_cast<UINT>(menu->Pos.x) + paperDesc.Width,
                           static_cast<UINT>(menu->Pos.y) + paperDesc.Height,
                           1};
        previewContext->CopySubresourceRegion(paperTarget, 0, 0, 0, 0, target, 0, &paperBox);
        DirectX::ScratchImage paperCapture;
        Check(DirectX::CaptureTexture(previewDevice, previewContext, paperTarget, paperCapture),
              "Capture paper-only preview");
        auto paperOutput = output.parent_path() / (output.stem().wstring() + L"-paper.png");
        Check(DirectX::SaveToWICFile(*paperCapture.GetImage(0, 0, 0),
                                     DirectX::WIC_FLAGS_NONE,
                                     DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
                                     paperOutput.c_str()),
              "Save paper-only preview");
        paperTarget->Release();
        auto report = output;
        report.replace_extension(L".txt");
        std::ofstream log(report);
        log << "Native ImGui 1.90.8 + WARP, same skin Draw code; scoped Chinese preview font: "
            << fontFile << "\n";
        const auto layoutName = layoutFile.u8string();
        log << "UI config: "
            << std::string(reinterpret_cast<const char *>(layoutName.data()), layoutName.size())
            << "; loaded=" << layoutResult.loaded << "; ignored=" << layoutResult.ignored
            << "; clamped=" << layoutResult.clamped << "\n";
        for (const auto &warning : layoutWarnings)
            log << "LAYOUT WARNING: " << warning << "\n";
        log << "CONFIGURED FONTS: inn=" << inn::uiLayout.innFont
            << " dish=" << inn::uiLayout.dishFont << " price=" << inn::uiLayout.priceFont
            << " controls=" << inn::uiLayout.controlsFont << "\n";
        log << "MENU TEXT: house=" << T("bundle_house_title")
            << "; daily=" << T("bundle_daily_title") << "; total=" << T("total")
            << "; cancel=" << T("cancel") << "\n";
        log << "INN PROMO: inn=" << session.innTextKey
            << "; text=" << inn::strings.InnText("bundle_price", session.innTextKey) << "\n";
        for (int index = 0; index < 2; ++index)
            log << "BUNDLE IMAGE " << index << ": " << session.bundleArt[index].path
                << "; loaded=" << (session.bundleArtTextures[index] != nullptr) << "\n";
        for (const auto &row : session.items)
            if (row.generated)
                log << "SPECIALTY FOOD IMAGE " << inn::RowKey(row) << ": " << row.image
                    << "; loaded=" << (row.texture != nullptr) << "\n";
        if (session.tab == 3)
            for (int index = FirstVisibleBundle(); index < 2; ++index)
            {
                const auto card = cards[index - FirstVisibleBundle()];
                const auto &dishes = BundleItems(index);
                std::vector<ImVec4> pictures;
                for (auto key : dishes)
                {
                    const auto row = std::find_if(session.items.begin(),
                                                  session.items.end(),
                                                  [key](const Item &item)
                                                  {
                                                      return inn::RowKey(item) == key;
                                                  });
                    if (row == session.items.end() || !row->texture)
                        continue;
                    const auto bounds = PreviewImageBounds(row->texture, card);
                    checkLayout(bounds.z > bounds.x && bounds.w > bounds.y,
                                "Meal picture has no drawn geometry");
                    checkLayout(bounds.x >= card->ClipRect.Min.x &&
                                    bounds.y >= card->ClipRect.Min.y &&
                                    bounds.z <= card->ClipRect.Max.x &&
                                    bounds.w <= card->ClipRect.Max.y,
                                "Meal picture clipped by its card");
                    for (const auto &other : pictures)
                        checkLayout(bounds.x >= other.z || bounds.z <= other.x ||
                                        bounds.y >= other.w || bounds.w <= other.y,
                                    "Meal pictures overlap each other");
                    pictures.push_back(bounds);
                    log << "MEAL PICTURE " << index << "/" << key << ": " << bounds.x << ","
                        << bounds.y << " " << bounds.z << "," << bounds.w
                        << "; alpha_crop=" << row->imageBounds.cropped << "\n";
                }
                log << "MEAL SPACING: card=" << index << "; pictures=" << pictures.size()
                    << "; no overlap or card clipping\n";
                const auto &names = previewMealNames[index];
                std::array<float, 3> centers{};
                for (int slot = 0; slot < 3; ++slot)
                {
                    if (!dishes[slot])
                        continue;
                    centers[slot] = (names[slot].y + names[slot].w) * .5f;
                    checkLayout(names[slot].w > names[slot].y, "Meal name has no drawn rectangle");
                    checkLayout(names[slot].y >= card->ClipRect.Min.y &&
                                    names[slot].w <= card->ClipRect.Max.y,
                                "Meal name clipped by its card");
                    if (slot && dishes[slot - 1])
                        checkLayout(names[slot].y >= names[slot - 1].w, "Meal names overlap");
                }
                const float firstPitch = centers[1] - centers[0],
                            secondPitch = centers[2] - centers[1];
                if (std::all_of(dishes.begin(), dishes.end(), [](auto id) { return id != 0; }))
                    checkLayout(firstPitch > 0 && std::abs(firstPitch - secondPitch) <= 1.f,
                                "Meal name rows have uneven center spacing");
                log << "NAME BALANCE: card=" << index << "; actual_center_pitch=" << firstPitch
                    << "," << secondPitch << "; equal within one pixel, no overlap or clipping\n";
            }
        log << "TITLE IMAGE: " << session.titleArt.path
            << "; loaded=" << (session.titleArtTexture != nullptr) << "\n";
        log << "TITLE HEIGHT: " << MenuTitleHeight() << "\n";
        if (session.titleArtTexture)
        {
            const auto bounds = PreviewImageBounds(session.titleArtTexture);
            const float scale = menu->Size.x / inn::uiLayout.windowWidth;
            const float centerError =
                std::abs((bounds.x + bounds.z) * .5f -
                         (menu->Pos.x + menu->Size.x * .5f + inn::uiLayout.innX * scale));
            const auto sign = PreviewImageBounds(session.bundleArtTextures[0]);
            const float signSide = std::max(sign.z - sign.x, sign.w - sign.y) / scale;
            log << "HEADING GEOMETRY: center_error=" << centerError
                << "; sign_max_side=" << signSide << "\n";
            if (inn::strings.IsChinese() &&
                (centerError > .1f || (!session.bundleArt[0].path.empty() &&
                 std::abs(signSide - inn::uiLayout.innSignSize) * scale > .5f)))
                throw std::runtime_error(
                    "Chinese title is not centered or configured inn sign size was not applied");
            if (!inn::strings.IsChinese())
            {
                const float height = MenuTitleHeight(), rowHeight = MenuInnTitleRowHeight(height);
                const float rowTop =
                    menu->Pos.y + (inn::uiLayout.paddingY + inn::uiLayout.innY) * scale;
                const float opticalCenter =
                    bounds.y + (bounds.w - bounds.y) * session.titleArt.bodyCenterY;
                const float expectedCenter = rowTop + rowHeight * .5f * scale;
                const float opticalError = std::abs(opticalCenter - expectedCenter);
                log << "ENGLISH TITLE BODY: center_error=" << opticalError
                    << "; normalized_center=" << session.titleArt.bodyCenterY << "\n";
                log << "ENGLISH TITLE CLEARANCE: row=" << rowTop << ","
                    << rowTop + rowHeight * scale << "; full_strokes=" << bounds.y << ","
                    << bounds.w << "\n";
                checkLayout(opticalError <= .1f,
                            "English text body is not vertically centered in the heading row");
                checkLayout(bounds.y >= rowTop - .1f &&
                                bounds.w <= rowTop + rowHeight * scale + .1f,
                            "English title ascenders or long tails leave the heading row");
            }
        }
        log << "Global style and both style stacks restored on all 4 frames.\n";
        log << "Menu within display; root footer has no vertical overflow.\n";
        log << "Menu: " << menu->Pos.x << "," << menu->Pos.y << " " << menu->Size.x << "x"
            << menu->Size.y << "\n";
        log << "Scenario: " << scenario << "; items: " << session.items.size() << "\n";
        log << "Card content fits; bundle cards: "
            << (session.tab == 3 ? cards.size() : 0) << "\n";
        log << "First visible bundle: " << FirstVisibleBundle()
            << "; bundle focus: " << session.bundleHighlighted << "\n";
        log << "Grid scroll: " << grid->Scroll.y << " / " << grid->ScrollMax.y
            << "; focused: " << session.highlighted << "\n";
        log << "Selected: " << session.selected[0] << "," << session.selected[1] << ","
            << session.selected[2] << "\n";
        log << "Grid bounds: " << grid->Pos.x << "," << grid->Pos.y << " " << grid->Size.x << "x"
            << grid->Size.y << "; clip: " << grid->ClipRect.Min.x << "," << grid->ClipRect.Min.y
            << " " << grid->ClipRect.Max.x << "," << grid->ClipRect.Max.y << "\n";
        for (const auto card : cards)
        {
            log << "Card: " << card->Pos.x << "," << card->Pos.y
                << "; clip: " << card->ClipRect.Min.x << "," << card->ClipRect.Min.y << " "
                << card->ClipRect.Max.x << "," << card->ClipRect.Max.y << "\n";
        }
        for (const auto list : ImGui::GetDrawData()->CmdLists)
        {
            log << "Draw list: " << list->_OwnerName << "\n";
            for (const auto &command : list->CmdBuffer)
            {
                log << " Command clip " << command.ClipRect.x << "," << command.ClipRect.y << " "
                    << command.ClipRect.z << "," << command.ClipRect.w << "; elements "
                    << command.ElemCount << "\n";
            }
        }
        std::cout << "PASS: " << scenario << " " << width << "x" << height
                  << "; global style restored; footer fits.\n";
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        for (auto [name, view] : textures)
            view->Release();
        targetView->Release();
        target->Release();
        previewContext->Release();
        previewDevice->Release();
        CoUninitialize();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
