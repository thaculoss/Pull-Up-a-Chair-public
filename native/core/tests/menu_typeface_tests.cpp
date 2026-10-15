// 中文阅读说明
// 离线检查：中文字体选择、缺字回退和绘制结束后的字体栈恢复。
// 阅读重点：这些检查验证数据和规则；通过检查不代表游戏内行为已验证。

#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace logger
{
    template <class... Args> void info(const char *, Args &&...)
    {
    }

    template <class... Args> void warn(const char *, Args &&...)
    {
    }
}

#define PUAC_FONT_PREVIEW
#include "MenuTypeface.h"

// 测试断言：条件不成立时报告该检查失败。
void Require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// 测试程序入口：依次检查各个场景，任一检查失败就报告错误。
int main(int argc, char **argv)
{
    try
    {
        Require(argc == 2, "font argument missing");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1920, 1080};
        io.DeltaTime = 1.f / 60;
        auto load = [&](bool chinese)
        {
            io.Fonts->Clear();
            io.FontDefault = io.Fonts->AddFontDefault();
            ImFontConfig config;
            config.PixelSnapH = true;
            config.OversampleH = 1;
            config.OversampleV = 1;
            io.Fonts->TexDesiredWidth = 16384;
            io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
            auto face = io.Fonts->AddFontFromFileTTF(argv[1],
                                                     32,
                                                     &config,
                                                     chinese ? io.Fonts->GetGlyphRangesChineseFull()
                                                             : io.Fonts->GetGlyphRangesDefault());
            Require(face && io.Fonts->Build(), "font atlas failed");
            return face;
        };
        auto face = load(true);
        inn::menufont::Initialize("PUAC-WenKai-Medium.ttf");
        ImGui::NewFrame();
        const auto original = ImGui::GetFont();
        const auto depth = ImGui::GetCurrentContext()->FontStack.Size;
        {
            inn::menufont::Scope scope(false, &io);
            Require(ImGui::GetFont() == original, "English font changed");
        }
        Require(ImGui::GetCurrentContext()->FontStack.Size == depth, "English stack changed");
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == face, "Chinese font not selected");
        }
        Require(ImGui::GetFont() == original && ImGui::GetCurrentContext()->FontStack.Size == depth,
                "font scope leaked");
        ImGui::PushFont(face);
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == face, "nested font changed");
        }
        Require(ImGui::GetCurrentContext()->FontStack.Size == depth + 1,
                "nested font scope unbalanced");
        ImGui::PopFont();
        inn::menufont::SetName("Missing.ttf");
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == original, "missing font lost fallback");
        }
        Require(ImGui::GetCurrentContext()->FontStack.Size == depth,
                "missing font popped another scope");
        inn::menufont::SetName("");
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == original, "disabled font changed");
        }
        Require(!inn::menufont::SetName("../bad.ttf"), "unsafe filename accepted");
        inn::menufont::SetName("PUAC-WenKai-Medium.ttf");
        ImGui::EndFrame();
        face = load(false);
        ImGui::NewFrame();
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == io.FontDefault,
                    "Latin-only face replaced Chinese fallback");
        }
        ImGui::EndFrame();
        face = load(true);
        ImGui::NewFrame();
        {
            inn::menufont::Scope scope(true, &io);
            Require(ImGui::GetFont() == face, "rebuilt atlas font not resolved");
        }
        Require(ImGui::GetFont() == io.FontDefault, "rebuilt font scope leaked");
        ImGui::EndFrame();
        ImGui::DestroyContext();
        std::cout
            << "PASS: Chinese selection, English isolation, nested stack, missing/disabled/Latin-only fallback, atlas rebuild\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
