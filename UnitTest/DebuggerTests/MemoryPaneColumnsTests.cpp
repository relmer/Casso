#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPaneColumnsTests
//
//  The memory bar's Columns and grouping drop-downs. Columns count values, as
//  Visual Studio's do, so a row is the columns times the bytes a value holds;
//  Go to still starts the first row at the address, whatever the row width,
//  and a wide row still reads every byte on screen.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryPaneColumnsTests)
    {
    public:

        TEST_METHOD (AWindowStartsAtSixteenValuesARow)
        {
            DxuiHexView  view;
            MemoryPane   pane (1, &view, [] (int, Word) {}, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});



            pane.Configure (nullptr);

            Assert::AreEqual (16, pane.GetColumns());
            Assert::AreEqual (16, view.GetBytesPerRow());
        }


        TEST_METHOD (WordsMakeARowOfTwiceTheBytes)
        {
            DxuiHexView  view;
            MemoryPane   pane (1, &view, [] (int, Word) {}, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});



            pane.Configure   (nullptr);
            pane.SetGrouping (2);

            Assert::AreEqual (2,  pane.GetGrouping());
            Assert::AreEqual (32, view.GetBytesPerRow(), L"sixteen words a row");
        }


        TEST_METHOD (GoToStartsANarrowRowAtTheAddress)
        {
            DxuiHexView  view;
            Word         asked = 0;
            MemoryPane   pane  (1, &view, [&asked] (int, Word first) { asked = first; }, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});



            pane.Configure  (nullptr);
            pane.SetColumns (4);
            pane.GoTo       (0x0345);

            Assert::AreEqual (4,             view.GetBytesPerRow());
            Assert::AreEqual ((Word) 0x0345, pane.GetTopAddress(), L"the first row starts at the address");
            Assert::AreEqual ((Word) 0x0340, asked,                L"the read starts on a sixteen-byte boundary");
        }


        TEST_METHOD (AWideRowReadsEveryByteOnScreen)
        {
            static constexpr int  kRows     = 30;
            static constexpr int  kRowBytes = 64;   // sixteen longs

            Assert::IsFalse (MemoryPane::GetReadStartFor (0x0300, 0x0300, kRows, kRowBytes).has_value(),
                             L"one read holds thirty rows of sixteen longs");
        }
    };
}
