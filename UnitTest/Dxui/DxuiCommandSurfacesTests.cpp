#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommandSurfacesTests
//
//  One command on three surfaces at once: a menu bar item list, a toolbar
//  entry and a popup menu list. What the command says about itself is what
//  all three show, so flipping its enabled state or its label changes every
//  surface without any of them being rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiCommandSurfacesTests)
{
public:

    struct Fixture
    {
        DxuiCommand           cmd;
        DxuiMenuBar           bar;
        DxuiToolbar           toolbar;
        DxuiPopupMenu         popup;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        bool                  enabled    = true;
        std::wstring          label      = L"Alpha";
        int                   dispatched = 0;

        Fixture()
        {
            std::vector<DxuiMenuBarItem>     items (1);
            std::vector<DxuiToolbar::Entry>  entries (1);

            cmd.id        = 7;
            cmd.glyph     = L"x";
            cmd.isEnabled = [this] () { return enabled; };
            cmd.labelText = [this] () { return label; };
            cmd.dispatch  = [this] () { dispatched++; };

            items[0].label = L"&Menu";
            items[0].submenu.push_back (DxuiPopupMenuItem::ForCommand (&cmd));

            entries[0].command = &cmd;

            bar.SetTextRendererForMeasure (&text);
            bar.SetItems (std::move (items));
            bar.Layout (RECT { 0, 0, 800, 32 }, scaler);

            // The popup is shown twice in a row inside one test; its reopen
            // guard would refuse the second show.
            popup.SetReopenGuard (false);

            toolbar.SetTextRenderer (&text);
            toolbar.SetEntries (std::move (entries));
            toolbar.Layout (RECT { 0, 32, 800, 74 }, scaler);
        }

        std::vector<DxuiPopupMenuItem>  List()
        {
            std::vector<DxuiPopupMenuItem>  rows;

            rows.push_back (DxuiPopupMenuItem::ForCommand (&cmd));
            return rows;
        }

        //  The toolbar's one entry, centered.
        POINT  ToolbarPoint() const { return POINT { 10 + 35, 32 + 21 }; }
    };


    TEST_METHOD (Disabled_OnAllThreeSurfaces_NoneDispatches)
    {
        Fixture  f;
        int      dimmed = 0;


        f.enabled = false;

        // Menu bar: open, Enter on the only row.
        f.bar.Open (0, true);
        Assert::IsTrue  (f.bar.IsOpen());
        f.bar.HandleKey (VK_RETURN);
        Assert::AreEqual (0, f.dispatched);
        f.bar.Close();

        // Toolbar: a full click on the entry.
        f.toolbar.OnToolbarMouseMove   (f.ToolbarPoint().x, f.ToolbarPoint().y);
        f.toolbar.OnToolbarLButtonDown (f.ToolbarPoint().x, f.ToolbarPoint().y);
        f.toolbar.OnToolbarLButtonUp   (f.ToolbarPoint().x, f.ToolbarPoint().y);
        Assert::AreEqual (0, f.dispatched);

        // Popup: shown, activated by index.
        f.popup.ShowAt (0, 100, f.List(), f.text, RECT { 0, 0, 800, 600 });
        f.popup.ActivateRow (0);
        Assert::AreEqual (0, f.dispatched);
        f.popup.Hide();

        // All three paint the label in a dimmed ink: none uses the theme's
        // full button or foreground color for it.
        f.text.Reset();
        f.toolbar.Paint (f.painter, f.text, f.theme);
        f.popup.ShowAt (0, 100, f.List(), f.text, RECT { 0, 0, 800, 600 });
        f.popup.SetTheme (&f.theme);
        f.popup.Paint (f.painter, f.text);

        for (const RecordedTextCall & c : f.text.Calls())
        {
            if (c.kind == RecordedTextKind::DrawString && c.text == L"Alpha")
            {
                Assert::AreNotEqual (MockDxuiTheme::s_kButtonText,  c.argb);
                Assert::AreNotEqual (MockDxuiTheme::s_kForeground,  c.argb);
                dimmed++;
            }
        }

        Assert::AreEqual (2, dimmed);
    }


    TEST_METHOD (Enabled_DispatchesFromEachSurface)
    {
        Fixture  f;


        f.bar.Open (0, true);
        f.bar.HandleKey (VK_RETURN);
        Assert::AreEqual (1, f.dispatched);

        f.toolbar.OnToolbarMouseMove   (f.ToolbarPoint().x, f.ToolbarPoint().y);
        f.toolbar.OnToolbarLButtonDown (f.ToolbarPoint().x, f.ToolbarPoint().y);
        f.toolbar.OnToolbarLButtonUp   (f.ToolbarPoint().x, f.ToolbarPoint().y);
        Assert::AreEqual (2, f.dispatched);

        f.popup.ShowAt (0, 100, f.List(), f.text, RECT { 0, 0, 800, 600 });
        f.popup.ActivateRow (0);
        Assert::AreEqual (3, f.dispatched);
    }


    TEST_METHOD (LabelChange_ShowsOnAllThreeSurfaces)
    {
        Fixture  f;
        int      seen = 0;


        f.label = L"Beta";

        f.text.Reset();
        f.toolbar.Paint (f.painter, f.text, f.theme);

        f.bar.Open (0, true);
        f.bar.PaintDropdown (f.painter, f.text, f.theme, 96);

        f.popup.ShowAt (0, 200, f.List(), f.text, RECT { 0, 0, 800, 600 });
        f.popup.SetTheme (&f.theme);
        f.popup.Paint (f.painter, f.text);

        for (const RecordedTextCall & c : f.text.Calls())
        {
            if (c.kind == RecordedTextKind::DrawString && c.text == L"Beta") { seen++; }
            Assert::IsFalse (c.kind == RecordedTextKind::DrawString && c.text == L"Alpha");
        }

        Assert::AreEqual (3, seen);
    }
};
