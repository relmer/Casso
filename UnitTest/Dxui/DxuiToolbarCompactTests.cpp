#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarCompactTests
//
//  A compact strip is a tool window's, as Visual Studio draws one inside a
//  pane: a 26 DIP band, 12 DIP icons in buttons 4 DIPs wider each side, 2
//  DIPs of air above and below, and 1 DIP between neighbors. In a pane, the
//  strip can start its first entry where that entry's first ink lines up
//  with the pane's title.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarCompactTests)
{
public:

    static std::vector<DxuiToolbar::Entry> MakeEntries (int count)
    {
        std::vector<DxuiToolbar::Entry>  entries ((size_t) count);

        for (int i = 0; i < count; i++)
        {
            auto  command = std::make_shared<DxuiCommand>();

            command->id    = i + 1;
            command->label = L"Command";
            command->glyph = L"x";

            entries[(size_t) i].command  = command;
            entries[(size_t) i].iconOnly = true;
        }

        return entries;
    }


    TEST_METHOD (ACompactStripHasSmallButtonsInAThinBand)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first  = {};
        RECT           second = {};

        scaler.SetDpi (96);
        bar.SetCompact (true);
        bar.SetEntries (MakeEntries (2));
        bar.Layout     (RECT { 0, 0, 400, DxuiToolbar::kCompactBandDp }, scaler);

        Assert::AreEqual (26, bar.GetBandDp(), L"the band");
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::IsTrue   (bar.TryGetEntryRect (2, second));
        Assert::AreEqual (2L,  first.top,                   L"2 DIPs above");
        Assert::AreEqual (22L, first.bottom - first.top,    L"a 22 DIP button");
        Assert::AreEqual (20L, first.right  - first.left,   L"a 12 DIP icon with 4 each side");
        Assert::AreEqual (2L,  first.left,                  L"2 DIPs in from the edge");
        Assert::AreEqual (1L,  second.left - first.right,   L"1 DIP between neighbors");
    }


    TEST_METHOD (TheCommandBarKeepsItsOwnSpacing)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;
        RECT           first = {};

        scaler.SetDpi (96);
        bar.SetEntries (MakeEntries (1));
        bar.Layout     (RECT { 0, 0, 400, 42 }, scaler);

        Assert::AreEqual (42, bar.GetBandDp());
        Assert::IsTrue   (bar.TryGetEntryRect (1, first));
        Assert::AreEqual (35L, first.right - first.left);
    }


    //  A custom entry that draws one string, its lead in from its left.
    class LeadEntry : public IDxuiToolbarCustomEntry
    {
    public:
        static constexpr int  kLeadDip  = 2;
        static constexpr int  kWidthDip = 60;

        explicit LeadEntry (int leadDip = kLeadDip) : m_leadDip (leadDip) {}

        int              GetWidthPx   (bool, const DxuiDpiScaler & scaler, IDxuiTextRenderer *) const override { return scaler.ToPx (kWidthDip); }
        void             Layout       (const RECT & rc, bool, const DxuiDpiScaler & scaler)          override { m_rc = rc; m_scaler = scaler; }
        const wchar_t *  GetTooltipAt (int, int, RECT &) const                                       override { return nullptr; }
        bool             OnClick      (int, int)                                                     override { return false; }
        int              GetLeadPx    (const DxuiDpiScaler & scaler) const                           override { return scaler.ToPx (m_leadDip); }

        void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, bool hovered, bool pressed, bool labeled) override
        {
            HRESULT  hr = S_OK;



            (void) painter;
            (void) theme;
            (void) hovered;
            (void) pressed;
            (void) labeled;

            hr = text.DrawString (L"custom", (float) (m_rc.left + m_scaler.ToPx (m_leadDip)), (float) m_rc.top, 1.0f, 1.0f, 0xFFFFFFFFu,
                                  1.0f, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

    private:
        int            m_leadDip = kLeadDip;
        RECT           m_rc      = {};
        DxuiDpiScaler  m_scaler;
    };


    static std::shared_ptr<DxuiCommand>  MakeCommand (int id, const wchar_t * label, const wchar_t * glyph)
    {
        auto  command = std::make_shared<DxuiCommand>();



        command->id    = id;
        command->label = label;
        command->glyph = glyph;
        return command;
    }


    //  The x of the first DrawString of a string.
    static float  FindTextX (const MockDxuiTextRenderer & text, const wchar_t * string)
    {
        auto  found = std::ranges::find_if (text.Calls(), [string] (const RecordedTextCall & call)
        {
            return call.kind == RecordedTextKind::DrawString && call.text == string;
        });



        return (found != text.Calls().end()) ? found->x : -1.0f;
    }


    //  Lays the strip out at its natural length, paints it, and checks where
    //  its first entry's ink falls, the gap to the entry after it, and that
    //  the last entry still ends a bar pad short of the strip's end.
    static void  CheckFirstInk (std::vector<DxuiToolbar::Entry> entries, const wchar_t * firstInk, int dpi, const wchar_t * what)
    {
        constexpr int         kLeftPx = 50;
        DxuiToolbar           bar;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        RECT                  first   = {};
        RECT                  second  = {};
        int                   length  = 0;
        int                   barPad  = 0;
        std::wstring          at      = std::format (L"{} at {} DPI", what, dpi);



        scaler.SetDpi (dpi);
        bar.SetCompact       (true);
        bar.SetPaneTextInset (true);
        bar.SetEntries       (std::move (entries));

        length = bar.GetNaturalLengthPx (scaler);
        barPad = scaler.ToPx (bar.GetSpacingDp (DxuiToolbar::Spacing::BarPadX));

        bar.Layout (RECT { kLeftPx, 0, kLeftPx + length, scaler.ToPx (DxuiToolbar::kCompactBandDp) }, scaler);
        bar.Paint  (painter, text, theme);

        Assert::IsTrue   (bar.TryGetEntryRect (1, first),  at.c_str());
        Assert::IsTrue   (bar.TryGetEntryRect (2, second), at.c_str());
        Assert::IsFalse  (bar.IsInSeeMore (2),             (L"nothing goes into See more at the natural length, " + at).c_str());

        Assert::AreEqual ((float) (kLeftPx + DxuiPaneMetrics::GetContentTextInsetPx (scaler)), FindTextX (text, firstInk), 0.5f,
                          (L"the first ink is on the pane's text inset, " + at).c_str());
        Assert::AreEqual ((LONG) scaler.ToPx (1), second.left - first.right, (L"the next entry keeps its 1-DIP gap, " + at).c_str());
        Assert::IsTrue   (second.right <= kLeftPx + length - barPad, (L"the natural length counts the room the inset takes, " + at).c_str());
    }


    TEST_METHOD (PaneTextInsetPutsTheFirstContentOnTheInset)
    {
        constexpr int  kDpis[] = { 96, 106, 120, 144, 168 };



        for (int dpi : kDpis)
        {
            LeadEntry                        custom;
            std::vector<DxuiToolbar::Entry>  icons (2);
            std::vector<DxuiToolbar::Entry>  labels (2);
            std::vector<DxuiToolbar::Entry>  customs (2);



            icons[0].command  = MakeCommand (1, L"First", L"a");
            icons[0].iconOnly = true;
            icons[1].command  = MakeCommand (2, L"Second", L"b");
            icons[1].iconOnly = true;
            CheckFirstInk (std::move (icons), L"a", dpi, L"an icon button");

            labels[0].command  = MakeCommand (1, L"Show", nullptr);
            labels[0].kind     = DxuiToolbar::Kind::DropDown;
            labels[1].command  = MakeCommand (2, L"Second", L"b");
            labels[1].iconOnly = true;
            CheckFirstInk (std::move (labels), L"Show", dpi, L"a label drop-down");

            customs[0].command  = MakeCommand (1, L"Custom", nullptr);
            customs[0].custom   = &custom;
            customs[1].command  = MakeCommand (2, L"Second", L"b");
            customs[1].iconOnly = true;
            CheckFirstInk (std::move (customs), L"custom", dpi, L"a custom entry");
        }
    }


    //  A first entry whose own lead is wider than the inset less the bar
    //  padding, such as a check box's 8 DIP or a text box's, moves back
    //  toward the strip's edge, past the bar padding and past the edge
    //  itself if need be, so its ink still lands exactly on the inset.
    TEST_METHOD (AFirstEntryWithAWideLeadStillLandsOnTheInset)
    {
        constexpr int   kDpis[]     = { 96, 106, 120, 144, 168 };
        constexpr int   kWideDips[] = { 8, 10 };
        constexpr long  kLeftPx     = 50;
        constexpr long  kWidthPx    = 400;



        for (int dpi : kDpis)
        {
            for (int wideDip : kWideDips)
            {
                LeadEntry                        custom (wideDip);
                std::vector<DxuiToolbar::Entry>  entries (1);
                DxuiToolbar                      bar;
                DxuiDpiScaler                    scaler;
                MockDxuiPainter                  painter;
                MockDxuiTextRenderer             text;
                MockDxuiTheme                    theme;
                RECT                             first  = {};
                long                             barPad = 0;
                long                             inset  = 0;
                std::wstring                     at     = std::format (L"a {}-DIP lead at {} DPI", wideDip, dpi);

                scaler.SetDpi (dpi);
                entries[0].command = MakeCommand (1, L"Custom", nullptr);
                entries[0].custom  = &custom;

                bar.SetCompact       (true);
                bar.SetPaneTextInset (true);
                bar.SetEntries       (std::move (entries));
                bar.Layout           (RECT { kLeftPx, 0, kLeftPx + kWidthPx, scaler.ToPx (DxuiToolbar::kCompactBandDp) }, scaler);
                bar.Paint            (painter, text, theme);

                barPad = scaler.ToPx (bar.GetSpacingDp (DxuiToolbar::Spacing::BarPadX));
                inset  = DxuiPaneMetrics::GetContentTextInsetPx (scaler);

                Assert::IsTrue   (custom.GetLeadPx (scaler) > inset - barPad,               (L"the lead is wider than the inset less the bar padding, " + at).c_str());
                Assert::IsTrue   (bar.TryGetEntryRect (1, first),                           at.c_str());
                Assert::AreEqual (kLeftPx + inset - custom.GetLeadPx (scaler), first.left,  (L"the entry starts its lead short of the inset, " + at).c_str());
                Assert::AreEqual ((float) (kLeftPx + inset), FindTextX (text, L"custom"),    (L"and its ink is exactly on it, " + at).c_str());
            }
        }
    }
};
