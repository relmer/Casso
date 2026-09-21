#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryEditModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModelTests
//
//  A memory window's source (FR-034 to FR-036): the bytes the window shows,
//  the command each edit sends, and the window's own undo.
//
//  AN EDIT IS A COMMAND LINE. The model writes nothing itself; it hands the
//  window a PATCH line, which runs on the CPU thread as a typed one would, so
//  an edit reaches RAM, ROM and a refusal by the same path the command box
//  does.
//
//  UNDO RESTORES WHAT THE EDIT REPLACED, even when the machine has changed the
//  byte since; that is what undoing an edit means.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryEditModelTests)
    {
    public:

        struct Rig
        {
            MemoryEditModel            model;
            std::vector<std::string>   lines;

            Rig()
            {
                //  $0300-$030F: $00-$0F, all RAM, and $C000-$C00F: I/O.
                Show (0x0300, MemoryRegion::MainRam);
                model.SetOnCommand ([this] (const std::string & line) { lines.push_back (line); });
            }

            void  Show (Word first, MemoryRegion region)
            {
                std::vector<std::optional<Byte>>  bytes;
                std::vector<MemoryRegion>         regions;

                for (int i = 0; i < 16; i++)
                {
                    bytes.push_back (region == MemoryRegion::Io ? std::optional<Byte>() : std::optional<Byte> ((Byte) i));
                    regions.push_back (region);
                }

                model.SetContents (first, bytes, regions);
            }

            bool  Write (Word address, std::initializer_list<uint8_t> bytes)
            {
                std::vector<uint8_t>  data (bytes);

                return model.WriteBytes (address, data);
            }

            Byte  Read (Word address) const
            {
                uint8_t  value = 0;

                model.ReadBytes (address, std::span<uint8_t> (&value, 1));
                return value;
            }
        };



        TEST_METHOD (AByteThatChangedSinceTheLastReadIsMarked)
        {
            Rig                               rig;
            std::vector<std::optional<Byte>>  bytes;
            std::vector<MemoryRegion>         regions (16, MemoryRegion::MainRam);
            std::array<uint8_t, 16>           marks   = {};



            for (int i = 0; i < 16; i++)
            {
                bytes.push_back ((Byte) (i == 5 ? 0xAA : i));
            }

            rig.model.SetContents (0x0300, bytes, regions);
            rig.model.ReadMarks   (0x0300, marks);

            for (int i = 0; i < 16; i++)
            {
                Assert::AreEqual ((int) (i == 5 ? MemoryEditModel::kMarkChanged : MemoryEditModel::kMarkNone), (int) marks[(size_t) i]);
            }

            rig.model.SetContents (0x0300, bytes, regions);
            rig.model.ReadMarks   (0x0300, marks);
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone, (int) marks[5], L"unchanged since the read before");
        }


        TEST_METHOD (APhaseStartsTheRowsAtAnyAddress)
        {
            Rig  rig;



            rig.model.SetPhase (3);

            Assert::AreEqual ((Word) 0x0303, rig.model.GetAddressOf (0x0300));
            Assert::AreEqual ((Byte) 3,      rig.Read (0x0300), L"offset $0300 is address $0303");
            Assert::IsTrue   (rig.Write (0x0300, { 0x77 }));
            Assert::AreEqual (std::string ("PATCH 0303 77"), rig.lines.back());
        }


        TEST_METHOD (TheWholeAddressSpaceIsTheSource)
        {
            Rig  rig;



            Assert::AreEqual ((uint64_t) 0x10000, rig.model.GetByteCount());
            Assert::AreEqual ((Byte) 0x05, rig.Read (0x0305));
            Assert::AreEqual ((Byte) 0x00, rig.Read (0x2000), L"a byte the window was not shown reads as zero");
        }


        TEST_METHOD (AByteEditIsAPatchLine)
        {
            Rig  rig;



            Assert::IsTrue   (rig.Write (0x0300, { 0xA9 }));
            Assert::AreEqual ((size_t) 1, rig.lines.size());
            Assert::AreEqual (std::string ("PATCH 0300 A9"), rig.lines[0]);
            Assert::AreEqual ((Byte) 0xA9, rig.Read (0x0300), L"shown at once, before the next snapshot");
        }


        TEST_METHOD (AWordEditIsOneLineLowByteFirst)
        {
            Rig  rig;



            Assert::IsTrue   (rig.Write (0x0302, { 0x34, 0x12 }));
            Assert::AreEqual (std::string ("PATCH 0302 34 12"), rig.lines.at (0));
        }


        TEST_METHOD (AnIoEditIsRefusedAndSendsNothing)
        {
            Rig  rig;



            rig.Show (0xC000, MemoryRegion::Io);

            Assert::IsFalse (rig.Write (0xC000, { 0x00 }));
            Assert::IsTrue  (rig.lines.empty());
            Assert::IsFalse (rig.model.CanUndo());
        }


        TEST_METHOD (AByteTheWindowWasNotShownIsRefused)
        {
            Rig  rig;



            Assert::IsFalse (rig.Write (0x2000, { 0x00 }), L"without the byte it replaces there is nothing to undo to");
            Assert::IsTrue  (rig.lines.empty());
        }


        TEST_METHOD (UndoRestoresMostRecentFirst)
        {
            Rig  rig;



            rig.Write (0x0300, { 0xA9 });
            rig.Write (0x0301, { 0x41 });
            rig.lines.clear();

            Assert::IsTrue   (rig.model.Undo());
            Assert::AreEqual (std::string ("PATCH 0301 01"), rig.lines.at (0));
            Assert::IsTrue   (rig.model.Undo());
            Assert::AreEqual (std::string ("PATCH 0300 00"), rig.lines.at (1));
            Assert::IsFalse  (rig.model.Undo(), L"nothing left");
            Assert::IsFalse  (rig.model.CanUndo());
        }


        TEST_METHOD (UndoRestoresWhatTheEditReplacedEvenAfterTheMachineChangedIt)
        {
            Rig  rig;



            rig.Write (0x0300, { 0xA9 });
            rig.Show  (0x0300, MemoryRegion::MainRam);      // a new snapshot: the machine rewrote $0300 as $00
            rig.lines.clear();

            rig.model.Undo();
            Assert::AreEqual (std::string ("PATCH 0300 00"), rig.lines.at (0), L"the value the edit replaced, not what is there now");
        }


        TEST_METHOD (EachWindowHasItsOwnHistory)
        {
            Rig  first;
            Rig  second;



            first.Write (0x0300, { 0xA9 });

            Assert::IsTrue  (first.model.CanUndo());
            Assert::IsFalse (second.model.CanUndo());
        }


        TEST_METHOD (AMachineSwitchClearsTheHistory)
        {
            Rig  rig;



            rig.Write (0x0300, { 0xA9 });
            rig.model.ClearHistory();

            Assert::IsFalse (rig.model.CanUndo());
        }


        TEST_METHOD (MarksTellIoAndRomApart)
        {
            Rig      rig;
            uint8_t  marks[2] = {};



            rig.Show (0xC000, MemoryRegion::Io);
            rig.model.ReadMarks (0xC000, marks);
            Assert::AreEqual (MemoryEditModel::kMarkIo, marks[0]);

            rig.Show (0xF800, MemoryRegion::Rom);
            rig.model.ReadMarks (0xF800, marks);
            Assert::AreEqual (MemoryEditModel::kMarkRom, marks[0]);

            rig.model.ReadMarks (0xF7FF, std::span<uint8_t> (marks, 1));
            Assert::AreEqual (MemoryEditModel::kMarkNone, marks[0]);
        }
    };
}
