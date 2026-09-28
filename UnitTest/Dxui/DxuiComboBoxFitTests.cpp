#include "Pch.h"

#include "Widgets/DxuiComboBox.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiComboBoxFitTests
//
//  How the closed box fits its selected item: whole by default, shortened in
//  the middle when asked, so the end of a long label stays visible.
//
//  The mock renderer measures a flat 7 DIP per character, at 96 DPI one pixel
//  each, so the widths here are counts of characters.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTests
{
    TEST_CLASS (DxuiComboBoxFitTests)
    {
    public:

        static constexpr UINT  kDpi          = 96;
        static constexpr int   kRowHeightPx  = 28;
        static constexpr int   kNarrowPx     = 120;

        static constexpr const wchar_t *  kpszLong = L"Controller (XBOX 360 For Windows) (045e:028e)";


        //  The text the closed box drew, painted at `widthPx`.
        static std::wstring PaintSelected (DxuiComboBox & combo, int widthPx)
        {
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            std::wstring          drawn;



            scaler.SetDpi (kDpi);
            combo.Layout  (RECT { 0, 0, widthPx, kRowHeightPx }, scaler);
            combo.SetDpi  (kDpi);
            combo.PaintBase (painter, text);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString && !call.text.empty())
                {
                    drawn = call.text;
                }
            }

            return drawn;
        }


        TEST_METHOD (Elide_ByDefaultTheItemIsDrawnWhole)
        {
            DxuiComboBox  combo;



            combo.SetItems    ({ kpszLong });
            combo.SetSelected (0);

            Assert::IsTrue   (combo.GetElide() == DxuiElide::None);
            Assert::AreEqual (std::wstring (kpszLong), PaintSelected (combo, kNarrowPx));
        }


        TEST_METHOD (Elide_MiddleKeepsBothEndsOfALongItem)
        {
            DxuiComboBox  combo;
            std::wstring  drawn;
            std::wstring  full   = kpszLong;
            std::wstring  tail   = full.substr (full.size() - 1);



            combo.SetItems    ({ kpszLong });
            combo.SetSelected (0);
            combo.SetElide    (DxuiElide::Middle);

            drawn = PaintSelected (combo, kNarrowPx);

            Assert::IsTrue (drawn.size() < full.size(),                    L"shortened to fit");
            Assert::IsTrue (drawn.find (L'\x2026') != std::wstring::npos, L"with an ellipsis");
            Assert::IsTrue (drawn.rfind (L"Co", 0) == 0,                   L"keeping the start");
            Assert::IsTrue (drawn.back() == tail.back(),                   L"and the end");

            combo.SetItems    ({ L"Paddle" });
            combo.SetSelected (0);

            Assert::AreEqual (std::wstring (L"Paddle"), PaintSelected (combo, kNarrowPx), L"an item that fits is untouched");
        }
    };
}
