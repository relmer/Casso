#include "Pch.h"

#include "ControllerRig.h"
#include "Core/TextEncoding.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/DebugViewCapture.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/DiskBreakSpecText.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DiskBreakpointPaneTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneRig
    //
    //  The breakpoints pane's view of a real session on a //e with a Disk II
    //  in slot 6, with no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneRig : public ControllerRig
    {
    public:
        PaneRig() : ControllerRig ("Apple2e", TestMachine::Slots::DiskOnly) {}

        std::vector<std::string>  Step (std::vector<DebuggerAction> actions)
        {
            BreakpointStep  step;

            step.actions = std::move (actions);
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<std::string>  Do (BreakpointStep::Kind kind)
        {
            BreakpointStep  step;

            step.kind = kind;
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<std::string>  Import (const std::string & script)
        {
            BreakpointStep  step;

            files.WriteAllText (L"C:\\Work\\import.txt", script);
            step.kind = BreakpointStep::Kind::Import;
            step.path = "C:\\Work\\import.txt";
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<BreakpointInfo>  List()
        {
            BreakpointListData  list;

            BreakpointHandlers::ListAll (controller.GetSession(), list);
            return list.breakpoints;
        }

        std::string  Cell (BreakpointColumns::Column column, size_t index = 0)
        {
            DebuggerViewSnapshot  snapshot = view.Build (controller.GetSession());

            return BreakpointColumns::GetCells (snapshot, snapshot.breakpoints.at (index))[(size_t) column];
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskBreakpointPaneTests
    //
    //  Disk breakpoints in the Breakpoints pane: the columns, the dialog's
    //  definition, the gutter, import in every mode's form, and undo.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiskBreakpointPaneTests)
    {
    public:

        //  Kind "Disk" for every event and the event in Trigger, in FR-163's
        //  order after Interrupt; the three write events are one.
        TEST_METHOD (EachEventHasItsKindAndTrigger)
        {
            static constexpr const char *  kTriggers[] =
            {
                "Sector read", "Address field", "Head", "Write", "Write", "Write", "DOS sector",
                "ProDOS block", "RWTS call", "Driver call", "Motor", "Drive select", "Insert", "Eject",
            };
            BreakpointColumns::RowKind     previous = BreakpointColumns::RowKind::Interrupt;



            for (int event = 0; event <= (int) DiskEvent::Ejected; event++)
            {
                BreakpointInfo              info;
                BreakpointColumns::RowKind  kind;



                info.kind = BreakpointKind::Disk;
                info.disk = DiskBreakSpec { (DiskEvent) event };
                kind      = BreakpointColumns::GetRowKind (info);

                Assert::AreEqual (std::string ("Disk"),    BreakpointColumns::GetKindText (kind));
                Assert::AreEqual (std::string (kTriggers[event]), BreakpointColumns::GetTriggerText (kind));
                Assert::IsTrue   (kind >= previous, L"sorts in FR-163's order");

                previous = kind;
            }
        }

        //  An RWTS breakpoint lists "unresolved" until its vector has a JMP,
        //  then its routine, the routine's symbol and a gutter mark there.
        TEST_METHOD (AnRwtsBreakpointsColumnsFollowItsVector)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("MEB 3D9 00");
            rig.Run ("BPDISK RWTS 11 5");

            Assert::AreEqual (std::string ("Disk RWTS call, track $11 sector $05"), rig.Cell (BreakpointColumns::Column::Name));
            Assert::AreEqual (std::string ("Disk"),                rig.Cell (BreakpointColumns::Column::Kind));
            Assert::AreEqual (std::string ("RWTS call"),           rig.Cell (BreakpointColumns::Column::Trigger));
            Assert::AreEqual (std::string ("unresolved"),          rig.Cell (BreakpointColumns::Column::Address));
            Assert::AreEqual (std::string ("Slot 6, either drive"), rig.Cell (BreakpointColumns::Column::Data));
            Assert::IsFalse  (BreakpointColumns::HasRoutine (rig.List().at (0)), L"Go to disassembly has nowhere to go");

            rig.controller.GetSession().GetSymbols().Add (SymbolTableId::User, "MYRWTS", 0x0300);
            rig.Run ("MEB 3D9 4C 00 03");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (std::string ("$0300"),  rig.Cell (BreakpointColumns::Column::Address));
            Assert::AreEqual (std::string ("MYRWTS"), rig.Cell (BreakpointColumns::Column::Function));
            Assert::AreEqual (std::string ("MYRWTS"), rig.Cell (BreakpointColumns::Column::Symbol));
            Assert::IsTrue   (BreakpointColumns::HasRoutine (snapshot.breakpoints.at (0).info));
            Assert::AreEqual ((Word) 0x0300, snapshot.breakpoints.at (0).address, L"Go to disassembly goes to the routine");
            Assert::IsTrue   (DebuggerViewState::IsDiskRoutineAt (snapshot.breakpoints.at (0), 0x0300), L"the gutter marks it");
            Assert::IsFalse  (DebuggerViewState::IsCodeBreakpointAt (snapshot.breakpoints.at (0), 0x0300), L"toggling the line adds a breakpoint");
        }

        TEST_METHOD (AnEventBreakpointHasNoAddress)
        {
            PaneRig  rig;



            rig.Run ("BPDISK D2 MOTOR ON");

            Assert::AreEqual (std::string (""),              rig.Cell (BreakpointColumns::Column::Address));
            Assert::AreEqual (std::string ("Slot 6, drive 2"), rig.Cell (BreakpointColumns::Column::Data));
            Assert::AreEqual (std::string ("Motor"),         rig.Cell (BreakpointColumns::Column::Trigger));
            Assert::IsFalse  (BreakpointColumns::HasRoutine (rig.List().at (0)));
        }

        TEST_METHOD (TheCapturedViewHasTheSlotAndAnInsertsName)
        {
            PaneRig                            rig;
            std::unique_ptr<DebugViewCapture>  owner   = std::make_unique<DebugViewCapture>();
            DebugViewCapture                 & capture = *owner;



            rig.Run ("BPDISK D2 INSERT");
            DebugViewCapture::Take (rig.controller.GetSession().GetTarget(), CallRecord(), 0, 0, capture);

            Assert::IsTrue   (capture.diskState.has_value());
            Assert::AreEqual (6, capture.diskState->slot);
            Assert::AreEqual (std::string ("Disk inserted, drive 2"), rig.Cell (BreakpointColumns::Column::Name));
        }

        //  A new disk breakpoint starts with no arguments, so the field has
        //  the event's syntax.
        TEST_METHOD (ANewDiskBreakpointHasNoArguments)
        {
            BreakpointInfo  info;



            info.kind = BreakpointKind::Disk;

            Assert::AreEqual (std::string(), BreakpointDialog::GetFields (info).arguments);
        }

        //  MakeDefinition of the dialog's fields for a breakpoint returns the
        //  line that set it.
        TEST_METHOD (TheDialogGivesBackEachDefinition)
        {
            static constexpr const char *  kLines[] =
            {
                "BPDISK D1 READ 11 0 PASSED", "BPDISK ADDRESS 11 * FE BADSUM", "BPDISK HEAD QT 8:C BUMP", "BPDISK WRITE QT 4",
                "BPDISK D2 WRITE 3 4", "BPDISK WRITE BLOCKED", "BPDISK DOS 11 5", "BPDISK BLOCK 7", "BPDISK RWTS 11 5 READ",
                "BPDISK DRIVER BLOCK 2 WRITE", "BPDISK MOTOR STOPPED", "BPDISK SELECT EVERY", "BPDISK D1 INSERT", "BPDISK EJECT IF A == 1",
                "BPDISK READ 3 4 ADDR D5 AA 96 DATA D5 AA AD",
            };



            for (const char * line : kLines)
            {
                PaneRig                    rig;
                BreakpointInfo             info;
                BreakpointDialog::Fields   fields;



                rig.Run (line);
                info   = rig.List().at (0);
                fields = BreakpointDialog::GetFields (info);

                Assert::IsTrue   (fields.type == BreakpointDialog::Type::Disk, TextEncoding::NarrowToWide (line).c_str());
                Assert::AreEqual (BreakpointHandlers::MakeDefinition (info), BreakpointDialog::MakeDefinition (fields), TextEncoding::NarrowToWide (line).c_str());
            }
        }

        //  Every disk event with each of its choices and checks, filled in
        //  the dialog's disk fields with each drive filter: the definition
        //  the dialog makes is one BPDISK sets, the breakpoint it sets gives
        //  the same definition back, and the dialog reads the same fields
        //  back from it.
        TEST_METHOD (EveryEventAndOptionSetsTheBreakpointBpdiskSets)
        {
            using Fields = BreakpointDialog::DiskFields;

            struct Case
            {
                const char  * word;
                Fields        fields;
            };

            auto  marks = [] (const char * address, const char * data) { Fields f; f.addressMarks = address; f.dataMarks = data; return f; };
            auto  ts    = [] (const char * track, const char * sector) { Fields f; f.track = track; f.sector = sector; return f; };
            auto  with  = [] (Fields f, auto change) { change (f); return f; };

            const std::vector<Case>  cases =
            {
                { "READ",    Fields {} },
                { "READ",    with (ts ("11", "0"), [] (Fields & f) { f.isPassed = true; }) },
                { "READ",    marks ("D5 AA 96", "D5 AA AD") },
                { "ADDRESS", with (ts ("11", ""), [] (Fields & f) { f.number = "FE"; f.isBadChecksum = true; }) },
                { "ADDRESS", with (Fields {}, [] (Fields & f) { f.isWrongTrack = true; }) },
                { "ADDRESS", marks ("D5 AA 96", "") },
                { "HEAD",    with (Fields {}, [] (Fields & f) { f.choice = 0; }) },
                { "HEAD",    with (Fields {}, [] (Fields & f) { f.choice = 1; f.track = "3:5"; }) },
                { "HEAD",    with (Fields {}, [] (Fields & f) { f.choice = 2; f.track = "8:C"; f.isBump = true; }) },
                { "HEAD",    with (Fields {}, [] (Fields & f) { f.choice = 3; }) },
                { "WRITE",   with (Fields {}, [] (Fields & f) { f.choice = 0; }) },
                { "WRITE",   with (ts ("3", "4"), [] (Fields & f) { f.choice = 1; }) },
                { "WRITE",   with (ts ("4", ""), [] (Fields & f) { f.choice = 2; }) },
                { "WRITE",   with (ts ("4", ""), [] (Fields & f) { f.choice = 3; }) },
                { "WRITE",   with (Fields {}, [] (Fields & f) { f.choice = 4; }) },
                { "DOS",     ts ("11", "5") },
                { "DOS",     with (ts ("11", "5"), [] (Fields & f) { f.isPassed = true; }) },
                { "BLOCK",   with (Fields {}, [] (Fields & f) { f.number = "7"; }) },
                { "BLOCK",   with (Fields {}, [] (Fields & f) { f.number = "7"; f.isPassed = true; }) },
                { "RWTS",    with (ts ("11", "5"), [] (Fields & f) { f.choice = 0; }) },
                { "RWTS",    with (ts ("11", "5"), [] (Fields & f) { f.choice = 1; }) },
                { "RWTS",    with (Fields {}, [] (Fields & f) { f.choice = 2; }) },
                { "RWTS",    with (Fields {}, [] (Fields & f) { f.choice = 3; }) },
                { "RWTS",    with (Fields {}, [] (Fields & f) { f.choice = 4; }) },
                { "DRIVER",  with (Fields {}, [] (Fields & f) { f.choice = 0; }) },
                { "DRIVER",  with (Fields {}, [] (Fields & f) { f.choice = 1; f.number = "2"; }) },
                { "DRIVER",  with (Fields {}, [] (Fields & f) { f.choice = 2; f.number = "2"; }) },
                { "DRIVER",  with (Fields {}, [] (Fields & f) { f.choice = 3; }) },
                { "DRIVER",  with (Fields {}, [] (Fields & f) { f.choice = 4; }) },
                { "MOTOR",   with (Fields {}, [] (Fields & f) { f.choice = 0; }) },
                { "MOTOR",   with (Fields {}, [] (Fields & f) { f.choice = 1; }) },
                { "MOTOR",   with (Fields {}, [] (Fields & f) { f.choice = 2; }) },
                { "MOTOR",   with (Fields {}, [] (Fields & f) { f.choice = 3; }) },
                { "SELECT",  Fields {} },
                { "SELECT",  with (Fields {}, [] (Fields & f) { f.isEvery = true; }) },
                { "INSERT",  Fields {} },
                { "EJECT",   Fields {} },
            };

            auto  words = DiskBreakSpecText::GetEventWords();



            for (const Case & each : cases)
            {
                auto  found = std::ranges::find_if (words, [&each] (const DiskBreakSpecText::EventWord & word) { return std::string (word.word) == each.word; });
                int   event = (int) (found - words.begin());



                Assert::IsTrue (found != words.end(), TextEncoding::NarrowToWide (each.word).c_str());

                for (int drive = 0; drive <= 2; drive++)
                {
                    PaneRig                    rig;
                    BreakpointDialog::Fields   dialog;
                    BreakpointDialog::Fields   readBack;
                    std::string                definition;
                    std::wstring               label;
                    BreakpointInfo             info;



                    dialog.type      = BreakpointDialog::Type::Disk;
                    dialog.event     = event;
                    dialog.drive     = drive;
                    dialog.arguments = BreakpointDialog::MakeDiskArguments (event, each.fields);
                    definition       = BreakpointDialog::MakeDefinition (dialog);
                    label            = TextEncoding::NarrowToWide (definition);

                    rig.Run (definition);
                    Assert::AreEqual ((size_t) 1, rig.List().size(), (label + L": BPDISK sets it").c_str());

                    info     = rig.List().at (0);
                    readBack = BreakpointDialog::GetFields (info);

                    Assert::AreEqual (definition, BreakpointHandlers::MakeDefinition (info), (label + L": the same definition back").c_str());
                    Assert::AreEqual (event, readBack.event, label.c_str());
                    Assert::AreEqual (drive, readBack.drive, label.c_str());
                    Assert::AreEqual (dialog.arguments, BreakpointDialog::MakeDiskArguments (event, BreakpointDialog::ParseDiskArguments (event, readBack.arguments)),
                                      (label + L": the dialog reads its fields back").c_str());
                }
            }

            //  The lines a person would type for a few of them.
            for (const auto & [word, fields, drive, expected] : { std::tuple<const char *, Fields, int, const char *>
                  { "HEAD",  with (Fields {}, [] (Fields & f) { f.choice = 2; f.track = "8:C"; f.isBump = true; }), 0, "BPDISK HEAD QT 8:C BUMP" },
                  { "WRITE", ts ("3", "4") , 2, "BPDISK D2 WRITE 3 4" },
                  { "BLOCK", with (Fields {}, [] (Fields & f) { f.number = "7"; }), 0, "BPDISK BLOCK 7" } })
            {
                BreakpointDialog::Fields  dialog;
                auto                      found = std::ranges::find_if (words, [word] (const DiskBreakSpecText::EventWord & each) { return std::string (each.word) == word; });



                dialog.type      = BreakpointDialog::Type::Disk;
                dialog.event     = (int) (found - words.begin());
                dialog.drive     = drive;
                dialog.arguments = BreakpointDialog::MakeDiskArguments (dialog.event, (std::string (word) == "WRITE") ? with (fields, [] (Fields & f) { f.choice = 1; }) : fields);

                Assert::AreEqual (std::string (expected), BreakpointDialog::MakeDefinition (dialog));
            }
        }


        TEST_METHOD (ImportReadsEachModesForm)
        {
            for (const char * line : { "BPDISK D1 READ 11 0", "bpdisk D1 READ 11 0", "/bpdisk D1 READ 11 0", "!bpdisk D1 READ 11 0" })
            {
                PaneRig                      rig;
                std::vector<BreakpointInfo>  list;



                rig.Import (std::string (line) + "\n");
                list = rig.List();

                Assert::AreEqual ((size_t) 1, list.size(), TextEncoding::NarrowToWide (line).c_str());
                Assert::IsTrue   (list[0].disk.has_value() && list[0].disk->event == DiskEvent::SectorRead && list[0].disk->track == (Byte) 0x11);
            }
        }

        //  A deleted disk breakpoint comes back with its event, arguments,
        //  marks, condition, state and When hit setting.
        TEST_METHOD (UndoBringsBackADeletedDiskBreakpoint)
        {
            PaneRig         rig;
            BreakpointInfo  before;
            BreakpointInfo  after;



            rig.Run ("BPDISK D2 READ 3 4 ADDR D5 AA 96 PASSED IF A == 1");
            rig.Run ("BPD 0");
            rig.Run ("BPCHANGE 0 s");
            before = rig.List().at (0);

            rig.Step ({ DebuggerActions::GetClearBreakpoint (before.id, CommandMode::AppleWin) });
            Assert::IsTrue (rig.List().empty());

            rig.Do (BreakpointStep::Kind::Undo);
            after = rig.List().at (0);

            Assert::IsTrue   (before.disk == after.disk);
            Assert::AreEqual (before.condition, after.condition);
            Assert::AreEqual (before.enabled,   after.enabled);
            Assert::AreEqual (before.stops,     after.stops);

            rig.Do (BreakpointStep::Kind::Redo);
            Assert::IsTrue (rig.List().empty());
        }
    };
}
