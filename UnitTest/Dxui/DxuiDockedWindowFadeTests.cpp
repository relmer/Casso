#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindowFadeTests
//
//  While a torn-off pane's window is dragged over a tab strip, the row of
//  its own tab or title bar fades: most see-through at its left edge, back
//  to opaque about halfway across, so the strip under it stays visible.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockedWindowFadeTests
{
    //  A floating window whose creation a test can run without an HWND.
    class FadeTestWindow : public DxuiDockedWindow
    {
    public:
        using DxuiDockedWindow::OnCreate;
    };


    static constexpr RECT  s_kRow    = { 10, 0, 410, 28 };
    static constexpr RECT  s_kClient = { 0, 0, 600, 400 };



    static int GetAlpha (const DxuiDockDragMark & mark)
    {
        return (int) (mark.argb >> 24);
    }



    TEST_CLASS (DxuiDockedWindowFadeTests)
    {
    public:

        TEST_METHOD (TheFadeCoversTheLeftHalfOfTheRow)
        {
            std::vector<DxuiDockDragMark>  bands = DxuiDockedWindow::GetHeaderFade (s_kRow);



            Assert::IsTrue   (bands.size() > 1, L"a gradient, not one block");
            Assert::AreEqual (s_kRow.left, bands.front().rect.left);
            Assert::AreEqual (s_kRow.left + (s_kRow.right - s_kRow.left) / 2, bands.back().rect.right);

            for (size_t i = 0; i < bands.size(); i++)
            {
                Assert::AreEqual (s_kRow.top,    bands[i].rect.top);
                Assert::AreEqual (s_kRow.bottom, bands[i].rect.bottom);
                Assert::AreEqual (0u, bands[i].argb & 0x00FFFFFFu, L"black, so the blend only takes away");
                Assert::AreEqual (0,  bands[i].outlinePx);

                if (i > 0)
                {
                    Assert::AreEqual (bands[i - 1].rect.right, bands[i].rect.left, L"no gaps");
                    Assert::IsTrue   (GetAlpha (bands[i]) < GetAlpha (bands[i - 1]), L"fading toward opaque");
                }
            }

            Assert::IsTrue (GetAlpha (bands.front()) > 0x80, L"the left edge is mostly see-through");
        }


        TEST_METHOD (AnEmptyRowHasNoFade)
        {
            Assert::IsTrue (DxuiDockedWindow::GetHeaderFade (RECT {}).empty());
        }


        //  A floating window of one tool window laid out at a DPI; the fade's
        //  row, which is in the site's pixels.
        static RECT  GetFadeRowAt (UINT dpi)
        {
            MockDxuiControl   pane;
            FadeTestWindow    window;
            DxuiDpiScaler     scaler;
            RECT              row    = {};
            bool              hasRow = false;



            scaler.SetDpi (dpi);

            window.OnCreate();
            window.Adopt (pane);
            window.GetSite().AddPane       (L"console", L"Console", &pane);
            window.GetSite().SetPaneLayout (DxuiPaneLayout::MakeSingle (L"console"));
            window.GetSite().Layout        (s_kClient, scaler);

            hasRow = DxuiDockedWindow::TryGetHeaderFadeRow (window.GetSite(), row);
            Assert::IsTrue (hasRow, L"the window has its pane's group");

            return row;
        }


        //  The site lays out in pixels, so the row is its title bar as it is:
        //  inside the window and one title bar high at every scale, not
        //  scaled again.
        TEST_METHOD (TheFadeRowIsTheTitleBarAtEveryScale)
        {
            RECT  base = GetFadeRowAt (USER_DEFAULT_SCREEN_DPI);



            for (UINT dpi : { 120u, 144u })
            {
                RECT  row = GetFadeRowAt (dpi);

                Assert::IsTrue   (row.right <= s_kClient.right && row.bottom <= s_kClient.bottom, std::format (L"inside the window at {} dpi", dpi).c_str());
                Assert::AreEqual (MulDiv (base.bottom - base.top, (int) dpi, USER_DEFAULT_SCREEN_DPI), (int) (row.bottom - row.top), std::format (L"one title bar high at {} dpi", dpi).c_str());
            }
        }
    };
}
