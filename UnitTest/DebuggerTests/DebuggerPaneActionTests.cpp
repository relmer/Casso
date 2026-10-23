#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/DiskBreakSpecText.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneActionTests
//
//  The memory pane's edits, the source pane's breakpoint toggle, the call
//  stack's mode button and the trace's save run directly (FR-135): each
//  action is the command its line parses to, and the line is its echo.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerPaneActionTests)
{
public:

    static void AssertParsesTo (ControllerRig & rig, const std::string & line, const DebuggerAction & action)
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
        std::wstring         what   = TextEncoding::NarrowToWide (line);



        Assert::IsTrue   (parsed.status == ParseStatus::Ok, what.c_str());
        Assert::AreEqual ((int) parsed.command.verb,  (int) action.command.verb, what.c_str());
        Assert::AreEqual (parsed.command.sourceName,  action.command.sourceName, what.c_str());
        Assert::AreEqual (parsed.command.a1,          action.command.a1,         what.c_str());
        Assert::AreEqual (parsed.command.hasA1,       action.command.hasA1,      what.c_str());
        Assert::AreEqual (parsed.command.a2,          action.command.a2,         what.c_str());
        Assert::AreEqual (parsed.command.hasA2,       action.command.hasA2,      what.c_str());
        Assert::IsTrue   (parsed.command.values == action.command.values,        what.c_str());
        Assert::AreEqual (parsed.command.text,        action.command.text,       what.c_str());
        Assert::AreEqual (parsed.command.count,       action.command.count,      what.c_str());
        Assert::AreEqual (line,                       action.echo,               what.c_str());
    }



    TEST_METHOD (EachPaneActionIsTheCommandItsLineParsesTo)
    {
        ControllerRig  rig;
        CommandMode    aw      = CommandMode::AppleWin;
        const Byte     bytes[] = { 0x41, 0x42 };



        AssertParsesTo (rig, "PATCH 0400 41 42",       DebuggerActions::GetPatch            (0x0400, bytes, aw));
        AssertParsesTo (rig, "BP main.a65:4",          DebuggerActions::GetSourceBreakpoint ("main.a65", 4, aw));
        AssertParsesTo (rig, "CALLS MODE WALK",        DebuggerActions::GetCallStackMode    ("WALK", aw));
        AssertParsesTo (rig, "HISTORY SAVE \"trace.txt\"", DebuggerActions::GetSaveHistory      ("trace.txt", aw));
        AssertParsesTo (rig, "W 0300",                 DebuggerActions::GetAddWatch         (0x0300, aw));
    }



    //  File > Load file into memory... and Save memory to file... are BLOAD
    //  and BSAVE as their lines parse.
    TEST_METHOD (TheMemoryFileActionsAreTheCommandsTheirLinesParseTo)
    {
        ControllerRig  rig;
        CommandMode    aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "BLOAD \"game.bin\"",              DebuggerActions::GetLoadBinary ("game.bin", "", std::nullopt, std::nullopt, aw));
        AssertParsesTo (rig, "BLOAD \"my game\",DOS",           DebuggerActions::GetLoadBinary ("my game", "DOS", std::nullopt, std::nullopt, aw));
        AssertParsesTo (rig, "BLOAD \"game.bin\",RAW 6000",     DebuggerActions::GetLoadBinary ("game.bin", "RAW", 0x6000, std::nullopt, aw));
        AssertParsesTo (rig, "BLOAD \"game.bin\",RAW 6000,100", DebuggerActions::GetLoadBinary ("game.bin", "RAW", 0x6000, 0x100, aw));
        AssertParsesTo (rig, "BSAVE \"page.bin\" 0400,400",     DebuggerActions::GetSaveBinary ("page.bin", 0x0400, 0x400, aw));
    }



    //  The Registers and Stack panes' commands: RCC, PUSH, POP and PPOP.
    TEST_METHOD (TheRegisterAndStackActionsAreTheCommandsTheirLinesParseTo)
    {
        ControllerRig  rig;
        CommandMode    aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "RCC",     DebuggerActions::GetResetCycles (aw));
        AssertParsesTo (rig, "PUSH A9", DebuggerActions::GetPushByte    (0xA9, aw));
        AssertParsesTo (rig, "POP",     DebuggerActions::GetPopByte     (aw));
        AssertParsesTo (rig, "PPOP",    DebuggerActions::GetPopWord     (aw));
    }



    TEST_METHOD (TheWatchAndPointerActionsAreTheCommandsTheirLinesParseTo)
    {
        ControllerRig  rig;
        CommandMode    aw = CommandMode::AppleWin;



        AssertParsesTo (rig, "WE 2",  DebuggerActions::GetEnableWatch           (2, true,  aw));
        AssertParsesTo (rig, "WD 2",  DebuggerActions::GetEnableWatch           (2, false, aw));
        AssertParsesTo (rig, "ZP 06", DebuggerActions::GetAddZeroPagePointer    (0x06, aw));
        AssertParsesTo (rig, "ZPC 1", DebuggerActions::GetClearZeroPagePointer  (1, aw));
        AssertParsesTo (rig, "ZPE 1", DebuggerActions::GetEnableZeroPagePointer (1, true,  aw));
        AssertParsesTo (rig, "ZPD 1", DebuggerActions::GetEnableZeroPagePointer (1, false, aw));
    }



    //  The breakpoint dialog's New types for the stops that are not entries
    //  of the breakpoint table, each a definition the console runs.
    TEST_METHOD (TheDialogsMachineStopsAreDefinitionsTheConsoleRuns)
    {
        using Type = BreakpointDialog::Type;

        struct Case
        {
            Type         type;
            std::string  address;
            std::string  value;
            std::string  expected;
        };

        const Case  cases[] =
        {
            { Type::UnwrittenRead, "",          "",   "BRKUNINIT ON"  },
            { Type::VideoLine,     "64:6B",     "",   "BPV 64:6B"     },
            { Type::Beam,          "64",        "20", "BPBEAM 64 20"  },
            { Type::Beam,          "vbl",       "",   "BPBEAM VBL"    },
        };



        for (const Case & each : cases)
        {
            ControllerRig             rig;
            BreakpointDialog::Fields  fields;
            Reply                     reply;

            fields.type    = each.type;
            fields.address = each.address;
            fields.value   = each.value;

            Assert::AreEqual (each.expected, BreakpointDialog::MakeDefinition (fields));

            reply = rig.Run (each.expected);
            Assert::IsTrue (reply.status == CommandStatus::Ok, TextEncoding::NarrowToWide (each.expected).c_str());
        }
    }



    //  The dialog has only the fields a type's definition reads: a BASIC
    //  variable change, its variable and condition; a condition, its
    //  condition; a disk event, its event, drive, arguments and condition;
    //  BRK, a condition. A video line takes no condition.
    TEST_METHOD (TheBreakpointDialogHasOnlyTheFieldsATypeUses)
    {
        using Type = BreakpointDialog::Type;

        BreakpointDialog::VisibleFields  variable = BreakpointDialog::GetVisibleFields (Type::BasicVariable);
        BreakpointDialog::VisibleFields  reg      = BreakpointDialog::GetVisibleFields (Type::Condition);
        BreakpointDialog::VisibleFields  disk     = BreakpointDialog::GetVisibleFields (Type::Disk);
        BreakpointDialog::VisibleFields  brk      = BreakpointDialog::GetVisibleFields (Type::Brk);



        Assert::IsTrue  (variable.address && variable.condition);
        Assert::IsFalse (variable.value || variable.mode || variable.event || variable.drive);

        Assert::IsTrue  (reg.condition);
        Assert::IsFalse (reg.address || reg.value);

        Assert::IsTrue  (disk.event && disk.drive && disk.condition);
        Assert::IsFalse (disk.value || disk.mode || disk.address, L"a disk event's arguments are its own fields");

        Assert::IsTrue  (brk.condition);
        Assert::IsFalse (brk.address || brk.value, L"BRK ON takes nothing but a condition");
        Assert::IsFalse (BreakpointDialog::GetVisibleFields (Type::VideoLine).condition);
        Assert::IsTrue  (BreakpointDialog::GetVisibleFields (Type::Read).mode, L"a data breakpoint stops before or after");
    }



    //  A disk event's fields make the arguments BPDISK takes, which read back
    //  into the same fields, and each definition is one the console runs.
    TEST_METHOD (ADiskEventsFieldsMakeArgumentsThatReadBack)
    {
        using DiskFields = BreakpointDialog::DiskFields;

        struct Case
        {
            const char  * event;
            DiskFields    fields;
            const char  * arguments;
        };

        const Case  cases[] =
        {
            { "READ",    { "11", "0F", "", 0, true },                                   "11 0F PASSED"                },
            { "READ",    { "11", "0F", "", 0, false, false, false, false, false, "D5 AA 97", "D5 AA ??", true }, "11 0F ADDR D5 AA 97 DATA D5 AA ?? SECTORS 13" },
            { "READ",    { "", "0F" },                                                  "* 0F"                        },
            { "ADDRESS", { "", "", "254" },                                             "* * FE"                      },
            { "ADDRESS", { "11", "", "", 0, false, true, true },                        "11 BADSUM WRONGTRACK"        },
            { "HEAD",    { "10:12", "", "", 1, false, false, false, true },             "T 10:12 BUMP"                },
            { "HEAD",    { "", "", "", 3 },                                             "HALF"                        },
            { "WRITE",   { "11", "05", "", 1 },                                         "11 05"                       },
            { "WRITE",   { "", "", "", 4 },                                             "BLOCKED"                     },
            { "DOS",     { "11", "0F" },                                                "11 0F"                       },
            { "BLOCK",   { "", "", "2A", 0, true },                                     "2A PASSED"                   },
            { "RWTS",    { "11", "", "", 2 },                                           "11 WRITE"                    },
            { "DRIVER",  { "", "", "2A", 1 },                                           "BLOCK 2A READ"               },
            { "MOTOR",   { "", "", "", 3 },                                             "STOPPED"                     },
            { "SELECT",  { "", "", "", 0, false, false, false, false, true },           "EVERY"                       },
            { "INSERT",  {},                                                            ""                            },
        };



        for (const Case & each : cases)
        {
            int                     event     = 0;
            ControllerRig           rig;
            std::string             arguments;
            std::string             line;
            DiskFields              back;

            while (std::string (DiskBreakSpecText::GetEventWords()[(size_t) event].word) != each.event)
            {
                event++;
            }

            arguments = BreakpointDialog::MakeDiskArguments (event, each.fields);
            back      = BreakpointDialog::ParseDiskArguments (event, arguments);
            line      = std::string ("BPDISK ") + each.event + (arguments.empty() ? "" : " " + arguments);

            Assert::AreEqual (std::string (each.arguments), arguments, TextEncoding::NarrowToWide (each.event).c_str());
            Assert::AreEqual (arguments, BreakpointDialog::MakeDiskArguments (event, back), L"the fields read back differently");

            Assert::IsTrue (AppleWinParser::Parse (line, rig.controller.GetSession()).status == ParseStatus::Ok, TextEncoding::NarrowToWide (line).c_str());
        }
    }



    //  The session's own stops, set as the dialog sets them, are rows of the
    //  Breakpoints pane after the table's, with no id, and each OFF form
    //  clears its row.
    TEST_METHOD (TheSessionStopsAreRowsOfThePane)
    {
        ControllerRig                                       rig;
        CommandMode                                         aw       = CommandMode::AppleWin;
        DebuggerViewSnapshot                                snapshot;
        std::vector<std::string>                            names;



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint ("BP 300",       aw));
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint ("BRKUNINIT ON", aw));
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint ("BPV 64:6B",    aw));
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint ("BPBEAM 10 1A", aw));

        snapshot = rig.view.Build (rig.controller.GetSession());
        Assert::AreEqual ((size_t) 4, snapshot.breakpoints.size());

        for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
        {
            names.push_back (BreakpointColumns::GetCells (snapshot, bp)[(size_t) BreakpointColumns::Column::Name]);
        }

        Assert::AreEqual (-1, snapshot.breakpoints[1].id);
        Assert::IsTrue   (snapshot.breakpoints[1].info.kind == BreakpointKind::UnwrittenRead);
        Assert::IsTrue   (names[1].starts_with ("Reading uninitialized memory"), TextEncoding::NarrowToWide (names[1]).c_str());
        Assert::AreEqual (std::string ("Scanlines $64-$6B"),      names[2]);
        Assert::AreEqual (std::string ("Scanline $10, cycle $1A"), names[3]);
        Assert::IsTrue   (BreakpointDialog::GetType (snapshot.breakpoints[3].info) == BreakpointDialog::Type::Beam);
        Assert::AreEqual (std::string ("BPBEAM 10 1A"), BreakpointDialog::MakeDefinition (BreakpointDialog::GetFields (snapshot.breakpoints[3].info)));

        for (const char * off : { "BRKUNINIT OFF", "BPV OFF", "BPBEAM OFF" })
        {
            rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint (off, aw));
        }

        Assert::AreEqual ((size_t) 1, rig.view.Build (rig.controller.GetSession()).breakpoints.size(), L"an OFF form left its row");
    }



    //  Line 0 is a line like any other: the dialog opens with it in the field
    //  and makes the same definition from it.
    TEST_METHOD (ABasicLineBreakpointAtLineZeroEditsAsLineZero)
    {
        ControllerRig             rig;
        DebuggerViewSnapshot      snapshot;
        BreakpointDialog::Fields  fields;



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetDefineBreakpoint ("BPBASIC 0", CommandMode::AppleWin));

        snapshot = rig.view.Build (rig.controller.GetSession());
        Assert::AreEqual ((size_t) 1, snapshot.breakpoints.size());

        fields = BreakpointDialog::GetFields (snapshot.breakpoints[0].info);

        Assert::AreEqual (std::string ("0"),         fields.address);
        Assert::AreEqual (std::string ("BPBASIC 0"), BreakpointDialog::MakeDefinition (fields));
    }



    //  The New drop-down's types are under headings, every type the dialog
    //  makes is in it once, a memory address standing for reads, writes and
    //  both, and the dialog's title has the heading and the type.
    TEST_METHOD (TheBreakpointTypesAreGroupedUnderHeadings)
    {
        using Type = BreakpointDialog::Type;

        const std::vector<BreakpointDialog::TypeRow>  & rows     = BreakpointDialog::GetTypeRows();
        std::vector<std::wstring>                       headings;
        size_t                                          types    = 0;



        for (const BreakpointDialog::TypeRow & row : rows)
        {
            if (!row.type.has_value())
            {
                headings.push_back (row.label);
            }
            else
            {
                types++;
            }
        }

        Assert::IsTrue   (headings == std::vector<std::wstring> { L"Execution", L"Memory", L"BASIC", L"Hardware" });
        Assert::AreEqual ((size_t) Type::Beam + 1 - 2, types, L"a type is missing or repeated");
        Assert::AreEqual (BreakpointDialog::GetRowOf (Type::ReadWrite), BreakpointDialog::GetRowOf (Type::Read), L"a read is not a memory address");
        Assert::AreEqual (std::wstring (L"New memory breakpoint: Address"),   BreakpointDialog::GetTitle (Type::Write, std::nullopt));
        Assert::AreEqual (std::wstring (L"Execution breakpoint #3: Address"), BreakpointDialog::GetTitle (Type::Execute, 3));
        Assert::AreEqual (std::wstring (L"New BASIC breakpoint: Line"),       BreakpointDialog::GetTitle (Type::BasicLine, std::nullopt));
        Assert::IsFalse  (rows.front().type.has_value(), L"the list does not start with a heading");
        Assert::AreEqual (std::wstring (L"Reading uninitialized memory"), std::wstring (rows[(size_t) BreakpointDialog::GetRowOf (Type::UnwrittenRead)].label));
    }


    //  A Memory pane's Find, Fill, Copy and Compare: S's hits, F's bytes, M's
    //  copy and MC's differences, the hits and differences in the snapshot
    //  for the pane.
    TEST_METHOD (TheMemoryPanesCommandsRunAndTheirResultsReachTheSnapshot)
    {
        ControllerRig         rig;
        CommandMode           aw      = CommandMode::AppleWin;
        const Byte            bytes[] = { 0x5A, 0xA5 };
        DebuggerViewSnapshot  snapshot;



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetFillMemory (0x6000, 0x6003, bytes, aw));
        Assert::AreEqual ((Byte) 0x5A, rig.machine.GetMemoryBus().ReadByte (0x6002), L"the pattern repeats");
        Assert::AreEqual ((Byte) 0xA5, rig.machine.GetMemoryBus().ReadByte (0x6003));

        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetMoveMemory (0x7000, 0x6000, 0x6003, aw));
        Assert::AreEqual ((Byte) 0xA5, rig.machine.GetMemoryBus().ReadByte (0x7001), L"copied");

        rig.machine.GetMemoryBus().WriteByte (0x7002, 0x00);
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetCompareMemory (0x7000, 0x6000, 0x6003, aw));
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetSearchMemory ("5A A5 5A", aw));

        snapshot = rig.view.Build (rig.controller.GetSession());

        Assert::AreEqual ((size_t) 1, snapshot.comparison.size(), L"one byte differs");
        Assert::AreEqual ((Word) 0x6002, snapshot.comparison[0].address);
        Assert::AreEqual ((Byte) 0x00, snapshot.comparison[0].otherValue);
        Assert::IsTrue   (snapshot.compareSerial > 0);
        Assert::IsTrue   (std::ranges::find (snapshot.searchHits, (Word) 0x6000) != snapshot.searchHits.end(), L"the fill's start is a hit");
        Assert::IsTrue   (snapshot.searchSerial > 0);
    }



    //  A load from the File menu runs with no parser and writes the file's
    //  bytes at the address given.
    TEST_METHOD (ALoadFromTheMenuWritesTheFilesBytes)
    {
        ControllerRig  rig;



        rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));
        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetLoadBinary ("C:\\Work\\prog.bin", "RAW", 0x0300, std::nullopt, CommandMode::AppleWin));

        Assert::AreEqual ((Byte) 0xA9, rig.machine.GetMemoryBus().ReadByte (0x0300));
        Assert::AreEqual ((Byte) 0x60, rig.machine.GetMemoryBus().ReadByte (0x0302));
    }



    //  A memory edit runs with no parser and writes the bytes.
    TEST_METHOD (AMemoryEditWritesItsBytesDirectly)
    {
        ControllerRig  rig;
        const Byte     bytes[] = { 0x12, 0x34 };



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetPatch (0x0400, bytes, CommandMode::AppleWin));

        Assert::AreEqual ((Byte) 0x12, rig.machine.GetMemoryBus().ReadByte (0x0400));
        Assert::AreEqual ((Byte) 0x34, rig.machine.GetMemoryBus().ReadByte (0x0401));
    }



    TEST_METHOD (TheCallStackButtonCyclesTheMechanisms)
    {
        Assert::AreEqual (std::string ("RECORDED"), CallStackPane::GetNextMechanism (CallStackMechanism::Hybrid));
        Assert::AreEqual (std::string ("WALK"),     CallStackPane::GetNextMechanism (CallStackMechanism::Recorded));
        Assert::AreEqual (std::string ("HYBRID"),   CallStackPane::GetNextMechanism (CallStackMechanism::Walk));
    }
};