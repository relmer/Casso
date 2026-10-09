#include "Pch.h"

#include "Ui/Debugger/ToolbarCheckEntry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntryTests
//
//  A check box as a pane toolbar's entry. First on the strip, its box lands
//  on the pane's text inset, where the pane's title and the first ink of
//  every other pane strip start, and the options in a row sit evenly apart.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static std::shared_ptr<DxuiCommand> MakeCommand (int id, const wchar_t * label)
    {
        auto  command = std::make_shared<DxuiCommand>();



        command->id    = id;
        command->label = label;
        return command;
    }



    TEST_CLASS (ToolbarCheckEntryTests)
    {
    public:

        TEST_METHOD (AFirstCheckBoxLandsOnThePaneTextInset)
        {
            constexpr int   kDpis[]  = { 96, 106, 120, 144, 168, 192 };
            constexpr long  kLeftPx  = 50;
            constexpr long  kWidthPx = 600;



            for (int dpi : kDpis)
            {
                std::shared_ptr<DxuiCommand>     addresses = MakeCommand (1, L"Addresses");
                std::shared_ptr<DxuiCommand>     bytes     = MakeCommand (2, L"Code bytes");
                ToolbarCheckEntry                first (addresses);
                ToolbarCheckEntry                second (bytes);
                std::vector<DxuiToolbar::Entry>  entries (2);
                DxuiToolbar                      bar;
                DxuiDpiScaler                    scaler;
                std::wstring                     at        = std::format (L"{} DPI", dpi);

                scaler.SetDpi (dpi);
                entries[0].command = addresses;
                entries[0].custom  = &first;
                entries[1].command = bytes;
                entries[1].custom  = &second;

                bar.SetCompact       (true);
                bar.SetPaneTextInset (true);
                bar.SetEntries       (std::move (entries));
                bar.Layout           (RECT { kLeftPx, 0, kLeftPx + kWidthPx, scaler.ToPx (DxuiToolbar::kCompactBandDp) }, scaler);

                Assert::AreEqual (kLeftPx + DxuiPaneMetrics::GetContentTextInsetPx (scaler), first.GetCheckbox().GetBounds().left,
                                  (L"the first box is on the inset, " + at).c_str());
                Assert::IsTrue   (second.GetCheckbox().GetBounds().left > first.GetCheckbox().GetBounds().right,
                                  (L"the next box follows the first one's label, " + at).c_str());
            }
        }


        //  Options in a row sit evenly apart: each entry keeps the same 8 DIP
        //  ahead of its box as after its label, so the room from one label
        //  to the next box is that twice and the toolbar's gap.
        TEST_METHOD (OptionsInARowAreEvenlySpaced)
        {
            constexpr int   kDpis[]  = { 96, 106, 120, 144, 168, 192 };
            constexpr int   kPadDip  = 8;
            constexpr long  kLeftPx  = 50;
            constexpr long  kWidthPx = 600;



            for (int dpi : kDpis)
            {
                std::shared_ptr<DxuiCommand>     addresses = MakeCommand (1, L"Addresses");
                std::shared_ptr<DxuiCommand>     bytes     = MakeCommand (2, L"Code bytes");
                ToolbarCheckEntry                first (addresses);
                ToolbarCheckEntry                second (bytes);
                std::vector<DxuiToolbar::Entry>  entries (2);
                DxuiToolbar                      bar;
                DxuiDpiScaler                    scaler;
                RECT                             firstRect  = {};
                RECT                             secondRect = {};
                RECT                             firstBox   = {};
                RECT                             secondBox  = {};
                long                             gap        = 0;
                std::wstring                     at         = std::format (L"{} DPI", dpi);

                scaler.SetDpi (dpi);
                entries[0].command = addresses;
                entries[0].custom  = &first;
                entries[1].command = bytes;
                entries[1].custom  = &second;

                bar.SetCompact       (true);
                bar.SetPaneTextInset (true);
                bar.SetEntries       (std::move (entries));
                bar.Layout           (RECT { kLeftPx, 0, kLeftPx + kWidthPx, scaler.ToPx (DxuiToolbar::kCompactBandDp) }, scaler);

                Assert::IsTrue (bar.TryGetEntryRect (1, firstRect),  at.c_str());
                Assert::IsTrue (bar.TryGetEntryRect (2, secondRect), at.c_str());

                firstBox  = first.GetCheckbox().GetBounds();
                secondBox = second.GetCheckbox().GetBounds();
                gap       = scaler.ToPx (bar.GetSpacingDp (DxuiToolbar::Spacing::BtnGap));

                Assert::AreEqual ((long) scaler.ToPx (kPadDip),     firstBox.left - firstRect.left,     (L"8 DIP ahead of the box, " + at).c_str());
                Assert::AreEqual ((long) scaler.ToPx (kPadDip),     firstRect.right - firstBox.right,   (L"and 8 after the label, " + at).c_str());
                Assert::AreEqual (firstBox.left - firstRect.left,   secondBox.left - secondRect.left,   (L"the same ahead of every box, " + at).c_str());
                Assert::AreEqual (2 * scaler.ToPx (kPadDip) + gap,  secondBox.left - firstBox.right,    (L"from one label to the next box, " + at).c_str());
            }
        }
    };
}
