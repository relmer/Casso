#include "Pch.h"

#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChangesTests
//
//  A byte an edit changes shows in the changed color as one a step changes:
//  in a memory window, whose edit is shown at once and then read back by the
//  next snapshot, and in a disassembly view's bytes column. A stopped
//  machine's repeated snapshots leave the marks where they are.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ByteChangesTests)
    {
    public:

        static void  Show (MemoryEditModel & model, Byte at5)
        {
            std::vector<std::optional<Byte>>  bytes;
            std::vector<MemoryRegion>         regions (16, MemoryRegion::MainRam);

            for (int i = 0; i < 16; i++)
            {
                bytes.push_back ((Byte) (i == 5 ? at5 : i));
            }

            model.SetContents (0x0300, bytes, regions);
        }

        static uint8_t  GetMark (const MemoryEditModel & model, Word address)
        {
            uint8_t  mark = 0;

            model.ReadMarks (address, std::span<uint8_t> (&mark, 1));
            return mark;
        }


        TEST_METHOD (AMemoryWindowEditShowsInTheChangedColor)
        {
            MemoryEditModel       model;
            std::vector<uint8_t>  data = { 0xAA };



            model.SetOnPatch ([] (Word, std::span<const Byte>) {});
            Show (model, 5);

            Assert::IsTrue (model.WriteBytes (0x0305, data));

            //  The snapshot after the edit reads what the edit wrote.
            Show (model, 0xAA);
            Assert::AreEqual ((int) MemoryEditModel::kMarkChanged, (int) GetMark (model, 0x0305), L"the edited byte");
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone,    (int) GetMark (model, 0x0304), L"a byte left alone");

            //  A stopped machine sends the same bytes again.
            Show (model, 0xAA);
            Assert::AreEqual ((int) MemoryEditModel::kMarkChanged, (int) GetMark (model, 0x0305), L"held while stopped");
        }


        TEST_METHOD (ARunningMachineMarksOnlyWhatChangedSinceTheLastSnapshot)
        {
            MemoryEditModel  model;



            Show (model, 5);
            Show (model, 0xAA);
            Assert::AreEqual ((int) MemoryEditModel::kMarkChanged, (int) GetMark (model, 0x0305));

            model.SetPaused (false);
            Show (model, 0xAA);
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone, (int) GetMark (model, 0x0305));
        }


        //  The address tip says what a byte's color means: changed, I/O or
        //  ROM, in the order the colors take; plain RAM, or a byte outside
        //  the read, gives the address alone.
        TEST_METHOD (TheAddressTipSaysWhatTheBytesColorMeans)
        {
            MemoryEditModel                   model;
            std::vector<std::optional<Byte>>  bytes   = { 0x00, 0x00, 0x00, 0x00 };
            std::vector<MemoryRegion>         regions = { MemoryRegion::Io, MemoryRegion::Rom, MemoryRegion::SlotRom, MemoryRegion::MainRam };
            std::wstring                      tip;



            Show (model, 5);
            Show (model, 0xAA);

            Assert::IsTrue   (model.TryGetByteTip (0x0305, tip));
            Assert::AreEqual (std::wstring (L"$0305  changed"), tip);

            Assert::IsTrue   (model.TryGetByteTip (0x0304, tip));
            Assert::AreEqual (std::wstring (L"$0304"), tip, L"plain RAM");

            model.SetContents (0xC030, bytes, regions);

            Assert::IsTrue   (model.TryGetByteTip (0xC030, tip));
            Assert::AreEqual (std::wstring (L"$C030  I/O"), tip);

            Assert::IsTrue   (model.TryGetByteTip (0xC031, tip));
            Assert::AreEqual (std::wstring (L"$C031  ROM"), tip);

            Assert::IsTrue   (model.TryGetByteTip (0xC032, tip));
            Assert::AreEqual (std::wstring (L"$C032  ROM"), tip, L"a slot's ROM is colored as ROM");

            Assert::IsTrue   (model.TryGetByteTip (0xC033, tip));
            Assert::AreEqual (std::wstring (L"$C033"), tip);

            Assert::IsTrue   (model.TryGetByteTip (0x0400, tip));
            Assert::AreEqual (std::wstring (L"$0400  not read yet"), tip, L"outside every read, which the window has not reached");
        }


        TEST_METHOD (ADisassemblyRowMarksTheBytesThatChanged)
        {
            ByteChanges                       changes;
            std::vector<std::pair<int, int>>  ranges;



            changes.Update (true, ByteChanges::ParseRowBytes (0x0300, "A9 05"));
            changes.Update (true, ByteChanges::ParseRowBytes (0x0300, "A9 07"));

            ranges = changes.GetChangedRanges (0x0300, "A9 07");
            Assert::AreEqual ((size_t) 1, ranges.size());
            Assert::AreEqual (3, ranges[0].first);
            Assert::AreEqual (5, ranges[0].second);

            //  Shown again unchanged, as another snapshot of a stopped machine.
            changes.Update (true, ByteChanges::ParseRowBytes (0x0300, "A9 07"));
            Assert::AreEqual ((size_t) 1, changes.GetChangedRanges (0x0300, "A9 07").size());
        }


        TEST_METHOD (AByteNeverSeenBeforeIsNotChanged)
        {
            ByteChanges  changes;



            changes.Update (true, ByteChanges::ParseRowBytes (0x0300, "A9 05"));
            changes.Update (true, ByteChanges::ParseRowBytes (0x0400, "EA -- 60"));

            Assert::IsFalse (changes.IsChanged (0x0400));
            Assert::IsFalse (changes.IsChanged (0x0401));
            Assert::IsFalse (changes.IsChanged (0x0300));
        }
    };
}
