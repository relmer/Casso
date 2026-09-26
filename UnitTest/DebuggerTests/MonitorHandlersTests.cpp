#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"
#include "EmuTests/TestMachine.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"
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



        //  Without a name, batch and the channel have nobody to ask.
        TEST_METHOD (WriteWithNoFileName_IsAnError)
        {
            Rig    rig;
            Reply  reply = rig.Run ("300.301W");



            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("ERR"), reply.text.at (0));
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



        TEST_METHOD (AnUnknownSlot_IsAnError)
        {
            Rig  rig;



            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("9^K").status);
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
            Cpu6502Registers  registers;



            //  The slash is what reaches AppleWin mode from here; a bare
            //  `MODE APPLEWIN` would read as Monitor commands.
            rig.Run ("/MODE APPLEWIN");
            rig.Run ("MEB 300 EA 60");          // NOP / RTS
            rig.Run ("R A 41");
            rig.Run ("MEB 45 99");              // what the ROM would have loaded
            rig.Run ("MODE MONITOR");

            rig.Run ("300G");

            registers = rig.target.GetRegisters();
            Assert::AreEqual ((int) 0x41, (int) registers.a, L"the register the reader set, not the zero-page byte");
        }
    };
}
