#include "Pch.h"
#include "Core/TextEncoding.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColorsTests
//
//  Every text color the debugger draws reaches the WCAG AA ratio for text
//  against the content background, in every theme the debugger offers.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerTextColorsTests)
{
public:

    static void  CheckRatio (const std::string & theme, const wchar_t * role, uint32_t argb, uint32_t background, std::wstring & failures)
    {
        float  ratio = DxuiColor::ComputeContrastRatio (argb, background);



        if (ratio < DebuggerTextColors::s_kMinTextContrast)
        {
            failures += std::format (L"{} {}: {:08X} on {:08X} is {:.2f}:1\n", TextEncoding::NarrowToWide (theme), role, argb, background, ratio);
        }
    }



    TEST_METHOD (EveryTextColorMeetsAaInEveryTheme)
    {
        CassoTheme      emulator = CassoTheme::MakeSkeuomorphic();
        CassoTheme      own;
        DxuiLightTheme  light;
        DxuiDarkTheme   dark;
        std::wstring    failures;



        for (const DebuggerThemes::Choice & choice : DebuggerThemes::GetChoices())
        {
            const DxuiTheme &        theme  = DebuggerThemes::Choose (choice.name, emulator, light, dark, own);
            uint32_t                 bg     = theme.ContentBackground();
            DebuggerTextColors::Set  colors = DebuggerTextColors::Make (bg, theme.Foreground(), theme.ForegroundMuted(), theme.resultText, theme.Accent(), theme.changedText, theme.romText, theme.ioText);

            CheckRatio (choice.name, L"ROM",        colors.rom,              bg, failures);
            CheckRatio (choice.name, L"I/O",        colors.io,               bg, failures);
            CheckRatio (choice.name, L"mnemonic",   colors.syntax.mnemonic,  bg, failures);
            CheckRatio (choice.name, L"directive",  colors.syntax.directive, bg, failures);
            CheckRatio (choice.name, L"symbol",     colors.syntax.symbol,    bg, failures);
            CheckRatio (choice.name, L"number",     colors.syntax.number,    bg, failures);
            CheckRatio (choice.name, L"string",     colors.syntax.string,    bg, failures);
            CheckRatio (choice.name, L"comment",    colors.syntax.comment,   bg, failures);
            CheckRatio (choice.name, L"address",    colors.syntax.address,   bg, failures);
            CheckRatio (choice.name, L"bytes",      colors.syntax.bytes,     bg, failures);
            CheckRatio (choice.name, L"annotation", colors.annotation,       bg, failures);
            CheckRatio (choice.name, L"changed",    colors.changed,          bg, failures);
            CheckRatio (choice.name, L"result",     colors.result,           bg, failures);
            CheckRatio (choice.name, L"muted",      colors.muted,            bg, failures);
        }

        Logger::WriteMessage (failures.c_str());
        Assert::IsTrue (failures.empty(), failures.c_str());
    }



    //  Every theme's own text colors reach the WCAG AA ratio on each surface
    //  they are drawn on: body, muted, error and heading text on the panels
    //  and lists, and each control's text on its own fill. Disabled text is
    //  exempt, as WCAG exempts it. A hovered or selected row's fill is not
    //  here: the list and the menu move their text until it reads on it,
    //  whatever the theme gives.
    TEST_METHOD (EveryThemesTextMeetsAaOnItsSurfaces)
    {
        CassoTheme                     skeuo    = CassoTheme::MakeSkeuomorphic();
        CassoTheme                     modern   = CassoTheme::MakeDarkModern();
        CassoTheme                     retro    = CassoTheme::MakeRetroTerminal();
        DxuiLightTheme                 light;
        DxuiDarkTheme                  dark;
        std::wstring                   failures;
        const std::pair<std::string, const DxuiTheme *>  themes[] =
        {
            { "Skeuomorphic", &skeuo }, { "Dark modern", &modern }, { "Retro terminal", &retro }, { "Light", &light }, { "Dark", &dark },
        };



        for (const auto & [name, theme] : themes)
        {
            for (const auto & [surface, background] : { std::pair { L"panel",    theme->Background()          },
                                                        std::pair { L"content",  theme->ContentBackground()   },
                                                        std::pair { L"elevated", theme->BackgroundElevated()  },

                                                        std::pair { L"status",   theme->StatusBackground()    } })
            {
                CheckRatio (name, std::format (L"text on {}",    surface).c_str(), theme->Foreground(),        background, failures);
                CheckRatio (name, std::format (L"muted on {}",   surface).c_str(), theme->ForegroundMuted(),   background, failures);
                CheckRatio (name, std::format (L"heading on {}", surface).c_str(), theme->HeadingForeground(), background, failures);
                CheckRatio (name, std::format (L"error on {}",   surface).c_str(), theme->ErrorForeground(),   background, failures);
            }


            CheckRatio (name, L"button idle",        theme->ButtonText(),           theme->ButtonIdle(),           failures);
            CheckRatio (name, L"button hover",       theme->ButtonText(),           theme->ButtonHover(),          failures);
            CheckRatio (name, L"button pressed",     theme->ButtonText(),           theme->ButtonPressed(),        failures);
            CheckRatio (name, L"caption",            theme->CaptionForeground(),    theme->CaptionBackground(),    failures);
            CheckRatio (name, L"tooltip",            theme->TooltipForeground(),    theme->TooltipBackground(),    failures);
            CheckRatio (name, L"info banner",        theme->InfoBannerForeground(), DxuiColor::Composite (theme->InfoBannerBackground(), theme->Background()), failures);
        }

        Logger::WriteMessage (failures.c_str());
        Assert::IsTrue (failures.empty(), failures.c_str());
    }



    using TooltipCheckFn = std::function<void (const wchar_t * name, const DxuiTheme & theme, uint32_t fill, uint32_t border)>;

    //  Runs `check` over every theme's tooltip with the fill and border it is
    //  expected to have: the five themes the debugger and the emulator offer,
    //  and the starting sets DxuiTheme gives a host.
    static void  CheckEveryTooltip (const TooltipCheckFn & check)
    {
        CassoTheme      skeuo     = CassoTheme::MakeSkeuomorphic();
        CassoTheme      modern    = CassoTheme::MakeDarkModern();
        CassoTheme      retro     = CassoTheme::MakeRetroTerminal();
        DxuiDarkTheme   dark;
        DxuiLightTheme  light;
        DxuiTheme       baseDark  = DxuiTheme::Dark();
        DxuiTheme       baseLight = DxuiTheme::Light();



        check (L"Skeuomorphic",   skeuo,     0xFF1A2230u, 0xFF0D1118u);
        check (L"Dark modern",    modern,    0xFF1E2024u, 0xFF0F1012u);
        check (L"Retro terminal", retro,     0xFF0E2612u, 0xFF071309u);
        check (L"System dark",    dark,      0xFF272727u, 0xFF131313u);
        check (L"System light",   light,     0xFFFBFBFBu, 0xFF7D7D7Du);
        check (L"Dxui dark",      baseDark,  0xFF1E2024u, 0xFF0F1012u);
        check (L"Dxui light",     baseLight, 0xFFF6F6F6u, 0xFF7B7B7Bu);
    }



    //  A tooltip takes the window background, so it never shows brighter
    //  than the window it pops up over.
    TEST_METHOD (TooltipIsNoBrighterThanTheWindowInEveryTheme)
    {
        CheckEveryTooltip ([] (const wchar_t * name, const DxuiTheme & theme, uint32_t fill, uint32_t border)
        {
            float  tipLuminance    = DxuiColor::ComputeRelativeLuminance (theme.TooltipBackground());
            float  windowLuminance = DxuiColor::ComputeRelativeLuminance (theme.Background());



            (void) border;

            Assert::AreEqual (fill,               theme.TooltipBackground(), name);
            Assert::AreEqual (theme.Background(), theme.TooltipBackground(), name);
            Assert::IsTrue   (tipLuminance <= windowLuminance,               name);
        });
    }



    //  A tooltip's border is its fill at half the brightness, channel by
    //  channel, which sets it off from the window under it.
    TEST_METHOD (TooltipBorderIsHalfTheTooltipsBrightness)
    {
        CheckEveryTooltip ([] (const wchar_t * name, const DxuiTheme & theme, uint32_t fill, uint32_t border)
        {
            constexpr int  kChannelShifts[] = { 16, 8, 0 };

            uint32_t  tip  = theme.TooltipBackground();
            uint32_t  edge = theme.TooltipBorder();



            (void) fill;

            Assert::AreEqual (border, edge, name);

            for (int shift : kChannelShifts)
            {
                Assert::AreEqual (((tip >> shift) & 0xFFu) / 2, (edge >> shift) & 0xFFu, name);
            }
        });
    }



    //  A memory window's ROM and I/O colors are each theme's own: the
    //  Skeuomorphic theme's light blue ROM reads as it is, so it is unmoved,
    //  and a theme without them gets colors for its darkness.
    TEST_METHOD (RomAndIoColorsComeFromTheTheme)
    {
        CassoTheme               skeuo  = CassoTheme::MakeSkeuomorphic();
        DxuiLightTheme           light;
        DebuggerTextColors::Set  colors;



        Assert::AreEqual (0xFF7FB2E5u, skeuo.romText, L"the Skeuomorphic theme's ROM blue");
        Assert::AreNotEqual (0u, skeuo.ioText, L"and an I/O color of its own");

        colors = DebuggerTextColors::Make (skeuo.ContentBackground(), skeuo.Foreground(), skeuo.ForegroundMuted(), skeuo.resultText, skeuo.Accent(), skeuo.changedText, skeuo.romText, skeuo.ioText);
        Assert::AreEqual (skeuo.romText, colors.rom, L"unmoved, since it reads on the page");

        colors = DebuggerTextColors::Make (light.ContentBackground(), light.Foreground(), light.ForegroundMuted(), light.resultText, light.Accent(), light.changedText, light.romText, light.ioText);
        Assert::AreNotEqual (0u, colors.rom, L"a theme without one still gets a ROM color");
        Assert::AreNotEqual (0u, colors.io,  L"and an I/O color");
        Assert::AreNotEqual (colors.rom, colors.io, L"the two differ");
    }



    //  The kept colors are MakeFor's, and a theme color changed in place
    //  makes them again.
    TEST_METHOD (CacheFollowsAThemeEditedInPlace)
    {
        constexpr uint32_t  kOtherRom = 0xFF2060C0;

        CassoTheme                  theme = CassoTheme::MakeSkeuomorphic();
        DebuggerTextColors::Cache   cache;



        Assert::AreEqual (DebuggerTextColors::MakeFor (theme).rom, cache.GetFor (theme).rom, L"MakeFor's colors");

        theme.romText = kOtherRom;

        Assert::AreEqual (DebuggerTextColors::MakeFor (theme).rom, cache.GetFor (theme).rom, L"made again for the new ROM color");
        Assert::AreNotEqual (DebuggerTextColors::MakeFor (CassoTheme::MakeSkeuomorphic()).rom, cache.GetFor (theme).rom, L"and not the old one");
    }



    TEST_METHOD (ReadableColorIsLeftAlone)
    {
        Assert::AreEqual (0xFF0000FFu, DebuggerTextColors::GetReadable (0xFF0000FF, 0xFFFFFFFF));
    }



    TEST_METHOD (FaintColorMovesTowardBlackOnLight)
    {
        uint32_t  fixedColor = DebuggerTextColors::GetReadable (0xFF2B91AF, 0xFFFFFFFF);



        Assert::IsTrue (DxuiColor::ComputeContrastRatio (fixedColor, 0xFFFFFFFF) >= DebuggerTextColors::s_kMinTextContrast);
        Assert::IsTrue (DxuiColor::ComputeRelativeLuminance (fixedColor) < DxuiColor::ComputeRelativeLuminance (0xFF2B91AF));
    }
};
