#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace
{
    constexpr int  s_kStripX     = 0;
    constexpr int  s_kStripY     = 32;
    constexpr int  s_kStripWidth = 800;


    std::vector<DxuiMenuBarItem>  MakeTestItems ()
    {
        std::vector<DxuiMenuBarItem>  items;



        items.push_back ({ L"&File", 0, {
            { L"&New",     L"Ctrl+N",  nullptr, nullptr, true,  false, false },
            { L"&Open",    L"Ctrl+O",  nullptr, nullptr, true,  false, false },
            { L"",         L"",        nullptr, nullptr, true,  false, true  },
            { L"E&xit",    L"",        nullptr, nullptr, true,  false, false },
        } });
        items.push_back ({ L"&Edit", 0, {
            { L"&Cut",     L"Ctrl+X",  nullptr, nullptr, true,  false, false },
            { L"C&opy",    L"Ctrl+C",  nullptr, nullptr, true,  false, false },
            { L"&Paste",   L"Ctrl+V",  nullptr, nullptr, false, false, false },
        } });
        items.push_back ({ L"&View", 0, {
            { L"&Toolbar", L"",        nullptr, nullptr, true,  true,  false },
            { L"&Status",  L"",        nullptr, nullptr, true,  true,  false },
        } });
        return items;
    }
}





TEST_CLASS (DxuiMenuBarTests)
{
public:

    TEST_METHOD (SetItems_AutoDerivesAltLetterFromAmpersand)
    {
        DxuiMenuBar               bar;
        std::vector<DxuiMenuBarItem>  items = MakeTestItems();


        bar.SetItems (std::move (items));

        Assert::AreEqual (3, bar.MenuCount());
        Assert::IsFalse (bar.IsOpen());
        Assert::AreEqual (-1, bar.OpenIndex());
    }


    TEST_METHOD (HandleAltKey_OpensMatchingMenu)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());

        Assert::IsTrue  (bar.HandleAltKey (L'f'));
        Assert::IsTrue  (bar.IsOpen());
        Assert::AreEqual (0, bar.OpenIndex());
        Assert::IsTrue  (bar.IsOpenByKeyboard());
    }


    TEST_METHOD (HandleAltKey_UpperCaseAlsoMatches)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());

        Assert::IsTrue  (bar.HandleAltKey (L'E'));
        Assert::AreEqual (1, bar.OpenIndex());
    }


    TEST_METHOD (HandleAltKey_NoMatchReturnsFalse)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());

        Assert::IsFalse (bar.HandleAltKey (L'q'));
        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (HoverAfterClick_OpensAdjacentMenuWithoutClick)
    {
        DxuiMenuBar           bar;
        MockDxuiTextRenderer  text;


        bar.SetItems (MakeTestItems());
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);

        RECT  fileTitle = bar.MenuRect (0);
        RECT  editTitle = bar.MenuRect (1);

        // First click opens File.
        Assert::IsTrue (bar.HandleMouseDown ((fileTitle.left + fileTitle.right) / 2,
                                             (fileTitle.top  + fileTitle.bottom) / 2));
        Assert::AreEqual (0, bar.OpenIndex());

        // Hovering Edit (without clicking) swaps the active submenu.
        Assert::IsTrue (bar.HandleMouseMove ((editTitle.left + editTitle.right) / 2,
                                             (editTitle.top  + editTitle.bottom) / 2));
        Assert::AreEqual (1, bar.OpenIndex());
        Assert::IsTrue  (bar.IsOpen());
    }


    TEST_METHOD (HoverWithoutClick_DoesNotOpen)
    {
        DxuiMenuBar           bar;
        MockDxuiTextRenderer  text;


        bar.SetItems (MakeTestItems());
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);

        RECT  fileTitle = bar.MenuRect (0);

        bar.HandleMouseMove ((fileTitle.left + fileTitle.right) / 2,
                             (fileTitle.top  + fileTitle.bottom) / 2);

        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (ArrowRight_AdvancesActiveMenu)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());
        bar.Open (0, true);

        Assert::IsTrue (bar.HandleKey (VK_RIGHT));
        Assert::AreEqual (1, bar.OpenIndex());
        Assert::IsTrue (bar.HandleKey (VK_RIGHT));
        Assert::AreEqual (2, bar.OpenIndex());
        // Wraps around.
        Assert::IsTrue (bar.HandleKey (VK_RIGHT));
        Assert::AreEqual (0, bar.OpenIndex());
    }


    TEST_METHOD (ArrowLeft_RetreatsActiveMenu)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());
        bar.Open (0, true);

        // Wraps to last.
        Assert::IsTrue (bar.HandleKey (VK_LEFT));
        Assert::AreEqual (2, bar.OpenIndex());
        Assert::IsTrue (bar.HandleKey (VK_LEFT));
        Assert::AreEqual (1, bar.OpenIndex());
    }


    TEST_METHOD (ArrowDownUp_MovesHighlightSkippingSeparators)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());
        bar.Open (0, true);

        // File has rows: New (0), Open (1), [separator], Exit (2). Highlight starts at 0.
        Assert::AreEqual (0, bar.HighlightIndex());
        Assert::IsTrue   (bar.HandleKey (VK_DOWN));
        Assert::AreEqual (1, bar.HighlightIndex());
        Assert::IsTrue   (bar.HandleKey (VK_DOWN));
        Assert::AreEqual (2, bar.HighlightIndex());
        // Down again wraps to row 0.
        Assert::IsTrue   (bar.HandleKey (VK_DOWN));
        Assert::AreEqual (0, bar.HighlightIndex());
        // Up wraps backwards.
        Assert::IsTrue   (bar.HandleKey (VK_UP));
        Assert::AreEqual (2, bar.HighlightIndex());
    }


    TEST_METHOD (ArrowDown_SkipsDisabledRow)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());
        bar.Open (1, true);    // Edit menu: Cut(0), Copy(1), Paste(2 disabled)

        Assert::AreEqual (0, bar.HighlightIndex());
        Assert::IsTrue   (bar.HandleKey (VK_DOWN));
        Assert::AreEqual (1, bar.HighlightIndex());
        // Skip disabled Paste and wrap to Cut.
        Assert::IsTrue   (bar.HandleKey (VK_DOWN));
        Assert::AreEqual (0, bar.HighlightIndex());
    }


    TEST_METHOD (Escape_DismissesAndReturnsFocusable)
    {
        DxuiMenuBar  bar;


        bar.SetItems (MakeTestItems());
        bar.Open (1, true);

        Assert::IsTrue (bar.IsOpen());
        Assert::IsTrue (bar.HandleKey (VK_ESCAPE));
        Assert::IsFalse (bar.IsOpen());
        Assert::AreEqual (-1, bar.OpenIndex());

        // Caller can re-establish keyboard focus on the strip via SetFocusedMenu;
        // verify that the menu bar accepts a focus-only state without re-opening.
        bar.SetFocusedMenu (1);
        Assert::IsTrue  (bar.HasFocus());
        Assert::AreEqual (1, bar.FocusedMenu());
        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (Enter_DispatchesHighlightedRowAndCloses)
    {
        DxuiMenuBar               bar;
        std::vector<DxuiMenuBarItem>  items;
        int                       dispatched = 0;


        items.push_back ({ L"&Run", 0, {
            { L"&Go",    L"", [&] { dispatched = 42; }, nullptr, true, false, false },
        } });
        bar.SetItems (std::move (items));
        bar.Open (0, true);

        Assert::IsTrue (bar.HandleKey (VK_RETURN));
        Assert::AreEqual (42, dispatched);
        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (CheckQuery_RendersCheckGlyphWhenTrue)
    {
        DxuiMenuBar               bar;
        MockDxuiPainter           painter;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        std::vector<DxuiMenuBarItem>  items;
        bool                      isChecked = true;


        items.push_back ({ L"&View", 0, {
            { L"&Toolbar", L"", nullptr, [&] { return isChecked; }, true, true, false },
        } });
        bar.SetItems (std::move (items));
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);
        bar.Open (0, true);
        bar.PaintDropdown (painter, text, theme, 96);

        // Look for a DrawString carrying the check glyph (U+2713).
        bool  foundCheck = false;
        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == L"\u2713")
            {
                foundCheck = true;
                break;
            }
        }
        Assert::IsTrue (foundCheck);

        // Now flip the query to false; the glyph must NOT be drawn.
        isChecked = false;
        text.Reset();
        bar.PaintDropdown (painter, text, theme, 96);
        for (const RecordedTextCall & call : text.Calls())
        {
            Assert::IsFalse (call.kind == RecordedTextKind::DrawString && call.text == L"\u2713");
        }
    }


    TEST_METHOD (DisabledItem_DoesNotDispatchOnEnter)
    {
        DxuiMenuBar               bar;
        std::vector<DxuiMenuBarItem>  items;
        int                       dispatched = 0;


        items.push_back ({ L"&File", 0, {
            { L"&Disabled", L"", [&] { dispatched++; }, nullptr, false, false, false },
        } });
        bar.SetItems (std::move (items));
        bar.Open (0, true);

        // FirstEnabledRow lands on the first non-separator row even if disabled,
        // because there are no enabled rows; Enter must still be a no-op.
        Assert::IsFalse (bar.HandleKey (VK_RETURN));
        Assert::AreEqual (0, dispatched);
        Assert::IsTrue (bar.IsOpen());     // not closed
    }


    TEST_METHOD (Separator_NotClickableAndNotHittable)
    {
        DxuiMenuBar           bar;
        MockDxuiTextRenderer  text;
        int                   dispatched = 0;


        std::vector<DxuiMenuBarItem>  items;
        items.push_back ({ L"&File", 0, {
            { L"&New",     L"", [&] { dispatched++; }, nullptr, true, false, false },
            { L"",         L"", nullptr, nullptr, true, false, true  },
            { L"E&xit",    L"", [&] { dispatched++; }, nullptr, true, false, false },
        } });
        bar.SetItems (std::move (items));
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);
        bar.Open (0, true);

        RECT  dd = bar.DropdownRect();

        // The separator occupies the band between New and Exit. Compute roughly:
        // New row at y=0 of dropdown, separator next (10 dip), Exit after that.
        // Hit-test in the separator band should not dispatch.
        int  newCenterY  = dd.top + 13;       // ~middle of New (row height ~26 px)
        int  sepCenterY  = dd.top + 26 + 5;   // middle of separator (10 px tall)
        int  exitCenterY = dd.top + 26 + 10 + 13;

        Assert::IsTrue (bar.HandleMouseUp ((dd.left + dd.right) / 2, newCenterY));
        Assert::AreEqual (1, dispatched);

        bar.Open (0, true);
        Assert::IsFalse (bar.HandleMouseUp ((dd.left + dd.right) / 2, sepCenterY));
        Assert::AreEqual (1, dispatched);     // unchanged

        // Exit row still works.
        Assert::IsTrue (bar.HandleMouseUp ((dd.left + dd.right) / 2, exitCenterY));
        Assert::AreEqual (2, dispatched);
    }


    TEST_METHOD (Separator_PaintedAsDivider)
    {
        DxuiMenuBar           bar;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        bar.SetItems (MakeTestItems());
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);
        bar.Open (0, true);
        bar.PaintDropdown (painter, text, theme, 96);

        // A separator paints a divider-coloured FillRect with very thin height (~1 dip).
        bool  foundDivider = false;
        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect &&
                call.argb == MockDxuiTheme::s_kDivider &&
                call.height <= 2.0f)
            {
                foundDivider = true;
                break;
            }
        }
        Assert::IsTrue (foundDivider);
    }


    TEST_METHOD (ClickOutsideOpenMenu_DismissesMenu)
    {
        DxuiMenuBar           bar;
        MockDxuiTextRenderer  text;


        bar.SetItems (MakeTestItems());
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);
        bar.Open (0, false);

        // Click far below the dropdown.
        Assert::IsFalse (bar.HandleMouseDown (s_kStripWidth - 1, 4000));
        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (ClickSameTitleAgain_DismissesMenu)
    {
        DxuiMenuBar           bar;
        MockDxuiTextRenderer  text;


        bar.SetItems (MakeTestItems());
        bar.Layout (s_kStripX, s_kStripY, s_kStripWidth, 96, &text);

        RECT  fileTitle = bar.MenuRect (0);
        int   cx        = (fileTitle.left + fileTitle.right) / 2;
        int   cy        = (fileTitle.top  + fileTitle.bottom) / 2;

        Assert::IsTrue (bar.HandleMouseDown (cx, cy));
        Assert::IsTrue (bar.IsOpen());
        Assert::IsTrue (bar.HandleMouseDown (cx, cy));
        Assert::IsFalse (bar.IsOpen());
    }


    TEST_METHOD (SubmenuMnemonic_DispatchesAndCloses)
    {
        DxuiMenuBar               bar;
        std::vector<DxuiMenuBarItem>  items;
        int                       dispatched = 0;


        items.push_back ({ L"&File", 0, {
            { L"&New",     L"", [&] { dispatched = 1; }, nullptr, true, false, false },
            { L"&Open",    L"", [&] { dispatched = 2; }, nullptr, true, false, false },
        } });
        bar.SetItems (std::move (items));
        bar.Open (0, true);

        // 'O' should dispatch Open and close.
        Assert::IsTrue (bar.HandleKey ((WPARAM) 'O'));
        Assert::AreEqual (2, dispatched);
        Assert::IsFalse (bar.IsOpen());
    }
};
