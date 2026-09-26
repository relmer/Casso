#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/TestMachine.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"
#include "TestHelpers.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorHandlersTests
    //
    //  Story 2's scenarios, driven as a reader drives them: whole lines typed
    //  at a `*` prompt, through the real parser, the real handlers and the
    //  real formatter.
    //
    //  THE WHOLE COMMAND SET IS ATTACHED, not just the Monitor family, because
    //  that is the claim being tested. `41<300.3FFS` reaches the same search
    //  MemoryHandlers runs for AppleWin's `S`, and `/bpl` lists a breakpoint
    //  BreakpointHandlers set -- one engine behind two syntaxes, which is only
    //  true if the families really do share it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorHandlersTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Rig
        //
        //  A session over the mock target in Monitor mode, with every command
        //  family attached.
        //
        ////////////////////////////////////////////////////////////////////////

        struct Rig
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            InMemoryFileSystem         files;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;

            Rig()
            {
                handlers.Attach (session);
                session.SetFileSystem       (&files);
                session.SetCurrentDirectory (L"C:\\Work");

                //  Into Monitor mode the way a reader gets there.
                session.ExecuteLine ("MODE MONITOR");
            }

            Reply Run (const std::string & line)
            {
                Reply  reply = session.ExecuteLine (line);

                session.FormatReply (reply);
                return reply;
            }

            Reply RunOk (const std::string & line)
            {
                Reply  reply = Run (line);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status,
                                  Widen (line + ": " + reply.error.label + ", " + reply.error.detail).c_str());
                return reply;
            }

            std::string Line (const std::string & command, size_t index)
            {
                Reply  reply = RunOk (command);

                Assert::IsTrue (reply.text.size() > index, Widen (command + ": no line " + std::to_string (index)).c_str());
                return reply.text[index];
            }
        };



        //  L on its own goes on from where the last listing ended, as the
        //  Monitor's does, not from $0000.
        TEST_METHOD (List_WithoutAddress_ContinuesFromTheLastListing)
        {
            Rig          rig;
            TestCpu      cpu;
            std::string  line;



            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();
            rig.RunOk ("300: EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA");
            rig.RunOk ("300L");

            line = rig.Line ("L", 0);
            Assert::IsTrue (line.starts_with ("0314"), Widen (line).c_str());
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  Scenario 1: examine
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Examine_PrintsMemoryInMonitorFormat)
        {
            Rig  rig;



            rig.RunOk ("300: A9 00 8D 00 03 60");

            Assert::AreEqual (std::string ("0300- A9 00 8D 00 03 60"), rig.Line ("300.305", 0));
        }



        //  A line of several commands renders each in the output format a
        //  one-command line gets, not always as Monitor text.
        TEST_METHOD (SeveralCommands_FollowTheOutputFormat)
        {
            Rig    rig;
            Reply  single;
            Reply  both;



            rig.RunOk ("300: A9 00 8D 00 03 60");
            rig.RunOk ("/OUTPUT APPLEWIN");

            single = rig.RunOk ("300.305");
            both   = rig.RunOk ("300.305 300.305");

            Assert::IsFalse (single.text.empty());
            Assert::AreEqual (single.text.size() * 2, both.text.size());
            Assert::AreEqual (single.text.front(), both.text.front());
        }



        //  The Monitor's arithmetic is eight-bit; CALC keeps its whole value.
        TEST_METHOD (Calc_KeepsTheHighByte_ArithmeticDoesNot)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("=1234"), rig.Line ("/CALC 1234", 0));
            Assert::AreEqual (std::string ("=34"),   rig.Line ("/CALC 34",   0));
            Assert::AreEqual (std::string ("=FE"),   rig.Line ("FF+FF",      0));
        }



        //  Rows break on eight-byte boundaries, so a range that starts mid-row
        //  prints a short row first.
        TEST_METHOD (Examine_BreaksRowsOnEightByteBoundaries)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("300: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
            reply = rig.RunOk ("303.30F");

            Assert::AreEqual (size_t (2), reply.text.size());
            Assert::AreEqual (std::string ("0303- 03 04 05 06 07"),          reply.text[0]);
            Assert::AreEqual (std::string ("0308- 08 09 0A 0B 0C 0D 0E 0F"), reply.text[1]);
        }



        //  A bare Return continues from where the last one stopped.
        TEST_METHOD (Examine_ContinuesOnABareReturn)
        {
            Rig  rig;



            rig.RunOk ("300: 00 01 02 03 04 05 06 07 08");
            rig.RunOk ("300");

            //  Seven bytes, not eight: the row ends at the $0307 boundary.
            Assert::AreEqual (std::string ("0301- 01 02 03 04 05 06 07"), rig.Line ("", 0));

            //  And the next one is a whole row from the boundary.
            Assert::AreEqual (std::string ("0308- 08"), rig.Line ("", 0).substr (0, 8));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Scenario 3: search
        //
        ////////////////////////////////////////////////////////////////////////

        //  Through the same search AppleWin's `S` runs, which is the point:
        //  the syntax differs and the engine does not.
        TEST_METHOD (Search_PrintsEveryMatchingAddress)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("300: 41 00 41");
            reply = rig.RunOk ("41<300.302S");

            Assert::AreEqual (size_t (2), reply.text.size());
            Assert::AreEqual (std::string ("0300"), reply.text[0]);
            Assert::AreEqual (std::string ("0302"), reply.text[1]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Scenario 4: show and change the registers
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShowRegisters_ThenSetThemWithAColon)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("A=00 X=00 Y=00 P=30 S=FF"), rig.Line ("^E", 0));

            rig.RunOk (": 01 02 03");

            Assert::AreEqual ((int) 0x01, (int) rig.target.registers.a);
            Assert::AreEqual ((int) 0x02, (int) rig.target.registers.x);
            Assert::AreEqual ((int) 0x03, (int) rig.target.registers.y);

            //  And the same bytes land where the ROM keeps them, for a guest
            //  that reads them back.
            Assert::AreEqual ((int) 0x01, (int) rig.target.memory[0x45]);
            Assert::AreEqual ((int) 0x03, (int) rig.target.memory[0x47]);
        }



        //  The arming is spent by the one colon that follows, so the next one
        //  deposits into memory again.
        TEST_METHOD (TheColonAfterAnEditDepositsAgain)
        {
            Rig  rig;



            rig.RunOk ("^E");
            rig.RunOk (": 01 02 03");
            rig.RunOk ("300: 41");
            rig.RunOk (": 42");

            Assert::AreEqual ((int) 0x41, (int) rig.target.memory[0x300]);
            Assert::AreEqual ((int) 0x42, (int) rig.target.memory[0x301]);
        }



        //  A register edit that did not happen, because the machine was
        //  running, does not spend the arming: after a pause the same colon
        //  sets the registers rather than storing into zero page.
        TEST_METHOD (AnEditWhileRunning_KeepsTheColonArmed)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("^E");
            rig.session.OnUserResumed();

            reply = rig.Run (": 01 02 03");
            Assert::AreEqual (std::string ("machine running"), reply.error.label);

            rig.session.OnUserPaused();
            rig.RunOk (": 01 02 03");

            Assert::AreEqual ((int) 0x01, (int) rig.target.GetRegisters().a);
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x0000], L"nothing stored into zero page");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Scenario 5: host files
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (WriteThenReadAHostFile)
        {
            Rig          rig;
            std::string  saved;



            rig.RunOk ("300: 01 02 03 04");
            rig.RunOk ("300.303W out.bin");

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\out.bin", saved));
            Assert::AreEqual (size_t (4), saved.size());

            rig.RunOk ("300: 00 00 00 00");
            rig.RunOk ("300.303R out.bin");

            Assert::AreEqual ((int) 0x01, (int) rig.target.memory[0x300]);
            Assert::AreEqual ((int) 0x04, (int) rig.target.memory[0x303]);
        }



        //  A name with spaces in it is quoted, and the quotes are not part of
        //  the name.
        TEST_METHOD (AQuotedFileName_KeepsItsSpaces)
        {
            Rig  rig;



            rig.RunOk ("300: 01 02");
            rig.RunOk ("300.301W \"my file.bin\"");

            Assert::IsTrue (rig.files.Exists (L"C:\\Work\\my file.bin"));
        }



        //  A session with no file system gives the same message here as every
        //  other command family that reads or writes host files.
        TEST_METHOD (WriteWithNoFileSystem_GivesTheSharedMessage)
        {
            Rig    rig;
            Reply  reply;



            rig.session.SetFileSystem (nullptr);
            reply = rig.Run ("300.301W out.bin");

            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("This session cannot read or write host files."), reply.error.detail);
        }



        //  Without a name, batch and the channel have nobody to ask.
        TEST_METHOD (WriteWithNoFileName_IsAnError)
        {
            Rig    rig;
            Reply  reply = rig.Run ("300.301W");



            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("ERR"), reply.text.at (0));
        }



        //  R and W act on a range, first to last. A line without both ends is
        //  an error, and neither memory nor the file is touched.
        TEST_METHOD (ReadAndWrite_WithoutARange_AreErrors)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\in.bin", std::string ("\x11\x22\x33", 3));

            for (const char * line : { "R in.bin", "300R in.bin", "W out.bin", "300W out.bin" })
            {
                reply = rig.Run (line);
                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, Widen (line).c_str());
                Assert::AreEqual (std::string ("invalid arguments"), reply.error.label, Widen (line).c_str());
            }

            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x0000]);
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x0300]);
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x03FF]);
            Assert::IsFalse  (rig.files.Exists (L"C:\\Work\\out.bin"));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Scenario 6: one session behind both syntaxes
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ABreakpointSetInAppleWinMode_IsListedFromMonitorMode)
        {
            Rig    rig;
            Reply  listed;



            //  Set it in AppleWin mode, then read it back from Monitor mode
            //  through the `/` prefix.
            //
            //  THE SWITCH ITSELF NEEDS THE SLASH. A bare `MODE APPLEWIN`
            //  typed at a Monitor prompt is not a mode switch: `M` is the
            //  Monitor's move command, and the line reads as one.
            rig.RunOk ("/MODE APPLEWIN");
            rig.RunOk ("BP 300");
            rig.RunOk ("MODE MONITOR");

            listed = rig.RunOk ("/bpl");

            Assert::IsTrue (listed.text.at (0).find ("$0300") != std::string::npos,
                            Widen (listed.text.at (0)).c_str());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The display and hook commands
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (InverseAndNormal_SetTheInverseFlag)
        {
            Rig  rig;



            rig.RunOk ("I");
            Assert::AreEqual ((int) 0x3F, (int) rig.target.memory[0x32]);

            rig.RunOk ("N");
            Assert::AreEqual ((int) 0xFF, (int) rig.target.memory[0x32]);
        }



        TEST_METHOD (TheInputAndOutputHooks_PointAtASlot)
        {
            Rig  rig;



            rig.RunOk ("3^K");
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x38]);
            Assert::AreEqual ((int) 0xC3, (int) rig.target.memory[0x39]);

            rig.RunOk ("3^P");
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x36]);
            Assert::AreEqual ((int) 0xC3, (int) rig.target.memory[0x37]);

            //  Slot zero restores the ROM's own keyboard and screen.
            rig.RunOk ("0^K");
            Assert::AreEqual ((int) 0x1B, (int) rig.target.memory[0x38]);
            Assert::AreEqual ((int) 0xFD, (int) rig.target.memory[0x39]);

            rig.RunOk ("0^P");
            Assert::AreEqual ((int) 0xF0, (int) rig.target.memory[0x36]);
            Assert::AreEqual ((int) 0xFD, (int) rig.target.memory[0x37]);
        }



        //  The ROM keeps only the low four bits of the number, so every
        //  value is a slot: 8 is $C800, $13 is slot 3, and $10 is slot 0.
        TEST_METHOD (TheHookSlot_IsTheLowFourBitsOfTheNumber)
        {
            Rig  rig;



            rig.RunOk ("8^K");
            Assert::AreEqual ((int) 0xC8, (int) rig.target.memory[0x39]);

            rig.RunOk ("13^K");
            Assert::AreEqual ((int) 0xC3, (int) rig.target.memory[0x39]);

            rig.RunOk ("10^P");
            Assert::AreEqual ((int) 0xF0, (int) rig.target.memory[0x36]);
            Assert::AreEqual ((int) 0xFD, (int) rig.target.memory[0x37]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  A range whose end is before its start
        //
        //  The ROM's loops test the end after each byte, so the start byte is
        //  always handled once.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Examine_WithTheEndBeforeTheStart_ShowsTheStartByte)
        {
            Rig  rig;



            rig.RunOk ("300: 41");

            Assert::AreEqual (std::string ("0300- 41"), rig.Line ("300.200", 0));
        }



        TEST_METHOD (Verify_WithTheEndBeforeTheStart_ComparesTheStartByte)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("300: 41");
            rig.RunOk ("400: 42");

            reply = rig.RunOk ("400<300.200V");

            Assert::AreEqual (size_t (1), reply.text.size());
            Assert::AreEqual (std::string ("0300-41 (42)"), reply.text[0]);
        }



        TEST_METHOD (Write_WithTheEndBeforeTheStart_WritesTheStartByte)
        {
            Rig          rig;
            std::string  saved;



            rig.RunOk ("300: 41");
            rig.RunOk ("300.200W one.bin");

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\one.bin", saved));
            Assert::AreEqual (std::string ("\x41"), saved);
        }



        //  One byte, not the whole file wrapped through $FFFF.
        TEST_METHOD (Read_WithTheEndBeforeTheStart_ReadsTheStartByte)
        {
            Rig  rig;



            rig.files.WriteAllText (L"C:\\Work\\three.bin", std::string ("\x01\x02\x03"));
            rig.RunOk ("300.200R three.bin");

            Assert::AreEqual ((int) 0x01, (int) rig.target.memory[0x300]);
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x301]);
        }



        //  A file longer than the range is cut to it, and the reply says the
        //  sizes differ.
        TEST_METHOD (Read_AFileLongerThanTheRange_ReportsTheSizeDifference)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\four.bin", std::string ("\x01\x02\x03\x04"));
            reply = rig.RunOk ("300.301R four.bin");

            Assert::IsTrue   (std::get<FileIoData> (reply.data).mismatch);
            Assert::AreEqual ((int) 0x00, (int) rig.target.memory[0x302]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Arithmetic and verify
        //
        ////////////////////////////////////////////////////////////////////////

        //  Eight-bit, as the Monitor's is.
        TEST_METHOD (Arithmetic_IsEightBit)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("=FE"), rig.Line ("FF+FF", 0));
            Assert::AreEqual (std::string ("=1F"), rig.Line ("20-01", 0));
        }



        TEST_METHOD (Verify_ReportsOnlyTheDifferences)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("300: 41 42 43");
            rig.RunOk ("400: 41 99 43");

            reply = rig.RunOk ("400<300.302V");

            Assert::AreEqual (size_t (1), reply.text.size());
            Assert::AreEqual (std::string ("0301-42 (99)"), reply.text[0]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Several commands on one line
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SeveralCommandsOnOneLine_PrintInOrder)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("300: 41");
            rig.RunOk ("400: 42");

            reply = rig.RunOk ("300 400");

            Assert::AreEqual (size_t (2), reply.text.size());
            Assert::AreEqual (std::string ("0300- 41"), reply.text[0]);
            Assert::AreEqual (std::string ("0400- 42"), reply.text[1]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorRunTests
    //
    //  The scenarios that need a machine that really executes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorRunTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        struct MachineRig
        {
            TestMachine                machine;
            MachineDebugTarget         target;
            SynchronousRunDriver       driver;
            RecordingNotificationSink  sink;
            InMemoryFileSystem         files;
            DebugSession               session;
            DebugHandlerSet            handlers;

            static constexpr Byte  kStackTop   = 0xFF;
            static constexpr Byte  kIrqMasked  = 0x34;

            MachineRig() :
                machine ("Apple2e", TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, target.GetRunHook()),
                session (target, sink, RunState::Paused)
            {
                Cpu6502Registers  registers;

                target.SetRunDriver (&driver);
                machine.PowerCycle();

                //  A POWER-CYCLED MACHINE HAS NO USABLE STACK POINTER. It is
                //  whatever the power-on pattern left, and a Monitor `G`
                //  pushes its return address onto that stack; with SP near
                //  zero the push wraps and the RTS pops two unrelated bytes,
                //  so the program returns somewhere arbitrary and the run
                //  never ends. The real machine has run its ROM's reset by
                //  the time a reader types anything, and this stands in for
                //  that.
                registers    = target.GetRegisters();
                registers.sp = kStackTop;
                registers.p  = kIrqMasked;
                target.SetRegisters (registers);

                handlers.Attach (session);
                session.SetFileSystem (&files);

                //  No test may run unbounded. A run that does not reach its
                //  stop hangs the whole suite rather than failing, which is a
                //  far worse way to find out.
                session.ExecuteLine ("BUDGET 2000000");
                session.ExecuteLine ("MODE MONITOR");
            }

            Reply Run (const std::string & line)
            {
                Reply  reply = session.ExecuteLine (line);

                session.FormatReply (reply);
                return reply;
            }
        };



        //  The `!` assembler takes `addr:MNE operand`, continues with a bare
        //  instruction, and runs a Monitor command given as `$cmd`.
        TEST_METHOD (Assembler_TakesAnAddressPrefixAndDollarCommands)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            rig.Run ("!");

            reply = rig.Run ("300:LDA #41");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());

            reply = rig.Run ("RTS");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());

            rig.target.TryPeek (0x0300, value);
            Assert::AreEqual ((int) 0xA9, (int) value);
            rig.target.TryPeek (0x0301, value);
            Assert::AreEqual ((int) 0x41, (int) value);
            rig.target.TryPeek (0x0302, value);
            Assert::AreEqual ((int) 0x60, (int) value);

            reply = rig.Run ("$310: 42");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());
            rig.target.TryPeek (0x0310, value);
            Assert::AreEqual ((int) 0x42, (int) value);
            Assert::IsTrue   (rig.session.IsAssembling(), L"a $ command leaves the assembler active");

            rig.Run ("");
            Assert::IsFalse (rig.session.IsAssembling());
        }



        //  An assembly line writes memory, so it needs a paused machine.
        TEST_METHOD (Assembler_LineWhileRunning_IsAnError)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            rig.Run ("300: EA");
            rig.Run ("!");
            rig.session.OnUserResumed();

            reply = rig.Run ("300:LDA #41");
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("machine running"), reply.error.label);

            rig.target.TryPeek (0x0300, value);
            Assert::AreEqual ((int) 0xEA, (int) value);
        }



        //  I, N, ^K and ^P write zero page, so they need a paused machine.
        TEST_METHOD (ZeroPageSettersWhileRunning_AreErrors)
        {
            MachineRig  rig;
            Byte        value = 0;



            rig.target.TryPoke (0x0032, 0xFF);
            rig.target.TryPoke (0x0036, 0x12);
            rig.target.TryPoke (0x0038, 0x34);
            rig.session.OnUserResumed();

            for (const char * line : { "I", "N", "6^K", "6^P" })
            {
                Assert::AreEqual (std::string ("machine running"), rig.Run (line).error.label);
            }

            rig.target.TryPeek (0x0032, value);
            Assert::AreEqual ((int) 0xFF, (int) value);
            rig.target.TryPeek (0x0036, value);
            Assert::AreEqual ((int) 0x12, (int) value);
            rig.target.TryPeek (0x0038, value);
            Assert::AreEqual ((int) 0x34, (int) value);
        }



        //  I, N, ^K and ^P write zero page, and a run from an address sets PC
        //  (and G pushes the Monitor's return), so none of them may act while
        //  the machine runs freely.
        TEST_METHOD (ZeroPageWritesAndRunsFromAnAddress_WaitForAPausedMachine)
        {
            MachineRig        rig;
            Reply             reply;
            Cpu6502Registers  before;
            Byte              value  = 0;
            Byte              stack  = 0;



            rig.target.TryPoke (0x0032, 0x5A);
            rig.target.TryPoke (0x0036, 0x5A);
            rig.target.TryPoke (0x0038, 0x5A);
            rig.target.TryPoke (0x01FF, 0x5A);
            rig.session.OnUserResumed();
            before = rig.target.GetRegisters();

            for (const char * line : { "I", "N", "3^K", "3^P", "300G", "300S", "300T", "^Y" })
            {
                reply = rig.Run (line);
                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, Widen (line).c_str());
                Assert::AreEqual (std::string ("machine running"), reply.error.label, Widen (line).c_str());
            }

            rig.target.TryPeek (0x0032, value);
            Assert::AreEqual ((int) 0x5A, (int) value, L"INVFLG");
            rig.target.TryPeek (0x0036, value);
            Assert::AreEqual ((int) 0x5A, (int) value, L"CSWL");
            rig.target.TryPeek (0x0038, value);
            Assert::AreEqual ((int) 0x5A, (int) value, L"KSWL");
            rig.target.TryPeek (0x01FF, stack);
            Assert::AreEqual ((int) 0x5A, (int) stack, L"no return address pushed");
            Assert::AreEqual ((int) before.sp, (int) rig.target.GetRegisters().sp);
            Assert::AreEqual ((int) before.pc, (int) rig.target.GetRegisters().pc);
        }



        //  An instruction that does not fit in writable memory writes none of
        //  its bytes, and the error gives the byte that could not be written.
        TEST_METHOD (Assembler_UnwritableByte_WritesNothingAndGivesItsAddress)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            rig.Run ("BFFF: EA");
            rig.Run ("!");

            reply = rig.Run ("BFFF:JMP 1234");
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("memory not writable"), reply.error.label);
            Assert::IsTrue   (reply.error.detail.find ("$C000") != std::string::npos, Widen (reply.error.detail).c_str());

            rig.target.TryPeek (0xBFFF, value);
            Assert::AreEqual ((int) 0xEA, (int) value);
        }



        //  A deposit that runs past $FFFF wraps to $0000, and its reply shows
        //  both bytes it wrote. Two reads of $C083 make the language card's
        //  RAM writable, as a guest does, so $FFFF takes a byte.
        TEST_METHOD (Deposit_ThatWrapsPastFFFF_ShowsEveryByte)
        {
            static constexpr Word  kLcBank2RamWrite = 0xC083;
            MachineRig             rig;
            Reply                  reply;
            Byte                   value            = 0;



            rig.machine.GetMemoryBus().ReadByte (kLcBank2RamWrite);
            rig.machine.GetMemoryBus().ReadByte (kLcBank2RamWrite);

            reply = rig.Run ("FFFF: 01 02");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());

            rig.target.TryPeek (0xFFFF, value);
            Assert::AreEqual ((int) 0x01, (int) value);
            rig.target.TryPeek (0x0000, value);
            Assert::AreEqual ((int) 0x02, (int) value);

            Assert::AreEqual (size_t (2), reply.text.size());
            Assert::AreEqual (std::string ("FFFF- 01"), reply.text[0]);
            Assert::AreEqual (std::string ("0000- 02"), reply.text[1]);
        }



        //  Scenario 2: the //e's ROM has no step command, and Monitor mode
        //  steps on it anyway, because the step is Casso's and not the ROM's.
        TEST_METHOD (Step_WorksOnAMachineWhoseRomHasNoStepCommand)
        {
            MachineRig  rig;



            //  LDA #$41 at $0300.
            rig.Run ("300: A9 41");
            rig.Run ("300S");

            Assert::IsFalse  (rig.sink.stops.empty(), L"the step must report a stop");
            Assert::AreEqual ((int) 0x0302, (int) rig.target.GetRegisters().pc, L"one instruction, and the PC moved past it");
            Assert::AreEqual ((int) 0x41,   (int) rig.target.GetRegisters().a);
        }



        //  The original ]['s step display: the instruction the step ran, then
        //  the registers from the stop.
        TEST_METHOD (Step_ReplyCarriesTheInstructionItRan)
        {
            MachineRig  rig;
            Reply       reply;



            rig.Run ("300: A9 41");
            reply = rig.Run ("300S");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());
            Assert::IsFalse  (reply.text.empty(), L"the step prints its instruction");
            Assert::IsTrue   (reply.text[0].find ("0300-") == 0, Widen (reply.text[0]).c_str());
            Assert::IsTrue   (reply.text[0].find ("LDA") != std::string::npos, Widen (reply.text[0]).c_str());
        }



        //  On the ROM a command character after a deposit's bytes ends store
        //  mode and runs, so a deposit and a jump share one line.
        TEST_METHOD (Deposit_ACommandAfterTheBytesRuns)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            //  LDA #$41 / RTS, then run it.
            reply = rig.Run ("300:A9 41 60 N 300G");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());

            rig.target.TryPeek (0x0302, value);
            Assert::AreEqual ((int) 0x60, (int) value);
            Assert::AreEqual ((int) 0x41, (int) rig.target.GetRegisters().a, L"the G after the bytes ran");
        }



        //  `300!` assembles at $0300, not at the program counter.
        TEST_METHOD (Assembler_StartsAtTheTypedAddress)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            rig.Run ("300!");
            reply = rig.Run ("LDA #41");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());

            rig.target.TryPeek (0x0300, value);
            Assert::AreEqual ((int) 0xA9, (int) value);
            rig.Run ("");
        }



        //  The running-machine error reads as a sentence whatever was typed:
        //  a deposit's source is only a colon.
        TEST_METHOD (Deposit_WhileRunning_ErrorReadsAsASentence)
        {
            MachineRig  rig;
            Reply       reply;



            rig.session.OnUserResumed();
            reply = rig.Run ("300: EA");

            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::IsFalse  (reply.error.detail.starts_with (":"), Widen (reply.error.detail).c_str());
        }



        //  A slash with nothing after it is an error, not a silent success.
        TEST_METHOD (SlashAlone_IsAnError)
        {
            MachineRig  rig;



            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("/").status);
            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("/   ").status);
        }



        //  A Monitor `G` leaves the Monitor's return address on the stack, so
        //  a program ending in RTS comes back instead of running on.
        TEST_METHOD (Go_ReturnsToTheMonitorOnRts)
        {
            MachineRig  rig;



            //  LDA #$41 / RTS
            rig.Run ("300: A9 41 60");
            rig.Run ("300G");

            Assert::IsFalse  (rig.sink.stops.empty(), L"the run must report a stop");
            Assert::AreEqual ((int) 0xFF69, (int) rig.sink.stops.back().pc, L"stopped where the Monitor is re-entered");
            Assert::AreEqual ((int) 0x41,   (int) rig.target.GetRegisters().a, L"and the program really ran");
        }



        //  THE CPU IS THE TRUTH. The ROM's own G reloads A, X, Y, P and S from
        //  $45-$49 before running; Casso's does not, so a register set in
        //  AppleWin mode survives a Monitor G.
        TEST_METHOD (Go_DoesNotReloadRegistersFromZeroPage)
        {
            MachineRig        rig;
            Cpu6502Registers  before;
            Cpu6502Registers  registers;
            Reply             reply;



            //  The slash is what reaches AppleWin mode from here; a bare
            //  `MODE APPLEWIN` would read as Monitor commands.
            rig.Run ("/MODE APPLEWIN");
            rig.Run ("MEB 300 EA 60");          // NOP / RTS
            rig.Run ("R A 41");
            rig.Run ("R X 22");
            rig.Run ("R Y 33");
            rig.Run ("MEB 45 99 88 77 00 10");  // A, X, Y, P and S as the ROM would load them
            rig.Run ("MODE MONITOR");

            before = rig.target.GetRegisters();
            reply  = rig.Run ("300G");

            registers = rig.target.GetRegisters();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::IsFalse  (rig.sink.stops.empty(), L"the run must report a stop");
            Assert::AreEqual ((int) 0xFF69,    (int) rig.sink.stops.back().pc, L"the program ran and returned");
            Assert::AreEqual ((int) 0x41,      (int) registers.a,  L"the register the reader set, not the zero-page byte");
            Assert::AreEqual ((int) 0x22,      (int) registers.x);
            Assert::AreEqual ((int) 0x33,      (int) registers.y);
            Assert::AreEqual ((int) before.p,  (int) registers.p);
            Assert::AreEqual ((int) before.sp, (int) registers.sp);
        }



        //  A blank line in a script is nothing, as it is in a batch; typed at
        //  the prompt in Monitor mode it would print the next memory row.
        TEST_METHOD (Script_BlankLines_PrintNothing)
        {
            MachineRig  rig;
            Reply       reply;



            rig.files.WriteAllText (L"C:\\Work\\rows.txt", "300.307\n\n  \r\n\n");

            reply = rig.Run ("/RUN C:\\Work\\rows.txt");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());
            Assert::AreEqual ((size_t) 1, reply.text.size(), L"the one examined row, and no rows after it");
            Assert::IsTrue   (reply.text[0].starts_with ("0300-"), Widen (reply.text[0]).c_str());
        }



        //  Inside an assembler block a blank line in a script ends the block,
        //  so the line after it is a command again.
        TEST_METHOD (Script_BlankLineInAssemblerBlock_EndsTheBlock)
        {
            MachineRig  rig;
            Reply       reply;
            Byte        value = 0;



            rig.files.WriteAllText (L"C:\\Work\\asm.txt", "!\n300:LDA #41\nRTS\n\n300.302\n");

            reply = rig.Run ("/RUN C:\\Work\\asm.txt");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());
            Assert::IsFalse  (rig.session.IsAssembling(), L"the blank line ended the block");

            rig.target.TryPeek (0x0300, value);
            Assert::AreEqual ((int) 0xA9, (int) value);

            Assert::IsFalse  (reply.text.empty());
            Assert::AreEqual (std::string ("0300- A9 41 60"), reply.text.back(), L"the line after the block examines memory");
        }



        static DisassemblyData GetListing (const Reply & reply)
        {
            Assert::IsTrue (std::holds_alternative<DisassemblyData> (reply.data), L"the reply is not a listing");
            return std::get<DisassemblyData> (reply.data);
        }



        //  A bare L lists on from where the last listing stopped.
        TEST_METHOD (List_WithNoAddress_ContinuesFromTheLastListing)
        {
            MachineRig       rig;
            DisassemblyData  listing;



            rig.Run ("300: EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA");
            rig.Run ("300L");

            listing = GetListing (rig.Run ("L"));

            Assert::AreEqual (size_t (20), listing.lines.size());
            Assert::AreEqual ((int) 0x0314, (int) listing.lines.front().instruction.address);
        }



        //  The ROM's L ignores the end of a range and always lists twenty.
        TEST_METHOD (List_WithARange_ListsTwentyInstructions)
        {
            MachineRig       rig;
            DisassemblyData  listing;



            rig.Run ("300: EA EA EA EA");

            listing = GetListing (rig.Run ("300.302L"));

            Assert::AreEqual (size_t (20), listing.lines.size());
            Assert::AreEqual ((int) 0x0300, (int) listing.lines.front().instruction.address);
        }



        //  With the language card's RAM write-enabled, a deposit can wrap from
        //  $FFFF to $0000, and the reply shows the bytes on both sides.
        TEST_METHOD (Deposit_PastTheTopOfMemory_ShowsTheWrappedBytes)
        {
            MachineRig  rig;
            Reply       reply;



            //  LDA $C08B twice write-enables the card's RAM; RTS returns.
            rig.Run ("300: AD 8B C0 AD 8B C0 60");
            rig.Run ("300G");

            reply = rig.Run ("FFFF: 41 42");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (reply.error.detail).c_str());
            Assert::AreEqual (size_t (2), reply.text.size());
            Assert::AreEqual (std::string ("FFFF- 41"), reply.text[0]);
            Assert::AreEqual (std::string ("0000- 42"), reply.text[1]);
        }



        //  A bare L picks up where the last listing stopped, as the help
        //  says, rather than listing from $0000.
        TEST_METHOD (List_Bare_ContinuesFromTheLastListing)
        {
            MachineRig               rig;
            Reply                    first;
            Reply                    next;
            const DisassemblyData  * listed = nullptr;
            const DisassemblyData  * more   = nullptr;
            Word                     after  = 0;
            std::string              nops   = "300:";



            //  Forty-eight NOPs, enough for two listings of twenty.
            for (int i = 0; i < 48; i++)
            {
                nops += " EA";
            }

            rig.Run (nops);
            first  = rig.Run ("300L");
            listed = std::get_if<DisassemblyData> (&first.data);
            Assert::IsNotNull (listed);
            Assert::IsFalse   (listed->lines.empty());

            after = (Word) (listed->lines.back().instruction.address + listed->lines.back().instruction.bytes.size());

            next = rig.Run ("L");
            more = std::get_if<DisassemblyData> (&next.data);
            Assert::IsNotNull (more);
            Assert::IsFalse   (more->lines.empty());
            Assert::AreEqual  ((int) after, (int) more->lines.front().instruction.address);
        }



        //  A line of several commands where one fails shows the error once.
        TEST_METHOD (SeveralCommands_WithAFailure_ShowTheErrorOnce)
        {
            MonitorHandlersTests::Rig rig;
            Reply   reply = rig.Run ("300.300 300.301W");
            size_t  count = 0;



            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);

            for (const std::string & line : reply.text)
            {
                count += line == "ERR" ? 1 : 0;
            }

            Assert::AreEqual ((size_t) 1, count);
        }



        //  A line of several commands renders in the output format in force,
        //  the same as a line of one.
        TEST_METHOD (SeveralCommands_FollowTheOutputFormat)
        {
            MonitorHandlersTests::Rig  rig;
            Reply                      one;
            Reply                      two;



            rig.RunOk ("/OUTPUT APPLEWIN");
            one = rig.RunOk ("300.300");
            two = rig.RunOk ("300.300 300.300");

            Assert::IsFalse  (two.text.empty());
            Assert::AreEqual (one.text.front(), two.text.front());
        }



        //  A GSSquared line run in its own mode on a session in another mode
        //  renders in GSSquared's format, whether it is one command or several.
        TEST_METHOD (SeveralGSSquaredCommands_FollowTheLinesMode)
        {
            MonitorHandlersTests::Rig  rig;
            Reply                      one;
            Reply                      two;



            rig.RunOk ("/MODE APPLEWIN");

            one = rig.session.ExecuteLine ("watch 300", CommandMode::GSSquared);
            rig.session.FormatReply (one, CommandMode::GSSquared);
            two = rig.session.ExecuteLine ("watch 301.302", CommandMode::GSSquared);
            rig.session.FormatReply (two, CommandMode::GSSquared);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) two.status);
            Assert::IsFalse  (one.text.empty());
            Assert::IsFalse  (two.text.empty());
            Assert::AreEqual (one.text.front().substr (0, 6), two.text.front().substr (0, 6));
        }



        //  An address in front of the colon deposits there even with a
        //  register edit armed, and the arming lasts only to the next line.
        TEST_METHOD (RegisterEdit_OnlyAnAddresslessColonOnTheNextLine)
        {
            MachineRig  rig;
            Byte        value = 0;



            rig.Run ("^E");
            rig.Run ("400: 77");

            Assert::IsTrue   (rig.target.TryPeek (0x400, value));
            Assert::AreEqual ((int) 0x77, (int) value, L"an address in front deposits");

            rig.Run ("^E");
            rig.Run ("300.300");
            rig.Run (": 66");

            Assert::IsTrue   (rig.target.TryPeek (0x300, value));
            Assert::AreEqual ((int) 0x66, (int) value, L"a line between ends the arming");
            Assert::AreNotEqual ((int) 0x66, (int) rig.target.GetRegisters().a);
        }



        //  `: bytes` stores at the last address typed, as the ROM's A3 does.
        TEST_METHOD (Deposit_WithNoAddress_StoresAtTheLastAddressTyped)
        {
            MonitorHandlersTests::Rig  rig;
            Byte                       value = 0;



            rig.RunOk ("500: 01 02");
            rig.RunOk ("300");
            rig.RunOk (": 12");

            Assert::IsTrue   (rig.target.TryPeek (0x300, value));
            Assert::AreEqual ((int) 0x12, (int) value);
        }
    };
}
