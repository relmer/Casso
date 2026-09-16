#include "Pch.h"

#include "Debugger/MonitorParser.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorParserTests
    //
    //  Every form in contracts/command-modes.md, "Apple II Monitor mode".
    //
    //  THE SAME LETTER MEANS DIFFERENT THINGS BY POSITION, which is the whole
    //  reason this is a line scanner and not a table of names: `S` alone
    //  steps and `41<300.3FFS` searches, `:` deposits bytes and `:` after
    //  `^E` sets registers. Each pair is tested together, because getting one
    //  right by breaking the other is the failure that matters.
    //
    //  The commands a line produces are asserted, not their effects; what
    //  each one does to the machine is MonitorHandlersTests.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorParserTests)
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        //
        //  Rig
        //
        //  A state that carries across lines, as the Monitor's does.
        //
        ////////////////////////////////////////////////////////////////////////

        struct Rig
        {
            MonitorState  state;

            MonitorParseResult Parse (const std::string & line)
            {
                return MonitorParser::Parse (line, state);
            }

            //  The one command the line produced.
            DebugCommand One (const std::string & line)
            {
                MonitorParseResult  result = Parse (line);



                Assert::IsTrue (result.status == ParseStatus::Ok,
                                Widen (line + ": " + result.error).c_str());
                Assert::AreEqual (size_t (1), result.commands.size(), Widen (line).c_str());

                return result.commands.front();
            }
        };



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        static void AssertVerb (DebugVerb expected, const DebugCommand & command, const std::string & line)
        {
            Assert::AreEqual ((int) expected, (int) command.verb, Widen (line).c_str());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Examining
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Examine_TakesAnAddressOrARange)
        {
            Rig           rig;
            DebugCommand  one   = rig.One ("300");
            DebugCommand  range = rig.One ("300.30F");



            AssertVerb (DebugVerb::Examine, one, "300");
            Assert::AreEqual ((int) 0x300, (int) one.a1);
            Assert::IsTrue   (one.hasA1);
            Assert::IsFalse  (one.hasA2, L"one address is not a range");

            AssertVerb (DebugVerb::Examine, range, "300.30F");
            Assert::AreEqual ((int) 0x300, (int) range.a1);
            Assert::AreEqual ((int) 0x30F, (int) range.a2);
            Assert::IsTrue   (range.hasA2);
        }



        //  `.addr` PICKS UP WHERE THE LAST ONE STOPPED, which is the whole
        //  point of the form: after `300`, `.30F` reads $0301 through $030F.
        TEST_METHOD (DotAddress_ContinuesFromTheLastExamined)
        {
            Rig           rig;
            DebugCommand  rest;



            rig.One ("300");
            rest = rig.One (".30F");

            AssertVerb (DebugVerb::Examine, rest, ".30F");
            Assert::AreEqual ((int) 0x301, (int) rest.a1, L"from the byte after the last one");
            Assert::AreEqual ((int) 0x30F, (int) rest.a2);
        }



        //  An empty line and a bare space both continue, a line at a time.
        TEST_METHOD (ReturnAndSpace_ContinueExamining)
        {
            Rig           rig;
            DebugCommand  afterReturn;
            DebugCommand  afterSpace;



            rig.One ("300");

            //  To the end of the row rather than eight bytes on: rows are
            //  labeled on eight-byte boundaries, so the first continuation
            //  is short and the next one is a full row.
            afterReturn = rig.One ("");
            AssertVerb (DebugVerb::Examine, afterReturn, "<return>");
            Assert::AreEqual ((int) 0x301, (int) afterReturn.a1);
            Assert::AreEqual ((int) 0x307, (int) afterReturn.a2, L"to the end of the row");

            afterSpace = rig.One (" ");
            AssertVerb (DebugVerb::Examine, afterSpace, "<space>");
            Assert::AreEqual ((int) 0x308, (int) afterSpace.a1, L"and the next row starts on the boundary");
            Assert::AreEqual ((int) 0x30F, (int) afterSpace.a2);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Depositing
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Deposit_WithAnAddress_AndWithout)
        {
            Rig           rig;
            DebugCommand  first;
            DebugCommand  more;



            first = rig.One ("300: A9 41 60");
            AssertVerb (DebugVerb::Deposit, first, "300: A9 41 60");
            Assert::AreEqual ((int) 0x300, (int) first.a1);
            Assert::IsTrue   (first.hasA1);
            Assert::AreEqual (size_t (3),  first.values.size());
            Assert::AreEqual ((int) 0xA9,  (int) first.values[0]);
            Assert::AreEqual ((int) 0x60,  (int) first.values[2]);

            //  A bare `:` continues from where the last deposit stopped.
            more = rig.One (": 01 02");
            AssertVerb (DebugVerb::Deposit, more, ": 01 02");
            Assert::AreEqual ((int) 0x303, (int) more.a1);
            Assert::AreEqual (size_t (2),  more.values.size());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Listing, moving, verifying
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (List_WithAnAddress_AndContinuing)
        {
            Rig           rig;
            DebugCommand  at   = rig.One ("300L");
            DebugCommand  next = rig.One ("L");



            AssertVerb (DebugVerb::List, at, "300L");
            Assert::AreEqual ((int) 0x300, (int) at.a1);
            Assert::IsTrue   (at.hasA1);

            AssertVerb (DebugVerb::List, next, "L");
            Assert::IsFalse (next.hasA1, L"a bare L continues from where the last listing stopped");
        }



        //  `dest<start.end` fills the same fields AppleWin's `M dest range`
        //  does -- destination in a3, source range in a1 and a2 -- so one
        //  handler serves both modes.
        TEST_METHOD (MoveAndVerify_TakeADestinationAndASourceRange)
        {
            Rig           rig;
            DebugCommand  move   = rig.One ("300<400.4FFM");
            DebugCommand  verify = rig.One ("300<400.4FFV");



            AssertVerb (DebugVerb::MoveMemory, move, "300<400.4FFM");
            Assert::AreEqual ((int) 0x300, (int) move.a3, L"the destination");
            Assert::IsTrue   (move.hasA3);
            Assert::AreEqual ((int) 0x400, (int) move.a1);
            Assert::AreEqual ((int) 0x4FF, (int) move.a2);

            AssertVerb (DebugVerb::Verify, verify, "300<400.4FFV");
            Assert::AreEqual ((int) 0x300, (int) verify.a3);
            Assert::AreEqual ((int) 0x4FF, (int) verify.a2);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The two S forms
        //
        ////////////////////////////////////////////////////////////////////////

        //  BARE `S` STEPS AND `value<range S` SEARCHES. The Monitor tells
        //  them apart by what the scan accumulated, not by the letter, and
        //  reading `41<300.3FFS` as a step would silently run the machine.
        TEST_METHOD (S_IsAStep_UnlessAValueAndRangeCameFirst)
        {
            Rig           rig;
            DebugCommand  step   = rig.One ("S");
            DebugCommand  atStep = rig.One ("300S");
            DebugCommand  search = rig.One ("41<300.3FFS");



            AssertVerb (DebugVerb::StepInto, step, "S");
            Assert::IsFalse  (step.hasA1);
            Assert::AreEqual ((uint32_t) 1, step.count);

            AssertVerb (DebugVerb::StepInto, atStep, "300S");
            Assert::IsTrue   (atStep.hasA1);
            Assert::AreEqual ((int) 0x300, (int) atStep.a1);

            AssertVerb (DebugVerb::SearchMemory, search, "41<300.3FFS");
            Assert::AreEqual ((int) 0x300, (int) search.a1);
            Assert::AreEqual ((int) 0x3FF, (int) search.a2);
            Assert::AreEqual (size_t (1),  search.values.size());
            Assert::AreEqual ((int) 0x41,  (int) search.values[0]);
        }



        TEST_METHOD (Trace_AndGo)
        {
            Rig           rig;
            DebugCommand  trace = rig.One ("300T");
            DebugCommand  go    = rig.One ("300G");



            AssertVerb (DebugVerb::Trace, trace, "300T");
            Assert::AreEqual ((int) 0x300, (int) trace.a1);
            Assert::IsTrue   (trace.hasA1);

            //  `addrG` RUNS AT the address; AppleWin's `G addr` runs TO it.
            //  The session already separates them by which field is set, so
            //  the address goes in a3.
            AssertVerb (DebugVerb::Go, go, "300G");
            Assert::AreEqual ((int) 0x300, (int) go.a3);
            Assert::IsTrue   (go.hasA3);
            Assert::IsFalse  (go.hasA1, L"a1 would make it run to the address instead");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Arithmetic, display and hooks
        //
        ////////////////////////////////////////////////////////////////////////

        //  Eight-bit, as the Monitor's is: FF+FF is FE.
        TEST_METHOD (Arithmetic_AddsAndSubtracts)
        {
            Rig           rig;
            DebugCommand  sum        = rig.One ("FF+FF");
            DebugCommand  difference = rig.One ("20-01");



            AssertVerb (DebugVerb::Arithmetic, sum, "FF+FF");
            Assert::AreEqual ((int) 0xFF, (int) sum.a1);
            Assert::AreEqual ((int) 0xFF, (int) sum.a2);
            Assert::AreEqual (std::string ("+"), sum.text);

            AssertVerb (DebugVerb::Arithmetic, difference, "20-01");
            Assert::AreEqual (std::string ("-"), difference.text);
        }



        TEST_METHOD (InverseAndNormal)
        {
            Rig  rig;



            AssertVerb (DebugVerb::SetInverse, rig.One ("I"), "I");
            AssertVerb (DebugVerb::SetNormal,  rig.One ("N"), "N");
        }



        TEST_METHOD (InputAndOutputHooks_TakeASlot)
        {
            Rig           rig;
            DebugCommand  input  = rig.One ("3^K");
            DebugCommand  output = rig.One ("3^P");
            DebugCommand  restoreIn = rig.One ("0^K");



            AssertVerb (DebugVerb::SetInputSlot, input, "3^K");
            Assert::AreEqual ((uint32_t) 3, input.count);

            AssertVerb (DebugVerb::SetOutputSlot, output, "3^P");
            Assert::AreEqual ((uint32_t) 3, output.count);

            //  Slot zero is the restore, and is a slot number like any other
            //  as far as the parse goes.
            AssertVerb (DebugVerb::SetInputSlot, restoreIn, "0^K");
            Assert::AreEqual ((uint32_t) 0, restoreIn.count);
        }



        TEST_METHOD (BasicEntriesAndTheUserVector)
        {
            Rig  rig;



            AssertVerb (DebugVerb::BasicColdStart, rig.One ("^B"), "^B");
            AssertVerb (DebugVerb::BasicWarmStart, rig.One ("^C"), "^C");
            AssertVerb (DebugVerb::UserVector,     rig.One ("^Y"), "^Y");
        }



        //  `^E` SHOWS THE REGISTERS AND ARMS THE NEXT `:`, which is the other
        //  half of the deposit test: the same character stores bytes or sets
        //  registers depending on what came before it.
        TEST_METHOD (ShowRegisters_ArmsTheColonThatSetsThem)
        {
            Rig           rig;
            DebugCommand  show;
            DebugCommand  edit;
            DebugCommand  afterwards;



            show = rig.One ("^E");
            AssertVerb (DebugVerb::ShowRegistersForEdit, show, "^E");
            Assert::IsTrue (rig.state.registerEditPending, L"the next colon sets registers");

            edit = rig.One (": 01 02 03");
            AssertVerb (DebugVerb::EditRegisters, edit, ": 01 02 03");
            Assert::AreEqual (size_t (3), edit.values.size());
            Assert::AreEqual ((int) 0x01, (int) edit.values[0]);
            Assert::IsFalse  (rig.state.registerEditPending, L"and only that one");

            //  The arming is spent, so the next colon deposits again.
            rig.One ("300: A9");
            afterwards = rig.One (": 41");
            AssertVerb (DebugVerb::Deposit, afterwards, ": 41");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The assembler and host files
        //
        ////////////////////////////////////////////////////////////////////////

        //  `F666G` IS AN ALIAS, NOT A JUMP. $F666 is the mini-assembler entry
        //  only on the original ][; on the other four machines those bytes
        //  are Applesoft, so jumping there would run the middle of a
        //  floating-point routine.
        TEST_METHOD (BangAndF666G_BothEnterTheAssembler)
        {
            Rig  rig;



            AssertVerb (DebugVerb::EnterAssembler, rig.One ("!"),     "!");
            AssertVerb (DebugVerb::EnterAssembler, rig.One ("F666G"), "F666G");
        }



        TEST_METHOD (ReadAndWrite_TakeAnOptionalFilename)
        {
            Rig           rig;
            DebugCommand  write  = rig.One ("800.9FFW out.bin");
            DebugCommand  read   = rig.One ("800.9FFR out.bin");
            DebugCommand  quoted = rig.One ("800.9FFW \"my file.bin\"");
            DebugCommand  bare   = rig.One ("800.9FFW");



            AssertVerb (DebugVerb::WriteFile, write, "800.9FFW out.bin");
            Assert::AreEqual ((int) 0x800, (int) write.a1);
            Assert::AreEqual ((int) 0x9FF, (int) write.a2);
            Assert::AreEqual (std::string ("out.bin"), write.text);

            AssertVerb (DebugVerb::ReadFile, read, "800.9FFR out.bin");
            Assert::AreEqual (std::string ("out.bin"), read.text);

            Assert::AreEqual (std::string ("my file.bin"), quoted.text, L"the quotes are not part of the name");

            //  No name parses; batch and the channel answer it with an error,
            //  and the window prompts.
            Assert::IsTrue (bare.text.empty());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  How a line is read
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SeveralCommandsOnOneLine)
        {
            Rig                 rig;
            MonitorParseResult  result = rig.Parse ("300.30F 400.40F");



            Assert::IsTrue   (result.status == ParseStatus::Ok, Widen (result.error).c_str());
            Assert::AreEqual (size_t (2), result.commands.size());
            Assert::AreEqual ((int) 0x300, (int) result.commands[0].a1);
            Assert::AreEqual ((int) 0x400, (int) result.commands[1].a1);
        }



        //  A control command arrives as the control character from a terminal
        //  and as `^` plus the letter from a script, in either case.
        TEST_METHOD (ControlCharacters_AsTheCharacterAndAsCaret)
        {
            Rig  rig;



            AssertVerb (DebugVerb::ShowRegistersForEdit, rig.One ("\x05"), "Ctrl+E");
            AssertVerb (DebugVerb::ShowRegistersForEdit, rig.One ("^E"),   "^E");
            AssertVerb (DebugVerb::ShowRegistersForEdit, rig.One ("^e"),   "^e");
        }



        TEST_METHOD (LowercaseInput_ReadsAsUppercase)
        {
            Rig           rig;
            DebugCommand  list = rig.One ("300l");
            DebugCommand  move = rig.One ("300<400.4ffm");



            AssertVerb (DebugVerb::List, list, "300l");
            Assert::AreEqual ((int) 0x300, (int) list.a1);

            AssertVerb (DebugVerb::MoveMemory, move, "300<400.4ffm");
            Assert::AreEqual ((int) 0x4FF, (int) move.a2, L"lowercase hex digits too");
        }



        //  A `/` line was never the Monitor's: it is handed on with no
        //  commands and no error.
        TEST_METHOD (SlashLine_IsHandedToTheAppleWinParser)
        {
            Rig                 rig;
            MonitorParseResult  result = rig.Parse ("/bpl");



            Assert::AreEqual (std::string ("bpl"), result.appleWinLine);
            Assert::IsTrue   (result.commands.empty());
            Assert::IsTrue   (result.error.empty());
        }



        TEST_METHOD (AMalformedLine_IsAnErrorAndNoCommand)
        {
            Rig                 rig;
            MonitorParseResult  result = rig.Parse ("300ZZ");



            Assert::IsTrue  (result.status == ParseStatus::Invalid);
            Assert::IsTrue  (result.commands.empty(), L"nothing runs from a line that did not parse");
            Assert::IsFalse (result.error.empty());
        }
    };
}
