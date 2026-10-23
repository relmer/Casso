#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
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
//  UNDO RESTORES WHAT THE EDIT REPLACED, only while memory contains what the
//  edit wrote: a byte the machine changed since is not written over.
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
            std::vector<Byte>          required;

            Rig()
            {
                //  $0300-$030F: $00-$0F, all RAM, and $C000-$C00F: I/O.
                Show (0x0300, MemoryRegion::MainRam);
                model.SetOnPatch ([this] (Word address, std::span<const Byte> bytes, std::span<const Byte> before)
                {
                    lines.push_back (DebuggerActions::GetPatch (address, bytes, CommandMode::AppleWin).echo);
                    required.assign (before.begin(), before.end());
                });
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

            rig.model.SetPaused   (false);
            rig.model.SetContents (0x0300, bytes, regions);
            rig.model.ReadMarks   (0x0300, marks);
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone, (int) marks[5], L"unchanged since the read before");
        }


        //  A comparison marks the bytes that differ, puts the other range's
        //  value in each one's tip, and draws those values in place with
        //  SetShowOther.
        TEST_METHOD (AComparisonMarksItsDifferencesAndCanShowTheOtherValues)
        {
            Rig                      rig;
            std::array<uint8_t, 4>   marks = {};
            std::wstring             tip;



            rig.model.SetComparison ({ { 0x0302, 0x99 } }, false);
            rig.model.ReadMarks (0x0300, marks);

            Assert::AreEqual ((int) MemoryEditModel::kMarkDiffers, (int) marks[2]);
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone,    (int) marks[1]);
            Assert::AreEqual ((Byte) 0x02, rig.Read (0x0302), L"this range's value");

            Assert::IsTrue   (rig.model.TryGetByteTip (0x0302, tip));
            Assert::IsTrue   (tip.find (L"$99") != std::wstring::npos, tip.c_str());

            rig.model.SetShowOther (true);
            Assert::AreEqual ((Byte) 0x99, rig.Read (0x0302), L"the other range's value in its place");
            Assert::AreEqual ((Byte) 0x01, rig.Read (0x0301), L"a byte the same in both ranges is as it is");

            rig.model.ClearComparison();
            rig.model.ReadMarks (0x0300, marks);
            Assert::AreEqual ((int) MemoryEditModel::kMarkNone, (int) marks[2]);
            Assert::AreEqual ((Byte) 0x02, rig.Read (0x0302));
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


        //  A row wider than 16 bytes takes a phase of 16 or more, as Go to
        //  $0315 with 24 bytes a row does: 21.
        TEST_METHOD (APhaseCanPassSixteenBytes)
        {
            Rig  rig;



            rig.Show (0x0310, MemoryRegion::MainRam);
            rig.model.SetPhase (21);

            Assert::AreEqual ((Word) 0x0315, rig.model.GetAddressOf (0x0300));
            Assert::IsTrue   (rig.Write (0x0300, { 0x77 }));
            Assert::AreEqual (std::string ("PATCH 0315 77"), rig.lines.back(), L"the edit lands where the row is labeled");
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


        TEST_METHOD (UndoDoesNotWriteOverAByteTheMachineChanged)
        {
            Rig                  rig;
            std::optional<Word>  changed;



            rig.Write (0x0300, { 0xA9 });
            rig.Show  (0x0300, MemoryRegion::MainRam);      // a new snapshot: the machine rewrote $0300 as $00
            rig.lines.clear();

            Assert::IsFalse  (rig.model.Undo (changed));
            Assert::IsTrue   (changed == std::optional<Word> (0x0300));
            Assert::IsTrue   (rig.lines.empty(), L"nothing is written");
            Assert::IsTrue   (rig.model.CanUndo(), L"the edit stays in the history");
            Assert::IsFalse  (rig.model.CanRedo());
        }


        TEST_METHOD (UndoAndRedoSendTheBytesMemoryMustContain)
        {
            Rig                  rig;
            std::optional<Word>  changed;



            rig.Write (0x0300, { 0xA9, 0xEA });
            rig.lines.clear();

            Assert::IsTrue   (rig.model.Undo (changed));
            Assert::IsFalse  (changed.has_value());
            Assert::AreEqual (std::string ("PATCH 0300 00 01"), rig.lines.at (0));
            Assert::IsTrue   (rig.required == std::vector<Byte> { 0xA9, 0xEA }, L"for undo, the bytes the edit wrote");

            Assert::IsTrue   (rig.model.Redo (changed));
            Assert::AreEqual (std::string ("PATCH 0300 A9 EA"), rig.lines.at (1));
            Assert::IsTrue   (rig.required == std::vector<Byte> { 0x00, 0x01 }, L"for redo, the bytes the edit replaced");
        }


        TEST_METHOD (RedoDoesNotWriteOverAByteTheMachineChanged)
        {
            Rig                               rig;
            std::optional<Word>               changed;
            std::vector<std::optional<Byte>>  bytes (16);
            std::vector<MemoryRegion>         regions (16, MemoryRegion::MainRam);



            rig.Write (0x0300, { 0xA9 });
            Assert::IsTrue   (rig.model.Undo());

            bytes[0] = 0x77;                                // the machine wrote $0300 after the undo
            rig.model.SetContents (0x0300, bytes, regions);
            rig.lines.clear();

            Assert::IsFalse  (rig.model.Redo (changed));
            Assert::IsTrue   (changed == std::optional<Word> (0x0300));
            Assert::IsTrue   (rig.lines.empty());
            Assert::IsTrue   (rig.model.CanRedo(), L"the step stays to redo");
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


        //  The session takes no memory writes while the machine runs, so an
        //  undo then would drop the edit from the history and change nothing.
        TEST_METHOD (UndoAndRedoWaitForThePausedMachine)
        {
            Rig  rig;



            rig.Write (0x0300, { 0xA9 });
            rig.lines.clear();
            rig.model.SetPaused (false);

            Assert::IsFalse (rig.model.CanUndo(), L"undo is unavailable while the machine runs");
            Assert::IsFalse (rig.model.Undo());
            Assert::IsTrue  (rig.lines.empty(), L"no PATCH reaches the running machine");

            rig.model.SetPaused (true);

            Assert::IsTrue   (rig.model.Undo(), L"the edit is in the history once the machine pauses");
            Assert::AreEqual (std::string ("PATCH 0300 00"), rig.lines.at (0));

            rig.model.SetPaused (false);

            Assert::IsFalse (rig.model.CanRedo(), L"redo is unavailable while the machine runs");
            Assert::IsFalse (rig.model.Redo());
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
            Assert::AreEqual (MemoryEditModel::kMarkUnread, marks[0], L"never read, so not RAM either");

            rig.Show (0xF7F0, MemoryRegion::MainRam);
            rig.model.ReadMarks (0xF7FF, std::span<uint8_t> (marks, 1));
            Assert::AreEqual (MemoryEditModel::kMarkNone, marks[0]);
        }


        //  A window scrolled away from its read keeps the bytes it was shown
        //  there, with their colors and outlines, until the next read arrives:
        //  drawn as plain RAM in the meantime, ROM flashed white before turning
        //  blue.
        TEST_METHOD (BytesReadBeforeKeepTheirMarksUntilTheNextRead)
        {
            Rig       rig;
            uint8_t   mark    = 0;
            uint16_t  region  = 0;
            Byte      value   = 0;



            rig.Show (0xF800, MemoryRegion::Rom);
            rig.Show (0x0300, MemoryRegion::MainRam);

            rig.model.ReadMarks   (0xF805, std::span<uint8_t>  (&mark,   1));
            rig.model.ReadRegions (0xF805, std::span<uint16_t> (&region, 1));
            value = rig.Read (0xF805);

            Assert::AreEqual (MemoryEditModel::kMarkRom,   mark,   L"ROM the window read before is still marked ROM");
            Assert::AreEqual (MemoryEditModel::kRegionRom, region, L"and still outlined");
            Assert::AreEqual ((Byte) 5,                    value,  L"with the value it read");

            Assert::IsFalse (rig.Write (0xF805, { 0x00 }), L"an edit still needs the byte in the window's current read");
        }
    };
}
