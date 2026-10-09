#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindowFocusTests
//
//  A floating pane's window outlines its pane in the focus accent while its
//  owner gives it the focused look, and in the border color otherwise, 1 DIP
//  wide at the window's edge, and reports its own keyboard focus coming and
//  going to the owner.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockedWindowFocusTests
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  FocusTestWindow
    //
    //  A floating window whose creation and focus hooks a test can run, as
    //  the window's own messages would, without an HWND.
    //
    ////////////////////////////////////////////////////////////////////////////

    class FocusTestWindow : public DxuiDockedWindow
    {
    public:
        using DxuiDockedWindow::OnCreate;
        using DxuiDockedWindow::OnWindowFocusChanged;
    };



    struct Rig
    {
        MockDxuiControl       code;
        FocusTestWindow       window;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        Rig()
        {
            window.OnCreate();
            window.GetSite().AddPane       (L"code", L"Disassembly", &code);
            window.GetSite().SetPaneLayout (DxuiPaneLayout::MakeSingle (L"code"));
            window.GetSite().Layout        (RECT { 0, 0, 400, 300 }, scaler);
        }

        //  Whether the site, painted with its after pass as the window paints
        //  it, draws anything in `argb`.
        bool IsPaintedIn (uint32_t argb)
        {
            bool  found = false;

            painter.Reset();
            window.GetSite().Paint              (painter, text, theme);
            window.GetSite().PaintAfterSiblings (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                found = found || call.argb == argb;
            }

            return found;
        }
    };



    TEST_CLASS (DxuiDockedWindowFocusTests)
    {
    public:

        TEST_METHOD (TheFocusedLookOutlinesThePaneInTheAccent)
        {
            Rig  rig;



            Assert::AreEqual ((size_t) 1, rig.window.GetSite().GetGroupCount(), L"one group");
            Assert::IsFalse  (rig.IsPaintedIn (rig.theme.FocusAccent()), L"no accent before the window has the look");

            rig.window.SetFocusedLook (true, rig.theme);

            Assert::IsTrue (rig.window.GetSite().GetGroup (0)->HasFocusedLook(), L"the group takes the focused look");
            Assert::IsTrue (rig.IsPaintedIn (rig.theme.FocusAccent()),           L"and its outline is in the accent");
        }


        TEST_METHOD (TheUnfocusedLookRevertsToTheBorder)
        {
            Rig  rig;



            rig.window.SetFocusedLook (true,  rig.theme);
            rig.window.SetFocusedLook (false, rig.theme);

            Assert::IsFalse (rig.window.GetSite().GetGroup (0)->HasFocusedLook(), L"the group loses the focused look");
            Assert::IsFalse (rig.IsPaintedIn (rig.theme.FocusAccent()),           L"no accent is left");
            Assert::IsTrue  (rig.IsPaintedIn (rig.theme.Border()),                L"the outline is in the border color");
        }


        TEST_METHOD (AWindowWithNoPaneTakesTheLook)
        {
            FocusTestWindow  window;
            MockDxuiTheme    theme;



            window.OnCreate();
            window.SetFocusedLook (true, theme);

            Assert::AreEqual ((size_t) 0, window.GetSite().GetGroupCount(), L"nothing to outline, and nothing fails");
        }


        TEST_METHOD (TheWindowsFocusChangesReachItsOwner)
        {
            Rig                rig;
            std::vector<bool>  seen;



            rig.window.SetOnFocusChanged ([&seen] (bool focused) { seen.push_back (focused); });

            rig.window.OnWindowFocusChanged (true);
            rig.window.OnWindowFocusChanged (false);

            Assert::AreEqual ((size_t) 2, seen.size(), L"each change is reported");
            Assert::IsTrue   (seen[0],                 L"the focus coming");
            Assert::IsFalse  (seen[1],                 L"and going");
        }


        TEST_METHOD (ArgbToColorrefSwapsRedAndBlue)
        {
            Assert::AreEqual (0x00332211u, DxuiDwm::ArgbToColorref (0xFF112233u), L"0x00BBGGRR, the alpha dropped");
        }


        //  A floating window's pane fills it, so its outline is the window's
        //  border: 1 DIP at the window's edge, 2 px at 150% as Visual Studio
        //  draws it, in the accent while focused and the border color not.
        TEST_METHOD (TheOutlineIsAOneDipBorderAtTheWindowsEdge)
        {
            constexpr UINT  kDpis[]  = { 96, 144 };
            constexpr long  kLines[] = { 1, 2 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                Rig           rig;
                long          left = 0;
                long          top  = 0;
                std::wstring  at   = std::format (L"{} dpi", kDpis[i]);

                rig.scaler.SetDpi (kDpis[i]);
                rig.window.GetSite().SetFloating ([] (const std::wstring &) {});
                rig.window.GetSite().Layout      (RECT { 0, 0, 400, 300 }, rig.scaler);

                for (bool focused : { true, false })
                {
                    uint32_t  argb = focused ? rig.theme.FocusAccent() : rig.theme.Border();

                    rig.window.SetFocusedLook (focused, rig.theme);
                    rig.painter.Reset();
                    rig.window.GetSite().Paint              (rig.painter, rig.text, rig.theme);
                    rig.window.GetSite().PaintAfterSiblings (rig.painter, rig.text, rig.theme);
                    left = 0;
                    top  = 0;

                    for (const RecordedPaintCall & call : rig.painter.Calls())
                    {
                        bool  isRun = call.kind == RecordedPaintKind::FillRect && call.argb == argb;

                        left = (isRun && call.x == 0.0f && call.height > call.width) ? std::lround (call.width)  : left;
                        top  = (isRun && call.y == 0.0f && call.width > call.height) ? std::lround (call.height) : top;
                    }

                    Assert::AreEqual (kLines[i], left, (L"the left side, " + at).c_str());
                    Assert::AreEqual (kLines[i], top,  (L"the top, " + at).c_str());
                }
            }
        }
    };
}
