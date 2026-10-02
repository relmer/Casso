#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteEdgeShareTests
//
//  An edge band shared with a toolbar: the edge is as deep as the toolbar,
//  and its auto-hide tabs run around the toolbar's stretch of it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiDockSiteEdgeShareTests)
{
public:

    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  console;
        DxuiDpiScaler    scaler;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetPaneLayout (layout);
            site.Layout        (RECT { 0, 0, 1000, 600 }, scaler);
        }
    };


    TEST_METHOD (ASharedEdgeIsAsDeepAsWhatSharesIt)
    {
        Rig  rig;


        rig.site.SetEdgeShare (DxuiDockSide::Left, 42, 0, 300);
        rig.site.Relayout();

        Assert::IsTrue (rig.code.GetBounds().left >= 42, L"the panes clear the toolbar's band");
    }


    TEST_METHOD (TabsRunAroundTheSharedStretch)
    {
        Rig   rig;
        RECT  tab = {};


        Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Left));
        rig.site.SetEdgeShare (DxuiDockSide::Left, 42, 0, 300);
        rig.site.Relayout();

        tab = rig.site.GetEdgeTabRect (L"console");

        Assert::AreEqual ((long) 300, tab.top, L"the tab starts past the toolbar");
        Assert::AreEqual ((long) 42,  tab.right - tab.left, L"in the band the toolbar sets");
        Assert::IsTrue   (rig.code.GetBounds().left >= 42);
    }


    TEST_METHOD (ClearingTheShareGivesTheEdgeBack)
    {
        Rig  rig;


        rig.site.SetEdgeShare (DxuiDockSide::Left, 42, 0, 300);
        rig.site.ClearEdgeShare();
        rig.site.Relayout();

        Assert::AreEqual ((long) 0, rig.code.GetBounds().left);
    }
};
