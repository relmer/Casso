#include "Pch.h"

#include "Window/DxuiDockedWindow.h"

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
    static constexpr RECT  s_kRow = { 10, 0, 410, 28 };



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
    };
}
