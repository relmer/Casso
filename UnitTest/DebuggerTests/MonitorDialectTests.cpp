#include "Pch.h"

#include "Debugger/CommandModeHelp.h"
#include "DialectRigs.h"
#include "Ui/Debugger/DebuggerLayout.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MonitorDialectTests
{
    using Lines = DialectCheck::Lines;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorSlashTests
    //
    //  `/` hands the rest of a Monitor line to AppleWin mode, the one way the
    //  Monitor reaches a Casso command. The reply renders in the Monitor's
    //  layout where the data has one, and AppleWin's otherwise.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorSlashTests)
    {
    public:
        TEST_METHOD (TheRestOfTheLine_RunsAsAnAppleWinLine)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Expect ("/bp 300",   { "Breakpoint #0 set at $0300" });
            rig.Expect ("  /BP 302", { "Breakpoint #1 set at $0302" });
            rig.Expect ("/bpl",      { "#0 enabled  at $0300, hits 0", "#1 enabled  at $0302, hits 0" });

            Assert::AreEqual (size_t (2), rig.session.GetBreakpoints().GetAll().size(), L"both are the session's");
            Assert::IsTrue   (rig.session.GetMode() == CommandMode::Monitor, L"and the mode is still the Monitor's");
        }



        //  Memory, disassembly and registers have a Monitor layout, so an
        //  AppleWin command's reply takes it.
        TEST_METHOD (AReplyWithAMonitorLayout_TakesIt)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Expect ("/MEB 300 A9 41 60", { "0300- A9 41 60" });
            Assert::AreEqual ((int) 0x41, (int) rig.Peek (0x0301), L"MEB wrote");

            rig.Expect ("/D 300:302", { "0300- A9 41 60" });
            rig.Expect ("/R",         { "A=00 X=00 Y=00 P=24 S=FD" });
            Assert::AreEqual (std::string ("0300-   A9 41       LDA   #$41"), rig.Lines ("/U 300").at (0));
        }



        //  A reply with no Monitor layout keeps AppleWin's: CALC is sixteen
        //  bits, and the Monitor's `=XX` would drop the high byte.
        TEST_METHOD (AReplyWithNoMonitorLayout_KeepsAppleWinsText)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Expect ("/CALC 1234", { "$1234  0z00110100   4660  '4'" });
            rig.Expect ("/CALC FF",   { "$00FF  0z11111111    255  ' ' (High Ctrl)" });
            rig.Expect ("/SWITCHES",  DialectSessionRig (CommandMode::AppleWin).Lines ("SWITCHES"));

            rig.Ok ("/OUTPUT MONITOR");
            rig.Ok ("/MODE APPLEWIN");
            rig.Ok ("OUTPUT MONITOR");
            rig.Expect ("CALC 1234", { "$1234  0z00110100   4660  '4'" });
        }



        TEST_METHOD (SlashModeLeavesTheMonitor)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Expect ("/MODE APPLEWIN", { "Mode: APPLEWIN" });
            Assert::IsTrue (rig.session.GetMode()         == CommandMode::AppleWin);
            Assert::IsTrue (rig.session.GetOutputFormat() == OutputFormat::AppleWin);

            rig.Ok ("MODE MONITOR");
            rig.Expect ("/mode gssquared", { "Mode: GSSQUARED" });
            Assert::IsTrue (rig.session.GetMode() == CommandMode::GSSquared);
        }



        //  A `/` anywhere but first is a character the Monitor does not
        //  have, and the line runs nothing.
        TEST_METHOD (ASlashInMidLine_IsNotAMarker)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            Reply              reply;



            rig.Ok ("300: 11 22 33 44 55 66 77 88");
            rig.Ok ("300.301");

            reply = rig.Run ("310 /bp 300");
            DialectCheck::IsError  (reply, CommandStatus::Error, "invalid arguments", "/ is not a Monitor command.", "310 /bp 300");
            DialectCheck::AreLines (DialectCheck::MonitorError ("invalid arguments", "/ is not a Monitor command."), reply.text, "310 /bp 300");

            //  Neither the examine in front of it nor the breakpoint after it
            //  ran: Return carries on from $0301, and no breakpoint is set.
            rig.Expect ("", { "0302- 33 44 55 66 77 88" });
            Assert::IsTrue (rig.session.GetBreakpoints().GetAll().empty(), L"nothing was set");
        }



        //  `/` with nothing after it is an empty AppleWin line, which does
        //  nothing, as an empty AppleWin line does.
        TEST_METHOD (ASlashAlone_IsAnEmptyAppleWinLine)
        {
            for (const char * line : { "/", "/   ", "   /" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);
                Cpu6502Registers   before = rig.Registers();
                Reply              reply  = rig.Run (line);



                DialectCheck::IsOk     (reply, line);
                DialectCheck::AreLines ({}, reply.text, line);
                Assert::AreEqual ((int) before.pc, (int) rig.Registers().pc, DialectCheck::Widen (line).c_str());
                Assert::IsTrue   (rig.session.GetMode() == CommandMode::Monitor);
            }
        }



        //  In the window a `/` line naming one of the window's own commands is
        //  carried out on its panes; batch and the channel have no panes.
        TEST_METHOD (WindowCommands_RunInTheWindow_AndNowhereElse)
        {
            DialectWindowRig   window (CommandMode::Monitor);
            DialectSessionRig  batch  (CommandMode::Monitor);
            Reply              reply;



            DialectCheck::IsOk (window.Window ("/CODE"), "/CODE");
            Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), window.view.Build (window.Session()).showPane, L"CODE brought the code pane forward");

            DialectCheck::IsOk (window.Window ("/MT2 $2000"), "/MT2");
            Assert::AreEqual ((Word) 0x2000, window.view.GetMemoryAddress(), L"MT2 moved the memory pane");

            DialectCheck::IsOk (window.Window ("/DATA2"), "/DATA2");
            Assert::IsTrue (window.view.GetMemoryWindowAddress (2).has_value(), L"DATA2 opened the second memory window");

            DialectCheck::IsOk (window.Window ("/PANEL"), "/PANEL");

            reply = batch.Run ("/CODE");
            DialectCheck::IsError  (reply, CommandStatus::NotAvailable, "command not available", "CODE needs the debugger window.", "batch /CODE");
            DialectCheck::AreLines (DialectCheck::MonitorError ("command not available", "CODE needs the debugger window."), reply.text, "batch /CODE");

            reply = window.Pipe ("/MT2 2000", CommandMode::Monitor);
            DialectCheck::IsError (reply, CommandStatus::NotAvailable, "command not available", "MT2 needs the debugger window.", "pipe /MT2");

            reply = batch.Run ("/PANEL");
            DialectCheck::IsError (reply, CommandStatus::NotAvailable, "command not available", "PANEL needs the debugger window.", "batch /PANEL");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorWithoutSlashTests
    //
    //  An AppleWin word typed at the Monitor prompt without `/` is read as
    //  Monitor characters, which is what they are there.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorWithoutSlashTests)
    {
    public:
        //  `MODE APPLEWIN` is M, a move, then O, which is nothing; the line
        //  runs nothing, so the move never happens and the mode stays.
        TEST_METHOD (ModeWithoutTheSlash_IsNotAModeSwitch)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            std::vector<Byte>  zeroPage = rig.PeekRange (0x0000, 0x00FF);
            Reply              reply    = rig.Run ("MODE APPLEWIN");



            DialectCheck::IsError  (reply, CommandStatus::Error, "invalid arguments", "O is not a Monitor command.", "MODE APPLEWIN");
            DialectCheck::AreLines (DialectCheck::MonitorError ("invalid arguments", "O is not a Monitor command."), reply.text, "MODE APPLEWIN");
            Assert::IsTrue (rig.session.GetMode() == CommandMode::Monitor, L"the mode is the Monitor's still");
            Assert::IsTrue (zeroPage == rig.PeekRange (0x0000, 0x00FF), L"and the M in front moved nothing");
        }



        //  BPL starts with a hex digit.
        TEST_METHOD (Bpl_IsAnAddressAndThenAnUnknownCharacter)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            DialectCheck::IsError (rig.Run ("BPL"), CommandStatus::Error, "invalid arguments", "P is not a Monitor command.", "BPL");
        }



        //  V alone is the Monitor's verify, of one byte against the
        //  destination, never AppleWin's cursor command -- in the window too.
        TEST_METHOD (V_IsTheMonitorsVerify_InTheWindowToo)
        {
            DialectWindowRig  rig (CommandMode::Monitor);
            Reply             reply = rig.Window ("V");



            DialectCheck::IsOk (reply, "V");
            Assert::IsTrue   (std::holds_alternative<CompareData> (reply.data), L"a verify's data");
            Assert::AreEqual ((uint32_t) 1, std::get<CompareData> (reply.data).compared, L"of one byte");
            Assert::IsFalse  (rig.view.GetCodeAddress().has_value(), L"and the code pane did not move");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorLineTests
    //
    //  How a Monitor line is read: several commands to a line, case, control
    //  characters and their caret form, separators, hex accumulation, and what
    //  a line with an error does -- nothing at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorLineTests)
    {
    public:
        TEST_METHOD (SeveralCommands_RunInOrder_AndEachTakesEffect)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            Reply              reply;



            rig.Ok ("300: 01 02");
            rig.Ok ("400: 03 04");

            reply = rig.Run ("300.301 400.401");
            DialectCheck::IsOk     (reply, "two examines");
            DialectCheck::AreLines ({ "0300- 01 02", "0400- 03 04" }, reply.text, "two examines");

            //  An examine, then a deposit, which takes the rest of the line.
            reply = rig.Run ("300 500: 05 06");
            DialectCheck::AreLines ({ "0300- 01", "0500- 05 06" }, reply.text, "examine and deposit");
            Assert::AreEqual ((int) 0x06, (int) rig.Peek (0x0501));

            //  A search and an arithmetic on one line.
            DialectCheck::AreLines ({ "0301", "=05" }, rig.Lines ("02<300.301S 2+3"), "search and sum");
        }



        //  One command keeps its reply's data, so JSON sees its structure;
        //  several merge into one reply of text.
        TEST_METHOD (OneCommandKeepsItsData_SeveralMergeTheirText)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            Reply              one;
            Reply              two;



            rig.Ok ("300: 01 02");

            one = rig.Ok ("300.301");
            two = rig.Ok ("300 301");

            Assert::IsTrue (std::holds_alternative<MemoryData>  (one.data), L"one command's own data");
            Assert::IsTrue (std::holds_alternative<MessageData> (two.data), L"a merged reply's");
            Assert::IsTrue (std::get<MessageData> (two.data).lines.empty(),  L"which carries no lines of its own");
            DialectCheck::AreLines ({ "0300- 01", "0301- 02" }, two.text, "merged");
        }



        //  Every command still runs after one that fails, and the line
        //  reports the first failure.
        TEST_METHOD (ALineWithAFailure_RunsEveryCommand_AndReportsTheFirst)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            Reply              reply;
            std::vector<Byte>  hooks;



            rig.Ok ("300: 5A");
            hooks = rig.PeekRange (0x0036, 0x0039);

            reply = rig.Run ("9^K 300 5^P");

            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("invalid arguments"),                        reply.error.label);
            Assert::AreEqual (std::string ("There is no slot 9. The slots are 0 to 7."), reply.error.detail);
            Assert::IsTrue   (std::find (reply.text.begin(), reply.text.end(), "0300- 5A") != reply.text.end(), L"the examine after it ran");
            Assert::AreEqual ((int) 0xC5, (int) rig.Peek (0x0037), L"and so did 5^P");
            Assert::AreEqual ((int) hooks[2], (int) rig.Peek (0x0038), L"the failed 9^K changed nothing");
            Assert::AreEqual ((int) hooks[3], (int) rig.Peek (0x0039));
        }



        //  A one-command line takes the output format; OUTPUT APPLEWIN in
        //  Monitor mode prints AppleWin's dump.
        TEST_METHOD (ASingleCommand_TakesTheOutputFormat)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 01 02");
            rig.Ok ("/OUTPUT APPLEWIN");

            rig.Expect ("300.301", { "0300: 01 02" + std::string (20, ' ') + ".." });
        }



        TEST_METHOD (CommandLettersAndHexDigits_IgnoreCase)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: a9 41 60");
            Assert::AreEqual ((int) 0xA9, (int) rig.Peek (0x0300), L"lowercase hex");

            DialectCheck::AreLines (rig.Lines ("300L"), rig.Lines ("300l"), "l lists as L does");

            rig.Ok ("400<300.302m");
            Assert::IsTrue (rig.PeekRange (0x0400, 0x0402) == std::vector<Byte> ({ 0xA9, 0x41, 0x60 }), L"m moves");

            rig.Expect ("^e", { "A=00 X=00 Y=00 P=24 S=FD" });
        }



        //  A control command arrives as its control code from a terminal and
        //  as a caret and its letter, in either case, from a script.
        TEST_METHOD (ControlCommands_AsTheCodeOrACaretAndALetter)
        {
            for (const char * line : { "\x05", "^E", "^e" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);

                rig.Expect (line, { "A=00 X=00 Y=00 P=24 S=FD" });
            }

            for (const char * line : { "3\x0B", "3^K", "3^k" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);

                rig.Expect (line, { "Input hook: $C300" });
                Assert::AreEqual ((int) 0xC3, (int) rig.Peek (0x0039), DialectCheck::Widen (line).c_str());
            }

            for (const char * line : { "4\x10", "4^P", "4^p" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);

                rig.Expect (line, { "Output hook: $C400" });
                Assert::AreEqual ((int) 0xC4, (int) rig.Peek (0x0037), DialectCheck::Widen (line).c_str());
            }
        }



        //  ^I, ^M and ^J are tab, Return and a line feed: separators. One in
        //  front of an address is only a separator, not a Return that shows
        //  a row of its own.
        TEST_METHOD (CaretSeparators_EndAnAddressAndNothingMore)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 01 02 03 04");

            rig.Expect ("300^I301",   { "0300- 01", "0301- 02" });
            rig.Expect ("^I302",      { "0302- 03" });
            rig.Expect ("^M303",      { "0303- 04" });
            rig.Expect ("^J300.301",  { "0300- 01 02" });
        }



        TEST_METHOD (ACaretWithoutALetter_IsAnError_AndNothingRuns)
        {
            for (const char * line : { "^", "300^1", "^@", "^[", "3^K ^" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);
                std::vector<Byte>  hooks = rig.PeekRange (0x0038, 0x0039);
                Reply              reply = rig.Run (line);



                DialectCheck::IsError  (reply, CommandStatus::Error, "invalid arguments", "^ must be followed by a letter.", line);
                DialectCheck::AreLines (DialectCheck::MonitorError ("invalid arguments", "^ must be followed by a letter."), reply.text, line);
                Assert::IsTrue (hooks == rig.PeekRange (0x0038, 0x0039), DialectCheck::Widen (line).c_str());
            }
        }



        TEST_METHOD (AnUnknownCharacter_IsAnError_AndNothingRuns)
        {
            struct Case
            {
                const char  * line;
                const char  * shown;
            };

            static constexpr Case  kCases[] =
            {
                { "300ZZ",    "Z" },
                { "X",        "X" },
                { "?",        "?" },
                { "3^K ^A",   "^A" },
                { "^H",       "^H" },
                { "3^K 300J", "J" },
            };

            for (const Case & each : kCases)
            {
                DialectSessionRig  rig (CommandMode::Monitor);
                std::vector<Byte>  hooks  = rig.PeekRange (0x0038, 0x0039);
                std::string        detail = std::string (each.shown) + " is not a Monitor command.";
                Reply              reply  = rig.Run (each.line);



                DialectCheck::IsError  (reply, CommandStatus::Error, "invalid arguments", detail, each.line);
                DialectCheck::AreLines (DialectCheck::MonitorError ("invalid arguments", detail), reply.text, each.line);
                Assert::IsTrue (hooks == rig.PeekRange (0x0038, 0x0039), L"the 3^K in front did not run");
            }
        }



        //  A byte outside ASCII starts a UTF-8 character the window sent, and
        //  is quoted as it was typed rather than as a control code.
        TEST_METHOD (ACharacterOutsideAscii_IsQuotedAsTyped)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            std::string        line  = "300\xC3\xA9";
            Reply              reply = rig.Run (line);



            DialectCheck::IsError (reply, CommandStatus::Error, "invalid arguments", "\xC3\xA9 is not a Monitor command.", "300 e-acute");
        }



        //  Digits shift into sixteen bits, so the last four are the address.
        TEST_METHOD (MoreThanFourDigits_KeepTheLastFour)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("2345: 77");
            rig.Expect ("12345", { "2345- 77" });
            rig.Expect ("0F666G", { "Assembling at $FA62. Enter a blank line to stop." });
        }



        //  Space, tab, Return and a line feed each end an address. Between
        //  two commands they add no examine of their own.
        TEST_METHOD (Separators_EndAnAddress)
        {
            for (const char * line : { "300 301", "300\t301", "300\r301", "300\n301", "300   301" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);

                rig.Ok ("300: 01 02");
                rig.Expect (line, { "0300- 01", "0301- 02" });
            }

            {
                DialectSessionRig  rig (CommandMode::Monitor);

                rig.Ok ("300: 01 02");
                rig.Expect ("3^K 300", { "Input hook: $C300", "0300- 01" });
            }
        }



        //  A line that does not parse runs nothing, so it moves none of the
        //  state the next line reads: the examine continuation, the arming
        //  `^E` gives the next colon, or where a colon stores.
        TEST_METHOD (ALineWithAnError_LeavesTheMonitorsStateAlone)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 01 02 03 04 05 06 07 08 09");
            rig.Ok ("300");

            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("310 ZZ").status);
            rig.Expect ("", { "0301- 02 03 04 05 06 07 08" });

            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("^EZ").status);
            rig.Ok (": 44");
            Assert::AreEqual ((int) 0x00, (int) rig.Registers().a, L"the failed ^E armed nothing");
            Assert::AreEqual ((int) 0x44, (int) rig.Peek (0x0300), L"so the colon stored where 300 left it");

            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("500 : 55 G").status);
            rig.Ok (": 66");
            Assert::AreEqual ((int) 0x66, (int) rig.Peek (0x0301), L"and the failed 500 moved nothing either");
        }



        //  A command acts on the address the Monitor's A1 holds: the start
        //  of a range, or the first operand of an operator.
        TEST_METHOD (ARangeInFrontOfAOneAddressCommand_GivesItsStart)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: A9 41 60");

            rig.Ok ("300.310S");
            Assert::AreEqual ((int) 0x0302, (int) rig.Registers().pc, L"stepped from $0300");

            rig.Ok ("400.40F: 99");
            Assert::AreEqual ((int) 0x99, (int) rig.Peek (0x0400), L"the colon stored at the range's start");

            rig.Expect ("300I", { "Inverse" });
            rig.Expect ("5+3-1", { "=04" });
            rig.Expect ("300..302", { "0300- A9 41 60" });

            rig.Ok ("500: 11 22");
            rig.Expect ("300<500.501", { "0500- 11 22" });
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorExamineTests
    //
    //  `addr`, `first.last`, `.last`, Return and space: the Monitor's examine,
    //  in rows that start on eight-byte boundaries.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorExamineTests)
    {
    public:
        TEST_METHOD (AnAddress_ShowsOneByte)
        {
            for (const char * line : { "300", "0300", "00300" })
            {
                DialectSessionRig  rig (CommandMode::Monitor);
                Reply              reply;



                rig.Ok ("300: A9 41");
                reply = rig.Ok (line);

                DialectCheck::AreLines ({ "0300- A9" }, reply.text, line);
                Assert::AreEqual (size_t (1), std::get<MemoryData> (reply.data).rows.size(), DialectCheck::Widen (line).c_str());
            }
        }



        TEST_METHOD (ARange_BreaksRowsOnEightByteBoundaries)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10");

            rig.Expect ("300.302", { "0300- 00 01 02" });
            rig.Expect ("303.30F", { "0303- 03 04 05 06 07", "0308- 08 09 0A 0B 0C 0D 0E 0F" });
            rig.Expect ("307.310", { "0307- 07", "0308- 08 09 0A 0B 0C 0D 0E 0F", "0310- 10" });
        }



        //  `.last` examines from the byte after the last one shown.
        TEST_METHOD (DotLast_ContinuesFromTheLastExamined)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
            rig.Ok ("300");

            rig.Expect (".30F", { "0301- 01 02 03 04 05 06 07", "0308- 08 09 0A 0B 0C 0D 0E 0F" });
            rig.Expect (".300", {});
        }



        //  Return shows the rest of the row, then a whole row at a time. A
        //  line of blanks is Return once they are trimmed.
        TEST_METHOD (Return_ContinuesToTheRowsEnd_ThenARowAtATime)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17");
            rig.Ok ("300");

            rig.Expect ("",    { "0301- 01 02 03 04 05 06 07" });
            rig.Expect (" ",   { "0308- 08 09 0A 0B 0C 0D 0E 0F" });
            rig.Expect ("\t",  { "0310- 10 11 12 13 14 15 16 17" });
        }



        TEST_METHOD (TheIoPage_ShowsQuestionMarks)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Expect ("C000",      { "C000- ??" });
            rig.Expect ("C000.C00F", { "C000- ?? ?? ?? ?? ?? ?? ?? ??", "C008- ?? ?? ?? ?? ?? ?? ?? ??" });
        }



        //  A range that ends before it starts shows nothing, and the
        //  continuation carries on from its end.
        TEST_METHOD (AReversedRange_ShowsNothing)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            Reply              reply;



            rig.Ok ("300: 00 01 02 03 04 05 06 07");

            reply = rig.Ok ("30F.300");
            DialectCheck::AreLines ({}, reply.text, "30F.300");

            rig.Expect ("", { "0301- 01 02 03 04 05 06 07" });
        }



        //  A range reaching $FFFF ends there; the next Return goes on at
        //  $0000.
        TEST_METHOD (TheTopOfMemory_EndsAtFFFF_AndReturnWrapsToZero)
        {
            DialectSessionRig  rig (CommandMode::Monitor);
            std::vector<Byte>  top  = rig.PeekRange (0xFFF8, 0xFFFF);
            std::string        row  = "FFF8-";



            for (Byte value : top)
            {
                row += std::format (" {:02X}", value);
            }

            rig.Expect ("FFF8.FFFF", { row });

            rig.Ok ("0: 10 11 12 13 14 15 16 17");
            rig.Ok ("FFFF");
            rig.Expect ("", { "0000- 10 11 12 13 14 15 16 17" });
        }



        //  `addr.` with no end is the one byte.
        TEST_METHOD (AnAddressAndAPeriod_ShowsOneByte)
        {
            DialectSessionRig  rig (CommandMode::Monitor);



            rig.Ok ("300: A9 41");
            rig.Expect ("300.", { "0300- A9" });
        }



        //  The continuation belongs to the session: a channel client's
        //  examine and the window's Return share it, across a mode switch.
        TEST_METHOD (TheContinuation_IsTheSessions)
        {
            DialectWindowRig  rig (CommandMode::Monitor);



            DialectCheck::IsOk (rig.Window ("300: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"), "deposit");
            DialectCheck::IsOk (rig.Pipe ("300", CommandMode::Monitor), "pipe 300");

            DialectCheck::AreLines ({ "0301- 01 02 03 04 05 06 07" }, rig.Window ("").text, "the window's Return");

            DialectCheck::IsOk (rig.Window ("/MODE APPLEWIN"), "/MODE APPLEWIN");
            DialectCheck::IsOk (rig.Window ("MODE MONITOR"),   "MODE MONITOR");

            DialectCheck::AreLines ({ "0308- 08 09 0A 0B 0C 0D 0E 0F" }, rig.Window ("").text, "after the switch");
        }
    };
}
