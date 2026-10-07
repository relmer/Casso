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
            Assert::IsFalse (MemoryPane::GetReadStartFor (0x0300, (uint64_t) (0x0300 + DebuggerViewState::kMemoryWindowBytes - kRows * 16), kRows).has_value(), L"the last eight rows of the read");
        }


        TEST_METHOD (ScrollingPastTheEndAsksForAReadAroundTheRows)
        {
            uint64_t             top   = 0x0310 + DebuggerViewState::kMemoryWindowBytes - kRows * 16;
            std::optional<Word>  start = MemoryPane::GetReadStartFor (0x0300, top, kRows);



            Assert::IsTrue   (start.has_value());
            Assert::IsTrue   (*start <= top && *start + DebuggerViewState::kMemoryWindowBytes >= top + kRows * 16,
                              L"the new read holds every row on screen");
            Assert::AreEqual ((Word) 0, (Word) (*start % 16), L"on a row boundary");
            Assert::IsTrue   (*start < top, L"with room to scroll back");
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


        TEST_METHOD (UndoWaitsWhileTheWindowIsNotEditable)
        {
            DxuiHexView                             view;
            std::vector<std::string>                sent;
            MemoryPane                              pane (1, &view, [] (int, Word) {},
                                                          [&sent] (const DebuggerActionBuilder & build) { sent.push_back (build (CommandMode::AppleWin).echo); },
                                                          [] (const std::string &) {});
            DebuggerViewSnapshot::MemoryWindow      window;
            const Byte                              typed[] = { 0x22 };



            window.first   = 0x0300;
            window.bytes   = std::vector<std::optional<Byte>> (16, std::optional<Byte> (0x11));
            window.regions = std::vector<MemoryRegion> (16, MemoryRegion::MainRam);
            pane.Apply (window);

            view.SetEditable (true);
            Assert::IsTrue   (view.GetSource()->WriteBytes (0x0300, typed));
            Assert::AreEqual ((size_t) 1, sent.size());

            //  A running machine: the window is not editable, and the edit
            //  stays on the undo list for when it is.
            view.SetEditable (false);
            Assert::IsFalse  (pane.Undo(), L"nothing is undone while the window cannot edit");
            Assert::AreEqual ((size_t) 1, sent.size(), L"and no PATCH is sent");

            view.SetEditable (true);
            Assert::IsTrue   (pane.Undo(), L"the edit is still there to undo");
            Assert::AreEqual (std::string ("PATCH 0300 11"), sent.back());
        }


        //  Go to an address partway along a row starts the rows there, and the
        //  addresses stay the 64K's four digits rather than running past
        //  $FFFF into eight, with the usual gap and the outlines' own gutter
        //  between them and the values.
        TEST_METHOD (GoToPartwayAlongARowKeepsFourDigitAddresses)
        {
            constexpr int   kCellW = 8;
            constexpr int   kCellH = 16;
            DxuiHexView     view;
            DxuiDpiScaler   scaler;
            MemoryPane      pane (1, &view, [] (int, Word) {}, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});
            RECT            offset = {};
            RECT            value  = {};



            scaler.SetDpi (96);
            pane.Configure (nullptr);
            view.SetCellSizeDip (kCellW, kCellH);
            view.Layout (RECT { 0, 0, 800, 320 }, scaler);

            pane.GoTo (0xBFD1);

            offset = view.GetRowOffsetRect (view.GetTopRow());
            value  = view.GetByteRect (view.GetTopRow() * (uint64_t) view.GetBytesPerRow(), DxuiHexView::Column::Hex);

            Assert::AreEqual ((LONG) (4 * kCellW), offset.right - offset.left, L"$BFD1 labels in four digits");
            Assert::AreEqual ((LONG) ((DxuiHexView::kGutterCells + 1) * kCellW), value.left - offset.right,
                              L"the gap and the outlines' gutter between the address and the values");
        }
    };
}
