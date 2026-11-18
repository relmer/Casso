#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarVectorIconTests
//
//  An entry's vector icon is filled through the text renderer, in the entry's
//  ink and an accent. An entry can choose the icon and the accent at paint
//  time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarVectorIconTests)
{
    static constexpr uint32_t             s_kGreen       = 0xFF6CCB5Fu;
    static constexpr DxuiVectorIconLayer  s_kLayers[]    = { { 0x01u, true, 0.0f, 0.0f } };
    static constexpr DxuiVectorIcon       s_kFirst       = { L"M0 0H20V20H0Z", 20.0f, s_kLayers, std::size (s_kLayers) };
    static constexpr DxuiVectorIcon       s_kSecond      = { L"M5 5H15V15H5Z", 20.0f, s_kLayers, std::size (s_kLayers) };


    //  Paints a bar of the one entry and returns the vector icon calls.
    static std::vector<RecordedVectorIconCall> PaintOne (DxuiToolbar::Entry entry, bool enabled)
    {
        DxuiToolbar                      bar;
        DxuiDpiScaler                    scaler;
        MockDxuiTextRenderer             text;
        MockDxuiPainter                  painter;
        MockDxuiTheme                    theme;
        std::vector<DxuiToolbar::Entry>  entries;
        auto                             command = std::make_shared<DxuiCommand>();

        command->id        = 1;
        command->label     = L"Run";
        command->isEnabled = [enabled] { return enabled; };

        entry.command  = command;
        entry.iconOnly = true;
        entries.push_back (std::move (entry));

        bar.SetTextRenderer (&text);
        bar.SetEntries      (std::move (entries));
        bar.Layout          (RECT { 0, 0, 400, 40 }, scaler);
        bar.Paint           (painter, text, theme);

        return text.VectorIconCalls();
    }


public:

    TEST_METHOD (AnEntrysVectorIconIsFilledInTheThemesAccent)
    {
        DxuiToolbar::Entry                   entry;
        MockDxuiTheme                        theme;
        std::vector<RecordedVectorIconCall>  calls;

        entry.vectorIcon = &s_kFirst;
        calls            = PaintOne (entry, true);

        Assert::AreEqual (size_t (1), calls.size());
        Assert::IsTrue   (calls[0].icon == &s_kFirst);
        Assert::AreEqual (theme.Accent(), calls[0].accent);
        Assert::IsTrue   (calls[0].size > 0.0f);
    }


    TEST_METHOD (TheIconIsChosenAtPaintTime)
    {
        DxuiToolbar::Entry                   entry;
        bool                                 second = false;
        std::vector<RecordedVectorIconCall>  calls;

        entry.getVectorIcon = [&second] { return second ? &s_kSecond : &s_kFirst; };

        calls = PaintOne (entry, true);
        Assert::AreEqual (size_t (1), calls.size());
        Assert::IsTrue   (calls[0].icon == &s_kFirst);

        second = true;
        calls  = PaintOne (entry, true);
        Assert::AreEqual (size_t (1), calls.size());
        Assert::IsTrue   (calls[0].icon == &s_kSecond);
    }


    TEST_METHOD (GetAccentOverridesTheThemeAccent)
    {
        DxuiToolbar::Entry                   entry;
        std::vector<RecordedVectorIconCall>  calls;

        entry.vectorIcon = &s_kFirst;
        entry.getAccent  = [] { return s_kGreen; };
        calls            = PaintOne (entry, true);

        Assert::AreEqual (size_t (1), calls.size());
        Assert::AreEqual (s_kGreen, calls[0].accent);
    }


    //  A disabled entry's colored parts are grayed with the rest of the icon.
    TEST_METHOD (ADisabledEntrysAccentIsItsDimmedInk)
    {
        DxuiToolbar::Entry                   entry;
        std::vector<RecordedVectorIconCall>  enabled;
        std::vector<RecordedVectorIconCall>  disabled;

        entry.vectorIcon = &s_kFirst;
        entry.getAccent  = [] { return s_kGreen; };
        enabled          = PaintOne (entry, true);
        disabled         = PaintOne (entry, false);

        Assert::AreEqual (size_t (1), disabled.size());
        Assert::AreEqual (disabled[0].foreground, disabled[0].accent);
        Assert::AreNotEqual (enabled[0].foreground, disabled[0].foreground);
    }
};
