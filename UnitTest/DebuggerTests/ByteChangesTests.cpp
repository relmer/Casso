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
