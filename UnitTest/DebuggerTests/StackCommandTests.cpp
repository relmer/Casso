#include "Pch.h"

#include "CommandPathRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StackCommandTests
//
//  POP, PPOP, PUSH and STACK on a real //e through the window console, the
//  debug channel and a batch script: what each leaves in the registers and
//  the stack page, what the next instruction then sees, and the aux stack
//  under ALTZP.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (StackCommandTests)
    {
    public:

        static constexpr Word  kAltZpOff = 0xC008;
        static constexpr Word  kAltZpOn  = 0xC009;



        //  STACK's lines for the stack page as the CPU reads it now: the
        //  pointer, then each byte from $01FF down to the one above it.
        static std::vector<std::string> ExpectedStack (CommandPathRig & rig, Byte sp)
        {
            std::vector<std::string>  lines = { std::format ("S:{:02X}", sp) };



            for (int offset = 0xFF; offset > sp; --offset)
            {
                lines.push_back (std::format ("{:04X}: {:02X}", 0x0100 + offset, rig.Read ((Word) (0x0100 + offset))));
            }

            return lines;
        }



        TEST_METHOD (POP_PullsIntoA_AndReplyIsTheRegisters)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x01F1, { 0x5A });
                rig.SetRegisters (0x0300, 0x00, 0x11, 0x22, 0x30, 0xF0);

                rig.AssertText (rig.RunOk ("POP"), { "A:5A X:11 Y:22 P:30 S:F1 PC:0300  ..RB...." }, "POP");
                Assert::AreEqual ((Byte) 0x5A, rig.GetRegisters().a);
                Assert::AreEqual ((Byte) 0xF1, rig.GetRegisters().sp);
                Assert::AreEqual ((Byte) 0x5A, rig.Read (0x01F1), rig.Where ("the byte stays where it was, as after PLA").c_str());
            }
        }



        //  At $FF the pointer wraps to $00 and the byte comes from $0100, with
        //  no error; anything after POP is ignored.
        TEST_METHOD (POP_AtTheTop_WrapsToTheBottom)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0100, { 0x77 });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFF);

                rig.RunOk ("POP junk");
                Assert::AreEqual ((Byte) 0x77, rig.GetRegisters().a);
                Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().sp);
            }
        }



        //  PPOP pulls a word into PC exactly as it lies, low byte first,
        //  without the increment RTS adds.
        TEST_METHOD (PPOP_PullsAWordIntoPC)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x01F1, { 0x34, 0x12 });
                rig.SetRegisters (0x0300, 0x41, 0x00, 0x00, 0x30, 0xF0);

                rig.AssertText (rig.RunOk ("PPOP"), { "A:41 X:00 Y:00 P:30 S:F2 PC:1234  ..RB...." }, "PPOP");
                Assert::AreEqual ((Word) 0x1234, rig.GetRegisters().pc);
                Assert::AreEqual ((Byte) 0xF2,   rig.GetRegisters().sp);
                Assert::AreEqual ((Byte) 0x41,   rig.GetRegisters().a, rig.Where ("A is left alone").c_str());
            }
        }



        //  Near the top the word's bytes wrap to the bottom of the page.
        TEST_METHOD (PPOP_AtTheTop_Wraps)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x01FF, { 0x78 });
                rig.Store (0x0100, { 0x56, 0x9A });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFE);

                rig.RunOk ("PPOP");
                Assert::AreEqual ((Word) 0x5678, rig.GetRegisters().pc, rig.Where ("$01FF and $0100").c_str());
                Assert::AreEqual ((Byte) 0x00,   rig.GetRegisters().sp);

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFF);
                rig.RunOk ("PPOP");
                Assert::AreEqual ((Word) 0x9A56, rig.GetRegisters().pc, rig.Where ("$0100 and $0101").c_str());
                Assert::AreEqual ((Byte) 0x01,   rig.GetRegisters().sp);
            }
        }



        //  Each byte goes where PHA would put it and the pointer moves down;
        //  the reply is the stack as STACK shows it.
        TEST_METHOD (PUSH_StoresEachByte_AndReplyIsTheStack)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                reply = rig.RunOk ("PUSH 12 34");
                Assert::AreEqual ((Byte) 0x12, rig.Read (0x01F0));
                Assert::AreEqual ((Byte) 0x34, rig.Read (0x01EF));
                Assert::AreEqual ((Byte) 0xEE, rig.GetRegisters().sp);
                Assert::AreEqual (CommandPathRig::Join (ExpectedStack (rig, 0xEE)), CommandPathRig::Join (reply.text), rig.Where ("PUSH").c_str());
                Assert::AreEqual (std::string ("01EF: 34"), reply.text.back(), rig.Where ("the last byte pushed is last").c_str());
                Assert::AreEqual ((size_t) 18, reply.text.size());
            }
        }



        //  At $00 the byte goes to $0100 and the pointer wraps to $FF, which
        //  leaves an empty stack to show.
        TEST_METHOD (PUSH_AtTheBottom_Wraps)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0x00);

                rig.AssertText (rig.RunOk ("PUSH 5A"), { "S:FF" }, "PUSH 5A");
                Assert::AreEqual ((Byte) 0x5A, rig.Read (0x0100));
                Assert::AreEqual ((Byte) 0xFF, rig.GetRegisters().sp);
            }
        }



        //  A value is an expression, and a bare register name is that
        //  register: PUSH P pushes P as it is, with none of the bits PHP
        //  forces.
        TEST_METHOD (PUSH_ValuesAreExpressions)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0xA1, 0xB2, 0x00, 0xC3, 0xF0);

                rig.RunOk ("PUSH P A X #10 'Q'");
                Assert::AreEqual ((Byte) 0xC3, rig.Read (0x01F0), rig.Where ("P as stored").c_str());
                Assert::AreEqual ((Byte) 0xA1, rig.Read (0x01EF));
                Assert::AreEqual ((Byte) 0xB2, rig.Read (0x01EE));
                Assert::AreEqual ((Byte) 10,   rig.Read (0x01ED));
                Assert::AreEqual ((Byte) 'Q',  rig.Read (0x01EC));
                Assert::AreEqual ((Byte) 0xEB, rig.GetRegisters().sp);
            }
        }



        TEST_METHOD (PUSH_NothingOrAWord_IsRefused)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Byte            before = 0;



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);
                before = rig.Read (0x01F0);

                rig.AssertText (rig.RunFails ("PUSH", "invalid arguments", "PUSH takes one or more bytes."),
                                { "Error: invalid arguments", "       PUSH takes one or more bytes." }, "PUSH");
                rig.RunFails ("PUSH 100",   "invalid arguments", "100 is not a byte.");
                rig.RunFails ("PUSH 12 1FF", "invalid arguments", "1FF is not a byte.");

                Assert::AreEqual ((Byte) 0xF0, rig.GetRegisters().sp, rig.Where ("nothing was pushed").c_str());
                Assert::AreEqual (before,      rig.Read (0x01F0),      rig.Where ("not even the first byte").c_str());
            }
        }



        //  The stack from $01FF down to the byte above the pointer, top first.
        TEST_METHOD (STACK_ShowsThePageAboveThePointer)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x01FD, { 0x33, 0x22, 0x11 });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFC);

                rig.AssertText (rig.RunOk ("STACK"),      { "S:FC", "01FF: 11", "01FE: 22", "01FD: 33" }, "STACK");
                rig.AssertText (rig.RunOk ("stack junk"), { "S:FC", "01FF: 11", "01FE: 22", "01FD: 33" }, "stack junk");

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFF);
                rig.AssertText (rig.RunOk ("STACK"), { "S:FF" }, "an empty stack");

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0x00);
                Assert::AreEqual ((size_t) 256, rig.RunOk ("STACK").text.size(), rig.Where ("a full stack").c_str());
            }
        }



        //  Every dialect reaches the stack commands through its marker, and
        //  none has a layout of its own for the stack.
        TEST_METHOD (Dialects_ReachEveryStackCommand)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x01FE, { 0x22, 0x11 });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFD);

                rig.AssertText (rig.RunOk ("/STACK", CommandMode::Monitor),   { "S:FD", "01FF: 11", "01FE: 22" }, "/STACK");
                rig.AssertText (rig.RunOk ("STACK",  CommandMode::GSSquared), { "S:FD", "01FF: 11", "01FE: 22" }, "STACK");
                rig.AssertText (rig.RunOk ("!STACK", CommandMode::WinDbg),    { "S:FD", "01FF: 11", "01FE: 22" }, "!STACK");

                rig.AssertText (rig.RunOk ("/PUSH 33", CommandMode::Monitor), { "S:FC", "01FF: 11", "01FE: 22", "01FD: 33" }, "/PUSH");
                rig.RunOk ("PUSH 44", CommandMode::GSSquared);
                rig.RunOk ("!PUSH 55", CommandMode::WinDbg);
                Assert::AreEqual ((Byte) 0xFA, rig.GetRegisters().sp);
                Assert::AreEqual ((Byte) 0x55, rig.Read (0x01FB));

                rig.AssertText (rig.RunOk ("/POP", CommandMode::Monitor), { "A=55 X=00 Y=00 P=30 S=FB" }, "/POP");
                rig.RunOk ("POP", CommandMode::GSSquared);
                Assert::AreEqual ((Byte) 0x44, rig.GetRegisters().a);
                rig.AssertText (rig.RunOk ("!POP", CommandMode::WinDbg), { "a=33 x=00 y=00 sp=fd pc=0300 nv-Bdizc" }, "!POP");

                rig.AssertText (rig.RunOk ("/PPOP", CommandMode::Monitor), { "A=33 X=00 Y=00 P=30 S=FF" }, "/PPOP");
                Assert::AreEqual ((Word) 0x1122, rig.GetRegisters().pc, rig.Where ("the Monitor's line has no PC, but PPOP set it").c_str());
                rig.RunOk ("R S FD");
                rig.RunOk ("PPOP", CommandMode::GSSquared);
                Assert::AreEqual ((Word) 0x1122, rig.GetRegisters().pc);
                rig.RunOk ("R S FD");
                rig.RunOk ("!PPOP", CommandMode::WinDbg);
                Assert::AreEqual ((Word) 0x1122, rig.GetRegisters().pc);
            }
        }



        //  Under ALTZP the CPU's stack is the aux page, and so is the one the
        //  stack commands use; main memory's page 1 is left alone.
        TEST_METHOD (StackCommands_UseTheAuxStackUnderAltZp)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.GetMachine().GetMemoryBus().WriteByte (kAltZpOn, 0);
                rig.Store (0x01F1, { 0x22 });
                rig.Store (0x01FF, { 0x2F });
                rig.GetMachine().GetMemoryBus().WriteByte (kAltZpOff, 0);
                rig.Store (0x01F1, { 0x11 });
                rig.Store (0x01FF, { 0x1F });
                rig.GetMachine().GetMemoryBus().WriteByte (kAltZpOn, 0);

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);
                rig.RunOk ("POP");
                Assert::AreEqual ((Byte) 0x22, rig.GetRegisters().a, rig.Where ("POP read the aux stack").c_str());

                rig.RunOk ("PUSH 99");
                Assert::AreEqual ((Byte) 0x99, rig.Read (0x01F1), rig.Where ("PUSH wrote the aux stack").c_str());
                Assert::AreEqual (std::string ("01FF: 2F"), rig.RunOk ("STACK").text.at (1), rig.Where ("STACK shows it").c_str());

                rig.GetMachine().GetMemoryBus().WriteByte (kAltZpOff, 0);
                Assert::AreEqual ((Byte) 0x11, rig.Read (0x01F1), rig.Where ("and main's page 1 is untouched").c_str());
                Assert::AreEqual (std::string ("01FF: 1F"), rig.RunOk ("STACK").text.at (1), rig.Where ("STACK follows ALTZP").c_str());
            }
        }



        //  PUSH and POP change memory without a bus access, so a watchpoint on
        //  the stack page sees nothing: the next step runs its instruction
        //  instead of stopping on an access the program never made.
        TEST_METHOD (StackCommands_DoNotTripAWatchpoint)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0300, { 0xEA, 0xEA });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x34, 0xF0);

                rig.RunOk ("BPMW 1F0");
                rig.RunOk ("BPMR 1F0");
                rig.RunOk ("PUSH 12");
                rig.RunOk ("POP");
                rig.RunOk ("PPOP");
                rig.RunOk ("R PC 300");

                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0301, rig.GetRegisters().pc, rig.Where ("the step ran its NOP").c_str());
            }
        }



        //  What PUSH leaves is what PLA and RTS then pull.
        TEST_METHOD (PushedBytes_AreWhatTheNextRunPulls)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0300, { 0x68, 0x68, 0x60 });           // PLA, PLA, RTS
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x34, 0xF0);

                rig.RunOk ("PUSH 03 FF 12 34");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x34, rig.GetRegisters().a, rig.Where ("the last byte pushed is the first pulled").c_str());

                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x12, rig.GetRegisters().a);

                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0400, rig.GetRegisters().pc, rig.Where ("RTS returned through $03FF").c_str());
                Assert::AreEqual ((Byte) 0xF0,   rig.GetRegisters().sp);
            }
        }



        TEST_METHOD (Channel_STACK_CarriesThePointerAndEntries)
        {
            CommandPathRig     rig (CommandPath::Channel);
            const JsonValue  * data    = nullptr;
            const JsonValue  * entries = nullptr;
            std::string        kind;
            int                value   = 0;



            rig.Store (0x01FE, { 0x22, 0x11 });
            rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFD);
            rig.RunOk ("STACK");

            Assert::IsTrue   (rig.GetLastRecord().HasObject ("data", data));
            Assert::IsTrue   (data->HasString ("kind", kind));
            Assert::AreEqual (std::string ("stack"), kind);
            Assert::IsTrue   (data->HasInt ("sp", value));
            Assert::AreEqual (0xFD, value);
            Assert::IsTrue   (data->HasArray ("entries", entries));
            Assert::AreEqual ((size_t) 2, entries->GetArraySize());
            Assert::IsTrue   (entries->GetArrayElement (0).HasInt ("address", value));  Assert::AreEqual (0x01FF, value);
            Assert::IsTrue   (entries->GetArrayElement (0).HasInt ("value", value));    Assert::AreEqual (0x11,   value);
            Assert::IsTrue   (entries->GetArrayElement (1).HasInt ("address", value));  Assert::AreEqual (0x01FE, value);
            Assert::IsTrue   (entries->GetArrayElement (1).HasInt ("value", value));    Assert::AreEqual (0x22,   value);
        }



        //  The window's stack pane is STACK's entries, whatever the dialect.
        TEST_METHOD (StackPane_IsSTACKsEntries_InEveryDialect)
        {
            CommandPathRig  rig (CommandPath::WindowConsole);
            size_t          checked = 0;



            rig.Store (0x01FD, { 0x33, 0x22, 0x11 });
            rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xFC);

            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso })
            {
                DebuggerViewSnapshot  snapshot;
                std::wstring          where = rig.Where (CommandModeNames::GetName (mode));



                rig.SetMode (mode);
                snapshot = rig.GetWindow().view.Build (rig.GetSession());

                Assert::AreEqual ((size_t) 3,    snapshot.stack.size(), where.c_str());
                Assert::AreEqual ((Word) 0x01FF, snapshot.stack[0].address, where.c_str());
                Assert::AreEqual ((Byte) 0x11,   snapshot.stack[0].value,   where.c_str());
                Assert::AreEqual ((Word) 0x01FD, snapshot.stack[2].address, where.c_str());
                Assert::AreEqual ((Byte) 0x33,   snapshot.stack[2].value,   where.c_str());
                ++checked;
            }

            Assert::AreEqual ((size_t) 5, checked);
        }
    };
}
