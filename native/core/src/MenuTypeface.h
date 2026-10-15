#pragma once

// 中文阅读说明
// 只在中文点菜窗口内选择配置字体，检查字体接口和必要汉字。
// 阅读重点：每次绘制重新查找字体；Scope 进入时压入字体，退出时弹出，避免影响其他界面。

#include <cstring>
#include <string>
#include <string_view>

// The framework owns and rebuilds its atlas. Select an existing face only;
// never mutate the shared atlas or the framework's global named-font state.
namespace inn::menufont
{
    inline std::string name;
    inline bool reportedMissing{}, reportedReady{};

    struct Api
    {
        const char *(*version)(){};
        ImFont *(*current)(){};
        void (*push)(ImFont *){};
        void (*pop)(){};
        const char *(*debugName)(ImFont *){};
        const ImFontGlyph *(*glyph)(ImFont *, ImWchar){};
    };

    inline Api api;

    // 验证字体文件名，拒绝路径或过长名称。
    inline bool SetName(std::string value)
    {
        name.clear();
        reportedMissing = false;
        reportedReady = false;
        // ImGui's font debug name holds 40 bytes, including the size suffix.
        if (value.empty())
            return true;
        if (value.size() > 28 || value.find_first_of("/\\:") != std::string::npos ||
            value.find("..") != std::string::npos ||
            (!value.ends_with(".ttf") && !value.ends_with(".otf")))
            return false;
        name = std::move(value);
        return true;
    }

    // 同时检查字体函数是否齐全以及 ImGui 版本是否匹配。
    inline bool Compatible()
    {
        return api.version && api.current && api.push && api.pop && api.debugName && api.glyph &&
               std::string_view(api.version()) == "1.90.8";
    }

#ifdef PUAC_FONT_PREVIEW
    inline void Initialize(const std::string &requested)
    {
        SetName(requested);
        api = {ImGui::GetVersion,
               ImGui::GetFont,
               ImGui::PushFont,
               ImGui::PopFont,
               +[](ImFont *font)
               {
                   return font->GetDebugName();
               },
               +[](ImFont *font, ImWchar c)
               {
                   return font->FindGlyphNoFallback(c);
               }};
    }
#else
    inline void Initialize(HMODULE module)
    {
        char requested[96]{};
        const auto settings =
            std::filesystem::absolute("Data/SKSE/Plugins/PullUpAChairOrdering.ini").string();
        GetPrivateProfileStringA(
            "Menu", "ChineseFont", "", requested, sizeof(requested), settings.c_str());
        if (!SetName(requested))
            logger::warn("Chinese menu font: invalid filename; using framework default");
        api.version =
            reinterpret_cast<decltype(api.version)>(GetProcAddress(module, "igGetVersion"));
        api.current = reinterpret_cast<decltype(api.current)>(GetProcAddress(module, "igGetFont"));
        api.push = reinterpret_cast<decltype(api.push)>(GetProcAddress(module, "igPushFont"));
        api.pop = reinterpret_cast<decltype(api.pop)>(GetProcAddress(module, "igPopFont"));
        api.debugName = reinterpret_cast<decltype(api.debugName)>(
            GetProcAddress(module, "ImFont_GetDebugName"));
        api.glyph = reinterpret_cast<decltype(api.glyph)>(
            GetProcAddress(module, "ImFont_FindGlyphNoFallback"));
        if (!Compatible())
        {
            logger::warn("Chinese menu font: unsupported font API/layout; using framework default");
            api = {};
        }
        logger::info(
            "Chinese menu font revision=1: file='{}'; api_ready={}; Chinese ordering window only",
            name,
            Compatible());
    }
#endif

    // 在当前字体图集中寻找指定字体，并检查菜单需要的代表汉字。
    inline ImFont *Resolve(ImGuiIO *io)
    {
        if (!io || !io->Fonts)
            return nullptr;
        // Resolve on every draw so a framework atlas rebuild cannot leave a stale
        // cached ImFont pointer behind. There are only a few faces in this atlas.
        const auto &fonts = io->Fonts->Fonts;
        for (int index = 0; index < fonts.Size; ++index)
        {
            auto font = fonts.Data[index];
            if (!font)
                continue;
            const auto debug = api.debugName(font);
            if (!debug)
                continue;
            const std::string_view face(debug);
            if (!face.starts_with(name) || (face.size() > name.size() && face[name.size()] != ','))
                continue;
            // A face loaded without the Chinese glyph range must not replace the
            // already working default. Check representative menu glyphs first.
            if (!api.glyph(font, 0x83DC) || !api.glyph(font, 0x786E) || !api.glyph(font, 0x6D53))
                return nullptr;
            return font;
        }
        return nullptr;
    }

    // 在中文菜单绘制期间临时使用字体，析构时恢复之前的字体。
    class Scope
    {
        bool pushed_{};

      public:
        // 在中文菜单绘制期间临时使用字体，析构时恢复之前的字体。
        explicit Scope(bool chinese, ImGuiIO *io)
        {
            if (!chinese || name.empty() || !Compatible())
                return;
            const auto font = Resolve(io);
            if (!font)
            {
                if (!reportedMissing)
                {
                    logger::warn(
                        "Chinese menu font: face '{}' unavailable or lacks Chinese glyphs; keeping framework default",
                        name);
                    reportedMissing = true;
                }
                return;
            }
            api.push(font);
            pushed_ = true;
            if (!reportedReady)
            {
                logger::info(
                    "Chinese menu font selected: {}; scoped push/pop; Chinese glyph checks passed",
                    api.debugName(font));
                reportedReady = true;
            }
        }

        // 在中文菜单绘制期间临时使用字体，析构时恢复之前的字体。
        ~Scope()
        {
            if (pushed_)
                api.pop();
        }

        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
    };
}
