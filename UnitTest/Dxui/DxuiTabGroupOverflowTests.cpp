#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroupOverflowTests
//
//  Tabs that do not fit scroll with the strip's arrows at every group width
//  down to a pane's minimum (SC-028, US7/AC11): whichever tab is selected
//  lies whole inside the strip, between its arrows, and a press on an arrow
//  brings the next tab into reach.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTabGroupOverflowTests
{
    //  A floating pane's window is no narrower than this, and no docked pane
    //  is laid out narrower than a floating one.
    static constexpr int  s_kMinPaneDip   = 160;
    static constexpr int  s_kWidestDip    = 900;
    static constexpr int  s_kStepDip      = 10;
    static constexpr int  s_kHeightDip    = 300;
    static constexpr int  s_kTabCount     = 8;



    struct Rig
    {
        DxuiTabGroup                                    group;
        std::vector<std::unique_ptr<MockDxuiControl>>   panes;
        DxuiDpiScaler                                   scaler;

        explicit Rig (DxuiTabGroup::Kind kind)
        {
            group.SetKind (kind);

            for (int i = 0; i < s_kTabCount; i++)
            {
                panes.push_back (std::make_unique<MockDxuiControl>());
                group.AddTab (std::format (L"Memory window {}", i + 1), panes.back().get());
            }
        }

        void  LayOut (int widthDip)
        {
            group.Layout (RECT { 0, 0, widthDip, s_kHeightDip }, scaler);
        }
    };



    static bool IsWithin (const RECT & inner, const RECT & outer)
    {
        return inner.right > inner.left && inner.left >= outer.left && inner.right <= outer.right
            && inner.top >= outer.top && inner.bottom <= outer.bottom;
    }



    static void CheckEveryWidth (DxuiTabGroup::Kind kind)
    {
        Rig  rig (kind);



        for (int width = s_kWidestDip; width >= s_kMinPaneDip; width -= s_kStepDip)
        {
            for (int active : { 0, s_kTabCount / 2, s_kTabCount - 1 })
            {
                rig.LayOut (width);
                rig.group.SetActive (active);
                rig.LayOut (width);

                Assert::IsTrue (IsWithin (rig.group.GetTabRect (active), rig.group.GetStripRect()),
                                std::format (L"tab {} lies whole in the strip at {} DIP", active, width).c_str());
            }
        }
    }



    TEST_CLASS (DxuiTabGroupOverflowTests)
    {
    public:

        TEST_METHOD (TheSelectedDocumentTabFitsAtEveryWidth)
        {
            CheckEveryWidth (DxuiTabGroup::Kind::Document);
        }


        TEST_METHOD (TheSelectedToolWindowTabFitsAtEveryWidth)
        {
            CheckEveryWidth (DxuiTabGroup::Kind::ToolWindow);
        }


        TEST_METHOD (AtTheMinimumTheTabsOverflowAndScroll)
        {
            Rig   rig (DxuiTabGroup::Kind::Document);
            RECT  strip = {};
            RECT  last  = {};



            rig.LayOut (s_kMinPaneDip);
            strip = rig.group.GetStripRect();
            last  = rig.group.GetTabRect (s_kTabCount - 1);

            Assert::IsFalse (IsWithin (last, strip), L"the tabs do not all fit at the minimum");

            rig.group.SetActive (s_kTabCount - 1);
            rig.LayOut (s_kMinPaneDip);

            Assert::IsFalse (IsWithin (rig.group.GetTabRect (0), strip), L"selecting the last tab scrolled the first out of reach");
        }
    };
}
