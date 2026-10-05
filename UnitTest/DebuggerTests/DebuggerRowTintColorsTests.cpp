#include "Pch.h"
#include "Core/TextEncoding.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerRowTintColorsTests
//
//  The disassembly's text reads on the PC's row, a branch's destination and a
//  navigated row in every theme, and the colors a reader must tell apart are
//  apart.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerRowTintColorsTests)
{
public:

    static DebuggerTextColors::Set  MakeFor (const DxuiTheme & theme)
    {
        return DebuggerTextColors::Make (theme.ContentBackground(), theme.Foreground(), theme.ForegroundMuted(), theme.resultText, theme.Accent(), theme.changedText);
    }



    //  The changed color as it was before the row fills were made opaque: a
    //  coral on a dark page and the red on a light one, each read against the
    //  page alone, or a light page's darkest row shade.
    static uint32_t  GetEarlierChanged (const DxuiTheme & theme)
    {
        uint32_t  background = theme.ContentBackground();
        bool      dark       = DebuggerTextColors::IsDark (background);



        return DebuggerTextColors::GetReadable (dark ? 0xFFFF6B68 : 0xFFD00000, dark ? background : DebuggerTextColors::GetRowShade (background));
    }



    static float  GetSaturation (uint32_t argb)
    {
        float  r   = (float) ((argb >> 16) & 0xFF) / 255.0f;
        float  g   = (float) ((argb >> 8)  & 0xFF) / 255.0f;
        float  b   = (float) ( argb        & 0xFF) / 255.0f;
        float  max = (std::max) ({ r, g, b });
        float  min = (std::min) ({ r, g, b });



        return (max > 0) ? (max - min) / max : 0.0f;
    }



    static void  CheckRatio (const std::string & theme, const wchar_t * role, uint32_t argb, uint32_t background, std::wstring & failures)
    {
        float  ratio = DxuiColor::ComputeContrastRatio (argb, background);



        if (ratio < DebuggerTextColors::s_kMinTextContrast)
        {
            failures += std::format (L"{} {}: {:08X} on {:08X} is {:.2f}:1\n", TextEncoding::NarrowToWide (theme), role, argb, background, ratio);
        }
    }



    static float  GetHue (uint32_t argb)
    {
        float  r   = (float) ((argb >> 16) & 0xFF) / 255.0f;
        float  g   = (float) ((argb >> 8)  & 0xFF) / 255.0f;
        float  b   = (float) ( argb        & 0xFF) / 255.0f;
        float  max = (std::max) ({ r, g, b });
        float  min = (std::min) ({ r, g, b });
        float  d   = max - min;
        float  h   = 0;



        if (d > 0)
        {
            h = (max == r) ? std::fmod ((g - b) / d + 6.0f, 6.0f) : (max == g) ? (b - r) / d + 2.0f : (r - g) / d + 4.0f;
        }

        return h * 60.0f;
    }



    static float  GetHueDistance (float a, float b)
    {
        float  apart = std::fabs (a - b);



        return (apart > 180.0f) ? 360.0f - apart : apart;
    }



    TEST_METHOD (EveryTextColorReadsOnEveryRowTint)
    {
        CassoTheme      emulator = CassoTheme::MakeSkeuomorphic();
        CassoTheme      own;
        DxuiLightTheme  light;
        DxuiDarkTheme   dark;
        std::wstring    failures;



        for (const DebuggerThemes::Choice & choice : DebuggerThemes::GetChoices())
        {
            DebuggerTextColors::Set  c = MakeFor (DebuggerThemes::Choose (choice.name, emulator, light, dark, own));

            for (uint32_t row : { c.pcRow, c.targetRow, c.navigatedRow })
            {
                CheckRatio (choice.name, L"mnemonic",  c.syntax.mnemonic, row, failures);
                CheckRatio (choice.name, L"symbol",    c.syntax.symbol,   row, failures);
                CheckRatio (choice.name, L"number",    c.syntax.number,   row, failures);
                CheckRatio (choice.name, L"address",   c.syntax.address,  row, failures);
                CheckRatio (choice.name, L"bytes",     c.syntax.bytes,    row, failures);
                CheckRatio (choice.name, L"operand",   c.operandAddress,  row, failures);
                CheckRatio (choice.name, L"annotation", c.annotation,     row, failures);
                CheckRatio (choice.name, L"result",    c.result,          row, failures);
                CheckRatio (choice.name, L"changed",   DebuggerTextColors::GetChangedOn (c, row), row, failures);
            }
        }

        Logger::WriteMessage (failures.c_str());
        Assert::IsTrue (failures.empty(), failures.c_str());
    }



    TEST_METHOD (RowTintsAreOpaque)
    {
        DxuiDarkTheme            dark;
        DebuggerTextColors::Set  c = MakeFor (dark);



        Assert::AreEqual (0xFFu, c.pcRow >> 24);
        Assert::AreEqual (0xFFu, c.targetRow >> 24);
        Assert::AreEqual (0xFFu, c.navigatedRow >> 24);
    }



    TEST_METHOD (LightOperandAddressDiffersFromImmediate)
    {
        DxuiLightTheme           light;
        DebuggerTextColors::Set  c = MakeFor (light);



        Assert::AreNotEqual (0u, c.operandAddress);
        Assert::IsTrue (GetHueDistance (GetHue (c.operandAddress), GetHue (c.syntax.number)) >= 60.0f);
    }



    TEST_METHOD (LightResultDiffersFromAnnotation)
    {
        DxuiLightTheme           light;
        DebuggerTextColors::Set  c = MakeFor (light);



        Assert::IsTrue (GetHueDistance (GetHue (c.result), GetHue (c.annotation)) >= 90.0f);
    }



    TEST_METHOD (LightPcRowIsDarkYellow)
    {
        DxuiLightTheme           light;
        DebuggerTextColors::Set  c   = MakeFor (light);
        float                    hue = GetHue (c.pcRow);



        Assert::IsTrue (hue >= 35.0f && hue <= 55.0f);
        Assert::IsTrue (DxuiColor::ComputeRelativeLuminance (c.pcRow) < 0.6f);
    }



    TEST_METHOD (DarkChangedIsBrightOnTheDarkPage)
    {
        DxuiDarkTheme            dark;
        DebuggerTextColors::Set  c = MakeFor (dark);



        Assert::IsTrue (DxuiColor::ComputeContrastRatio (c.changed, dark.ContentBackground()) >= 7.0f);
    }


    TEST_METHOD (SkeuomorphicChangedIsARedderRedThanBefore)
    {
        CassoTheme               skeuo  = CassoTheme::MakeSkeuomorphic();
        DebuggerTextColors::Set  c      = MakeFor (skeuo);
        uint32_t                 before = GetEarlierChanged (skeuo);



        Assert::AreEqual (skeuo.changedText, c.changed, L"the theme's own red, unmoved, since it reads on the page");
        Assert::IsTrue   (GetSaturation (c.changed) >= GetSaturation (before) + 0.1f, L"noticeably more saturated than before");
        Assert::IsTrue   (GetSaturation (c.changed) < 0.9f, L"but not fully");
        Assert::IsTrue   (GetHueDistance (GetHue (c.changed), 0.0f) <= GetHueDistance (GetHue (before), 0.0f), L"and no farther from red");
        Assert::IsTrue   (DxuiColor::ComputeContrastRatio (c.changed, skeuo.ContentBackground()) >= DebuggerTextColors::s_kMinTextContrast);
    }


    TEST_METHOD (OtherThemesKeepTheirEarlierChangedColor)
    {
        CassoTheme      darkModern = CassoTheme::MakeDarkModern();
        CassoTheme      retro      = CassoTheme::MakeRetroTerminal();
        DxuiLightTheme  light;



        Assert::AreEqual (GetEarlierChanged (darkModern), MakeFor (darkModern).changed, L"Dark Modern");
        Assert::AreEqual (GetEarlierChanged (retro),      MakeFor (retro).changed,      L"Retro Terminal");
        Assert::AreEqual (GetEarlierChanged (light),      MakeFor (light).changed,      L"System light");
    }


    TEST_METHOD (ChangedIsLiftedOnARowFillOnly)
    {
        CassoTheme               skeuo = CassoTheme::MakeSkeuomorphic();
        DebuggerTextColors::Set  c     = MakeFor (skeuo);



        Assert::AreEqual    (c.changed, DebuggerTextColors::GetChangedOn (c, 0),       L"no fill: the page color");
        Assert::AreNotEqual (c.changed, DebuggerTextColors::GetChangedOn (c, c.pcRow), L"lifted to read on the PC row");
    }


    TEST_METHOD (ImmediateAndOperandAddressGetTheirOwnColors)
    {
        DxuiLightTheme           light;
        DebuggerTextColors::Set  c          = MakeFor (light);
        auto                     immediate  = DebuggerTextColors::GetInstructionRanges (L"LDA #$40", c);
        auto                     address    = DebuggerTextColors::GetInstructionRanges (L"LDA $C000", c);



        Assert::AreEqual (2, (int) immediate.size());
        Assert::AreEqual (c.syntax.mnemonic, std::get<2> (immediate[0]));
        Assert::AreEqual (c.syntax.number,   std::get<2> (immediate[1]));
        Assert::AreEqual (c.operandAddress,  std::get<2> (address[1]));
    }



    TEST_METHOD (TraceRowIsColoredAsTheDisassembly)
    {
        DxuiDarkTheme                    dark;
        DebuggerTextColors::Set          c   = MakeFor (dark);
        std::vector<DxuiListView::Cell>  row (8);



        row[2].text = L"C28B";
        row[5].text = L"LDA KBD";

        TracePane::AddColors (row, c);

        Assert::AreEqual (c.syntax.address, row[2].argb);
        Assert::AreEqual (c.syntax.symbol,  row[4].argb);
        Assert::IsFalse  (row[5].colorRanges.empty());
        Assert::AreEqual (c.syntax.mnemonic, std::get<2> (row[5].colorRanges[0]));
    }



    TEST_METHOD (CallStackFrameIsColoredButDimRowIsNot)
    {
        DxuiDarkTheme            dark;
        DebuggerTextColors::Set  c = MakeFor (dark);
        CallStackPane::Row       frame;
        CallStackPane::Row       dim;



        frame.site    = L"$C2A0";
        frame.routine = L"JSR $C152";
        dim           = frame;
        dim.isDim     = true;

        Assert::AreEqual (c.syntax.mnemonic, std::get<2> (CallStackPane::GetCells (frame, c)[1].colorRanges[0]));
        Assert::IsTrue   (CallStackPane::GetCells (dim, c)[1].colorRanges.empty());
    }
};
