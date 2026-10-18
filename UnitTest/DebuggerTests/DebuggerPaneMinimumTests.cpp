#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "../Dxui/MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerPaneMinimumTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MinimumWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MinimumWindow : public DebuggerWindow
    {
    public:
        MinimumWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;

        DxuiDockSite * FindDockSite()
        {
            DxuiDockSite  * site = nullptr;
            size_t          i    = 0;



            for (i = 0; i < GetChildCount() && site == nullptr; i++)
            {
                site = dynamic_cast<DxuiDockSite *> (GetChild (i));
            }

            return site;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerPaneMinimumTests
    //
    //  A pane made of a bar over its rows is laid out no shorter than its bar
    //  and a row, so a sash dragged over it stops before the bar runs past
    //  the pane's edge, over the next pane or the status bar.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerPaneMinimumTests)
    {
    public:

        static constexpr RECT  s_kWindow = { 0, 0, 1400, 1600 };


        //  A 30-DIP bar over rows: the bar, the gap after it and a row's room.
        TEST_METHOD (AFramesMinimumIsItsBarTheGapAndARow)
        {
            DebuggerPaneFrame  frame (L"Pane");
            MockDxuiControl    bar;
            MockDxuiControl    rows;
            DxuiDpiScaler      scaler;



            frame.AddPart (&bar, [] (int, const DxuiDpiScaler & s) { return s.ToPx (30); });
            frame.AddPart (&rows);

            scaler.SetDpi (USER_DEFAULT_SCREEN_DPI);
            Assert::AreEqual (30 + DebuggerPaneFrame::kGapDip + DebuggerPaneFrame::kMinFillDip, frame.GetMinimumHeightPx (400, scaler));

            scaler.SetDpi (144);
            Assert::AreEqual (45 + scaler.ToPx (DebuggerPaneFrame::kGapDip) + scaler.ToPx (DebuggerPaneFrame::kMinFillDip), frame.GetMinimumHeightPx (400, scaler), L"at 150%");
        }


        //  Every split of the starting layout dragged to each end: no framed
        //  pane is laid out shorter than the least height its parts fit in.
        TEST_METHOD (NoSashDragLeavesAFramedPaneShorterThanItsParts)
        {
            CassoTheme              theme  = CassoTheme::MakeSkeuomorphic();
            NullDebuggerWindowHost  host;
            MinimumWindow           window (theme, host);
            DxuiDpiScaler     scaler;
            DxuiDockSite    * site    = nullptr;
            DxuiPaneLayout    start;
            size_t            checked = 0;



            scaler.SetDpi (USER_DEFAULT_SCREEN_DPI);
            window.OnCreate();
            window.Layout (s_kWindow, scaler);

            site  = window.FindDockSite();
            Assert::IsNotNull (site, L"the dock site is one of the window's controls");

            start = site->GetPaneLayout();

            for (const DxuiPaneLayout::SplitRect & split : start.ArrangeSplits (s_kWindow, nullptr, nullptr))
            {
                for (float ratio : { 0.05f, 0.95f })
                {
                    DxuiPaneLayout  dragged = start;

                    Assert::IsTrue (dragged.SetRatio (split.path, ratio));

                    site->SetPaneLayout (dragged);
                    window.Layout (s_kWindow, scaler);

                    for (size_t g = 0; g < site->GetGroupCount(); g++)
                    {
                        DxuiTabGroup  * group = site->GetGroup (g);

                        for (size_t t = 0; t < group->GetTabCount(); t++)
                        {
                            const DebuggerPaneFrame  * frame  = dynamic_cast<const DebuggerPaneFrame *> (group->GetContent ((int) t));
                            RECT                       bounds = (frame != nullptr) ? frame->GetBounds() : RECT {};

                            if (frame == nullptr || !frame->IsVisible())
                            {
                                continue;
                            }

                            Assert::IsTrue (bounds.bottom - bounds.top >= frame->GetMinimumHeightPx ((int) (bounds.right - bounds.left), scaler),
                                            std::format (L"{} with the split at {} at {}", site->GetPaneOf (frame), split.path, ratio).c_str());
                            checked++;
                        }
                    }
                }
            }

            Assert::IsTrue (checked > 10, L"the framed panes were checked");
        }
    };
}
