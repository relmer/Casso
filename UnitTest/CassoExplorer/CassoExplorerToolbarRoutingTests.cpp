#include "Pch.h"
#include "CassoExplorer/CassoExplorerShell.h"

#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerToolbarRoutingTests
//
//  The window has two toolbars in two rows: the navigation toolbar with the
//  address bar, and the command bar under it. A pointer event goes to the one
//  whose row it is in. The command bar once received no pointer input at
//  all, so every button on it ignored clicks; these tests click through the
//  same choice the window makes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerToolbarRoutingTests)
{
public:

    struct Fixture
    {
        std::shared_ptr<DxuiCommand>  navCmd    = std::make_shared<DxuiCommand>();
        std::shared_ptr<DxuiCommand>  barCmd    = std::make_shared<DxuiCommand>();
        DxuiToolbar                   nav;
        DxuiToolbar                   bar;
        MockDxuiTextRenderer          text;
        DxuiDpiScaler                 scaler;
        RECT                          navBand   = { 0, 0,  800, 42 };
        RECT                          barBand   = { 0, 42, 800, 88 };
        int                           navClicks = 0;
        int                           barClicks = 0;

        Fixture()
        {
            Setup (nav, navCmd, 1, navBand, navClicks);
            Setup (bar, barCmd, 2, barBand, barClicks);
        }

        void  Setup (DxuiToolbar & toolbar, std::shared_ptr<DxuiCommand> & cmd, int id, const RECT & band, int & clicks)
        {
            std::vector<DxuiToolbar::Entry>  entries (1);

            cmd->id       = id;
            cmd->glyph    = L"x";
            cmd->dispatch = [&clicks] () { clicks++; };

            entries[0].command = cmd;

            toolbar.SetTextRenderer (&text);
            toolbar.SetEntries (std::move (entries));
            toolbar.Layout (band, scaler);
        }

        //  A full click at the point, delivered to the toolbar the window
        //  would choose.
        void  Click (POINT point)
        {
            DxuiToolbar &  under = CassoExplorerWindow::GetToolbarUnder (barBand, point, nav, bar);

            under.OnToolbarMouseMove   (point.x, point.y);
            under.OnToolbarLButtonDown (point.x, point.y);
            under.OnToolbarLButtonUp   (point.x, point.y);
        }

        static POINT  GetEntryCenter (const DxuiToolbar & toolbar, int id)
        {
            RECT  rc = {};

            Assert::IsTrue (toolbar.TryGetEntryRect (id, rc), L"the entry is laid out");

            return POINT { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        }
    };


    TEST_METHOD (ClickOnCommandBar_RunsItsCommand)
    {
        Fixture  f;

        f.Click (Fixture::GetEntryCenter (f.bar, 2));

        Assert::AreEqual (1, f.barClicks, L"the command bar's button ran");
        Assert::AreEqual (0, f.navClicks, L"and nothing on the row above");
    }


    TEST_METHOD (ClickOnNavigationToolbar_RunsItsCommand)
    {
        Fixture  f;

        f.Click (Fixture::GetEntryCenter (f.nav, 1));

        Assert::AreEqual (1, f.navClicks, L"the navigation toolbar's button ran");
        Assert::AreEqual (0, f.barClicks, L"and nothing on the command bar");
    }


    TEST_METHOD (GetToolbarUnder_ChoosesByRow)
    {
        Fixture  f;

        Assert::IsTrue (&CassoExplorerWindow::GetToolbarUnder (f.barBand, POINT { 400, 60 },  f.nav, f.bar) == &f.bar, L"inside the command bar's row");
        Assert::IsTrue (&CassoExplorerWindow::GetToolbarUnder (f.barBand, POINT { 400, 20 },  f.nav, f.bar) == &f.nav, L"in the row above");
        Assert::IsTrue (&CassoExplorerWindow::GetToolbarUnder (f.barBand, POINT { 400, 300 }, f.nav, f.bar) == &f.nav, L"below both rows");
    }
};
