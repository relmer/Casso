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
            DebuggerTextColors::Set  colors = DebuggerTextColors::Make (bg, theme.Foreground(), theme.ForegroundMuted(), theme.resultText, theme.Accent(), theme.changedText);

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
