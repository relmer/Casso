#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPaneTests
//
//  When a memory window asks the CPU thread to read somewhere else. The view
//  scrolls through all 64K, but a snapshot holds only the bytes read for the
//  window; once the rows on screen leave those, the window asks for a read
//  that holds them, with room above and below so a scroll of a row or two does
//  not ask again.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryPaneTests)
    {
    public:

        static constexpr int  kRows = 8;   // rows on screen, sixteen bytes each



        TEST_METHOD (RowsInsideTheReadAskForNothing)
        {
            Assert::IsFalse (MemoryPane::GetReadStartFor (0x0300, 0x0300, kRows).has_value());
            Assert::IsFalse (MemoryPane::GetReadStartFor (0x0300, 0x0480, kRows).has_value(), L"the last eight rows of the read");
        }


        TEST_METHOD (ScrollingPastTheEndAsksForAReadAroundTheRows)
        {
            std::optional<Word>  start = MemoryPane::GetReadStartFor (0x0300, 0x0490, kRows);



            Assert::IsTrue   (start.has_value());
            Assert::IsTrue   (*start <= 0x0490 && *start + DebuggerViewState::kMemoryWindowBytes >= 0x0490 + kRows * 16,
                              L"the new read holds every row on screen");
            Assert::AreEqual ((Word) 0, (Word) (*start % 16), L"on a row boundary");
            Assert::IsTrue   (*start < 0x0490, L"with room to scroll back");
        }


        TEST_METHOD (ScrollingAboveTheStartAsksToo)
        {
            std::optional<Word>  start = MemoryPane::GetReadStartFor (0x0300, 0x02F0, kRows);



            Assert::IsTrue (start.has_value());
            Assert::IsTrue (*start <= 0x02F0);
        }


        TEST_METHOD (TheReadStaysInsideTheAddressSpace)
        {
            Assert::AreEqual ((Word) 0x0000, *MemoryPane::GetReadStartFor (0x0300, 0x0000, kRows));
            Assert::AreEqual ((Word) (0x10000 - DebuggerViewState::kMemoryWindowBytes),
                              *MemoryPane::GetReadStartFor (0x0300, 0xFFF0, kRows), L"the last read ends at $FFFF");
        }
    };
}
