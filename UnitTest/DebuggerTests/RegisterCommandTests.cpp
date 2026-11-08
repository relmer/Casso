#include "Pch.h"

#include "CommandPathRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterCommandTests
//
//  R and REGISTER, shown and set, and the flag commands, on a real //e
//  through the window console, the debug channel and a batch script. Every
//  set is checked on the CPU and then by an instruction that uses the value,
//  so a change that reached only the reply cannot pass.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ShowRegistersTests)
    {
    public:

        static void ExpectUnchanged (CommandPathRig & rig, const Cpu6502Registers & before, const std::string & context)
        {
            Cpu6502Registers  after = rig.GetRegisters();



            Assert::AreEqual (before.a,  after.a,  rig.Where (context + ": A").c_str());
            Assert::AreEqual (before.x,  after.x,  rig.Where (context + ": X").c_str());
            Assert::AreEqual (before.y,  after.y,  rig.Where (context + ": Y").c_str());
            Assert::AreEqual (before.p,  after.p,  rig.Where (context + ": P").c_str());
            Assert::AreEqual (before.sp, after.sp, rig.Where (context + ": S").c_str());
            Assert::AreEqual (before.pc, after.pc, rig.Where (context + ": PC").c_str());
        }



        //  Every name and case, in AppleWin and Casso modes, on every path;
        //  two status bytes between them show every flag set and clear.
        TEST_METHOD (R_AndRegister_ShowEveryRegisterAndFlag)
        {
            static constexpr const char * kForms[] = { "R", "r", "REGISTER", "register", "Register" };
            size_t                        checked  = 0;



            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Casso })
                {
                    for (const char * form : kForms)
                    {
                        rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0xA5, 0xF0);
                        rig.AssertText (rig.RunOk (form, mode), { "A:41 X:01 Y:02 P:A5 S:F0 PC:1234  N.R..I.C" }, form);
                        ExpectUnchanged (rig, rig.GetRegisters(), form);

                        rig.SetRegisters (0xFFFF, 0xFF, 0x00, 0x80, 0x5A, 0x00);
                        rig.AssertText (rig.RunOk (form, mode), { "A:FF X:00 Y:80 P:5A S:00 PC:FFFF  .V.BD.Z." }, form);
                        ++checked;
                    }
                }
            }

            Assert::AreEqual ((size_t) 30, checked);
        }



        //  Anything after R is a register to set, so a lone name has no value
        //  and a word that is not a register is refused by name.
        TEST_METHOD (R_WithANameAndNoValue_OrAWordThatIsNoRegister_Fails)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig    rig (path);
                Cpu6502Registers  before;
                Reply             reply;



                rig.SetRegisters (0x0300, 0x41, 0x01, 0x02, 0x30, 0xF0);
                before = rig.GetRegisters();

                reply = rig.RunFails ("R X", "invalid arguments", "The expression is incomplete.");
                rig.AssertText (reply, { "Error: invalid arguments", "       The expression is incomplete." }, "R X");

                reply = rig.RunFails ("R junk", "invalid arguments", "JUNK is not a register. The registers are A, X, Y, P, S, and PC.");
                rig.AssertText (reply, { "Error: invalid arguments", "       JUNK is not a register. The registers are A, X, Y, P, S, and PC." }, "R junk");

                ExpectUnchanged (rig, before, "a failed R");
            }
        }



        //  The Monitor's own register line has no PC; `/R` reaches it, and a
        //  bare R is the Monitor's read-file command, not the registers.
        TEST_METHOD (Monitor_SlashR_IsTheMonitorsLine_AndBareRIsNotRegisters)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig    rig (path);
                Cpu6502Registers  before;
                Reply             reply;



                rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0xA5, 0xF0);
                before = rig.GetRegisters();

                rig.AssertText (rig.RunOk ("/R",        CommandMode::Monitor), { "A=41 X=01 Y=02 P=A5 S=F0" }, "/R");
                rig.AssertText (rig.RunOk ("/REGISTER", CommandMode::Monitor), { "A=41 X=01 Y=02 P=A5 S=F0" }, "/REGISTER");
                rig.AssertText (rig.RunOk ("/r",        CommandMode::Monitor), { "A=41 X=01 Y=02 P=A5 S=F0" }, "/r");

                reply = rig.Run ("R", CommandMode::Monitor);
                Assert::IsTrue (reply.text.empty() || reply.text[0].find ("A=41") == std::string::npos, rig.Where ("a bare R shows no registers").c_str());
                ExpectUnchanged (rig, before, "the Monitor's R");
            }
        }



        //  WinDbg's `r` line: lowercase names, a flag's letter uppercase when
        //  set, and a dash for bit 5 whatever it holds.
        TEST_METHOD (WinDbg_R_IsWinDbgsLine_AndTheMarkerFormsShowTheSame)
        {
            static constexpr const char * kForms[] = { "r", "R", "!R", "!REGISTER", "!register" };
            size_t                        checked  = 0;



            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                for (const char * form : kForms)
                {
                    rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0xA5, 0xF0);
                    rig.AssertText (rig.RunOk (form, CommandMode::WinDbg), { "a=41 x=01 y=02 sp=f0 pc=1234 Nv-bdIzC" }, form);

                    rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0xFF, 0xFF);
                    rig.AssertText (rig.RunOk (form, CommandMode::WinDbg), { "a=00 x=00 y=00 sp=ff pc=0300 NV-BDIZC" }, form);

                    rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x20, 0xFF);
                    rig.AssertText (rig.RunOk (form, CommandMode::WinDbg), { "a=00 x=00 y=00 sp=ff pc=0300 nv-bdizc" }, form);
                    ++checked;
                }
            }

            Assert::AreEqual ((size_t) 15, checked);
        }



        //  GSSquared's `r` is its step out, so its registers are REGISTER,
        //  which keeps AppleWin's line: GSSquared has none of its own.
        TEST_METHOD (GSSquared_RegisterShows_AndRStepsOut)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0xA5, 0xF0);
                rig.AssertText (rig.RunOk ("REGISTER", CommandMode::GSSquared), { "A:41 X:01 Y:02 P:A5 S:F0 PC:1234  N.R..I.C" }, "REGISTER");
                rig.AssertText (rig.RunOk ("register", CommandMode::GSSquared), { "A:41 X:01 Y:02 P:A5 S:F0 PC:1234  N.R..I.C" }, "register");

                //  An RTS with $0302 on the stack: stepping out lands after it.
                rig.Store (0x0310, { 0x60 });
                rig.Store (0x01FE, { 0x02, 0x03 });
                rig.SetRegisters (0x0310, 0x41, 0x01, 0x02, 0x34, 0xFD);

                rig.RunOk ("r", CommandMode::GSSquared);
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0303, rig.GetRegisters().pc, rig.Where ("r stepped out").c_str());
                Assert::AreEqual ((Byte) 0xFF,   rig.GetRegisters().sp, rig.Where ("through the RTS").c_str());
            }
        }



        //  The channel's data: every register as a number and every flag as a
        //  boolean, each checked set and clear.
        TEST_METHOD (Channel_R_CarriesTheRegistersAndEveryFlag)
        {
            static constexpr const char * kFlags[] = { "n", "v", "r", "b", "d", "i", "z", "c" };
            CommandPathRig                rig (CommandPath::Channel);
            size_t                        checked  = 0;



            for (Byte p : { (Byte) 0xA5, (Byte) 0x5A })
            {
                const JsonValue  * data  = nullptr;
                const JsonValue  * flags = nullptr;
                std::string        kind;
                int                value = 0;



                rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, p, 0xF0);
                rig.RunOk ("R");

                Assert::IsTrue   (rig.GetLastRecord().HasObject ("data", data));
                Assert::IsTrue   (data->HasString ("kind", kind));
                Assert::AreEqual (std::string ("registers"), kind);
                Assert::IsTrue   (data->HasInt ("a", value));   Assert::AreEqual (0x41,   value);
                Assert::IsTrue   (data->HasInt ("x", value));   Assert::AreEqual (0x01,   value);
                Assert::IsTrue   (data->HasInt ("y", value));   Assert::AreEqual (0x02,   value);
                Assert::IsTrue   (data->HasInt ("p", value));   Assert::AreEqual ((int) p, value);
                Assert::IsTrue   (data->HasInt ("s", value));   Assert::AreEqual (0xF0,   value);
                Assert::IsTrue   (data->HasInt ("pc", value));  Assert::AreEqual (0x1234, value);
                Assert::IsTrue   (data->HasObject ("flags", flags));

                for (int bit = 0; bit < 8; ++bit)
                {
                    bool  isSet = false;



                    Assert::IsTrue   (flags->HasBool (kFlags[bit], isSet), std::wstring (1, (wchar_t) kFlags[bit][0]).c_str());
                    Assert::AreEqual ((p & (0x80 >> bit)) != 0, isSet, std::wstring (1, (wchar_t) kFlags[bit][0]).c_str());
                    ++checked;
                }
            }

            Assert::AreEqual ((size_t) 16, checked);
        }



        //  The register pane reads R in AppleWin mode whatever dialect the
        //  session is in, so every row is right in every one of them.
        TEST_METHOD (RegisterPane_ShowsEveryRow_InEveryDialect)
        {
            static constexpr CommandMode  kModes[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso };
            CommandPathRig                rig (CommandPath::WindowConsole);
            size_t                        checked = 0;



            for (CommandMode mode : kModes)
            {
                DebuggerViewSnapshot  snapshot;
                std::wstring          where = rig.Where (CommandModeNames::GetName (mode));



                rig.SetMode (mode);
                Assert::IsTrue (rig.GetSession().GetMode() == mode, where.c_str());

                rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0xA5, 0xF0);
                snapshot = rig.GetWindow().view.Build (rig.GetSession());

                Assert::AreEqual ((size_t) 6, snapshot.registers.size(), where.c_str());
                Assert::AreEqual (std::string ("A=41 X=01 Y=02 P=A5 S=F0 PC=1234"),
                                  std::format ("{}={} {}={} {}={} {}={} {}={} {}={}",
                                               snapshot.registers[0].name, snapshot.registers[0].value,
                                               snapshot.registers[1].name, snapshot.registers[1].value,
                                               snapshot.registers[2].name, snapshot.registers[2].value,
                                               snapshot.registers[3].name, snapshot.registers[3].value,
                                               snapshot.registers[4].name, snapshot.registers[4].value,
                                               snapshot.registers[5].name, snapshot.registers[5].value), where.c_str());
                Assert::AreEqual (std::string ("N.-..I.C"), snapshot.flags, where.c_str());

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x5A, 0xFF);
                snapshot = rig.GetWindow().view.Build (rig.GetSession());
                Assert::AreEqual (std::string (".V.BD.Z."), snapshot.flags, where.c_str());
                Assert::AreEqual (std::string ("5A"), snapshot.registers[3].value, where.c_str());
                ++checked;
            }

            Assert::AreEqual ((size_t) 5, checked);
        }
    };





    TEST_CLASS (SetRegisterTests)
    {
    public:

        //  A program that stores A, X and Y, pushes P and stores S, so a run
        //  shows what each register held when it started.
        static void LoadObserver (CommandPathRig & rig)
        {
            rig.Store (0x0300, { 0x85, 0x10,        // STA $10
                                 0x86, 0x11,        // STX $11
                                 0x84, 0x12,        // STY $12
                                 0x08,              // PHP
                                 0xBA,              // TSX
                                 0x86, 0x13,        // STX $13
                                 0xEA });           // NOP at $030A
        }



        //  Every form of the set, each register checked on the CPU and in the
        //  reply, then a run that starts from them.
        TEST_METHOD (R_SetsEachRegister_AndTheRunStartsFromThem)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                LoadObserver (rig);
                rig.SetRegisters (0x0000, 0x00, 0x00, 0x00, 0x00, 0xFF);

                rig.AssertText (rig.RunOk ("R A 41"),       { "A:41 X:00 Y:00 P:00 S:FF PC:0000  ........" }, "R A 41");
                rig.AssertText (rig.RunOk ("R X=10"),       { "A:41 X:10 Y:00 P:00 S:FF PC:0000  ........" }, "R X=10");
                rig.AssertText (rig.RunOk ("R Y = 22"),     { "A:41 X:10 Y:22 P:00 S:FF PC:0000  ........" }, "R Y = 22");
                rig.AssertText (rig.RunOk ("r p=c7"),       { "A:41 X:10 Y:22 P:C7 S:FF PC:0000  NV...IZC" }, "r p=c7");
                rig.AssertText (rig.RunOk ("REGISTER S F0"), { "A:41 X:10 Y:22 P:C7 S:F0 PC:0000  NV...IZC" }, "REGISTER S F0");
                rig.AssertText (rig.RunOk ("r pc = 0300"),  { "A:41 X:10 Y:22 P:C7 S:F0 PC:0300  NV...IZC" }, "r pc = 0300");

                Assert::AreEqual ((Byte) 0x41,   rig.GetRegisters().a);
                Assert::AreEqual ((Byte) 0x10,   rig.GetRegisters().x);
                Assert::AreEqual ((Byte) 0x22,   rig.GetRegisters().y);
                Assert::AreEqual ((Byte) 0xC7,   rig.GetRegisters().p);
                Assert::AreEqual ((Byte) 0xF0,   rig.GetRegisters().sp);
                Assert::AreEqual ((Word) 0x0300, rig.GetRegisters().pc);

                rig.RunOk ("G 30A");
                rig.FinishRun();

                Assert::AreEqual ((Word) 0x030A, rig.GetRegisters().pc, rig.Where ("the run began at the PC that was set").c_str());
                Assert::AreEqual ((Byte) 0x41,   rig.Read (0x0010), rig.Where ("STA stored the A that was set").c_str());
                Assert::AreEqual ((Byte) 0x10,   rig.Read (0x0011), rig.Where ("STX stored the X").c_str());
                Assert::AreEqual ((Byte) 0x22,   rig.Read (0x0012), rig.Where ("STY stored the Y").c_str());
                Assert::AreEqual ((Byte) 0xF7,   rig.Read (0x01F0), rig.Where ("PHP pushed the P, with B and bit 5, at the S").c_str());
                Assert::AreEqual ((Byte) 0xEF,   rig.Read (0x0013), rig.Where ("and S moved down from the one that was set").c_str());
            }
        }



        //  A value is an expression: $ and # numbers, a character, another
        //  register, a symbol, a search result, a byte read through *, and
        //  the low and high byte of one.
        TEST_METHOD (R_ValueIsAnExpression)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x5C, 0x00, 0x30, 0xFF);
                rig.Store (0x0305, { 0x41 });
                rig.Store (0x0300, { 0x99 });

                rig.RunOk ("R A #10");      Assert::AreEqual ((Byte) 10,     rig.GetRegisters().a);
                rig.RunOk ("R A $7F");      Assert::AreEqual ((Byte) 0x7F,   rig.GetRegisters().a);
                rig.RunOk ("R A 'Z'");      Assert::AreEqual ((Byte) 'Z',    rig.GetRegisters().a);
                rig.RunOk ("R A X");        Assert::AreEqual ((Byte) 0x5C,   rig.GetRegisters().a, rig.Where ("A bare X is the register, not hex").c_str());
                rig.RunOk ("R Y A");        Assert::AreEqual ((Byte) 0x5C,   rig.GetRegisters().y, rig.Where ("nor a bare A").c_str());
                rig.RunOk ("R PC HOME");    Assert::AreEqual ((Word) 0xFC58, rig.GetRegisters().pc);
                rig.RunOk ("R A <HOME");    Assert::AreEqual ((Byte) 0x58,   rig.GetRegisters().a);
                rig.RunOk ("R A >HOME");    Assert::AreEqual ((Byte) 0xFC,   rig.GetRegisters().a);
                rig.RunOk ("R A *300");     Assert::AreEqual ((Byte) 0x99,   rig.GetRegisters().a);
                rig.RunOk ("R A 3+4");      Assert::AreEqual ((Byte) 7,      rig.GetRegisters().a);

                rig.RunOk ("S 300:30F 41");
                rig.RunOk ("R PC @1");      Assert::AreEqual ((Word) 0x0305, rig.GetRegisters().pc);
                rig.RunOk ("R A <@1");      Assert::AreEqual ((Byte) 0x05,   rig.GetRegisters().a);

                rig.RunFails ("R A *C030", "invalid arguments", "The byte at $C030 cannot be read.");
                rig.RunFails ("R A @2",    "invalid arguments", "@2 is not a symbol, register, or hex number.");
                Assert::AreEqual ((Byte) 0x05, rig.GetRegisters().a, rig.Where ("a failed value changes nothing").c_str());
            }
        }



        //  A, X, Y and P are bytes. A value above $FF is refused and leaves
        //  every register as it was.
        TEST_METHOD (R_ByteRegistersRefuseAWord)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x11, 0x22, 0x33, 0x30, 0xF0);

                rig.AssertText (rig.RunFails ("R A 100", "value out of range", "A takes a byte, $00-$FF."),
                                { "Error: value out of range", "       A takes a byte, $00-$FF." }, "R A 100");
                rig.RunFails ("R X 100", "value out of range", "X takes a byte, $00-$FF.");
                rig.RunFails ("R Y 1FF", "value out of range", "Y takes a byte, $00-$FF.");
                rig.RunFails ("R P 1FF", "value out of range", "P takes a byte, $00-$FF.");
                rig.RunFails ("R A FFFF", "value out of range", "A takes a byte, $00-$FF.");

                Assert::AreEqual ((Byte) 0x11,   rig.GetRegisters().a);
                Assert::AreEqual ((Byte) 0x22,   rig.GetRegisters().x);
                Assert::AreEqual ((Byte) 0x33,   rig.GetRegisters().y);
                Assert::AreEqual ((Byte) 0x30,   rig.GetRegisters().p);
                Assert::AreEqual ((Byte) 0xF0,   rig.GetRegisters().sp);
                Assert::AreEqual ((Word) 0x0300, rig.GetRegisters().pc);

                rig.RunOk ("R A FF");  Assert::AreEqual ((Byte) 0xFF, rig.GetRegisters().a);
                rig.RunOk ("R A 0");   Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().a);
            }
        }



        //  S takes a byte, or a stack address whose low byte it keeps; past
        //  the stack page it is refused.
        TEST_METHOD (R_S_TakesAByteOrAStackAddress)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                rig.RunOk ("R S FE");   Assert::AreEqual ((Byte) 0xFE, rig.GetRegisters().sp);
                rig.RunOk ("R S 1FD");  Assert::AreEqual ((Byte) 0xFD, rig.GetRegisters().sp);
                rig.RunOk ("R S 100");  Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().sp);
                rig.RunOk ("R S 1FF");  Assert::AreEqual ((Byte) 0xFF, rig.GetRegisters().sp);
                rig.RunOk ("R S 0");    Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().sp);

                rig.RunFails ("R S 200", "value out of range", "S takes a byte, $00-$FF.");
                rig.RunFails ("R S FF00", "value out of range", "S takes a byte, $00-$FF.");
                Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().sp, rig.Where ("a refused S changes nothing").c_str());
            }
        }



        //  PC takes the whole address space and nothing beyond it.
        TEST_METHOD (R_PC_TakesAWord)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                rig.RunOk ("R PC FFFF");  Assert::AreEqual ((Word) 0xFFFF, rig.GetRegisters().pc);
                rig.RunOk ("R PC 0");     Assert::AreEqual ((Word) 0x0000, rig.GetRegisters().pc);

                reply = rig.Run ("R PC 10000");
                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, rig.Where ("R PC 10000").c_str());
                Assert::AreEqual (std::string ("invalid arguments"), reply.error.label, rig.Where ("R PC 10000").c_str());
                Assert::IsTrue   (reply.error.detail.ends_with ("10000 is outside $0000-$FFFF."), rig.Where (reply.error.detail).c_str());
                Assert::AreEqual ((Word) 0x0000, rig.GetRegisters().pc);
            }
        }



        //  A register that does not exist is refused by name, and SP, which is
        //  WinDbg's name for S, is not an AppleWin register.
        TEST_METHOD (R_UnknownRegister_IsRefused)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                rig.RunFails ("R Q 1",  "invalid arguments", "Q is not a register. The registers are A, X, Y, P, S, and PC.");
                rig.RunFails ("R SP 1", "invalid arguments", "SP is not a register. The registers are A, X, Y, P, S, and PC.");
                rig.RunFails ("R sp=1", "invalid arguments", "SP is not a register. The registers are A, X, Y, P, S, and PC.");
                Assert::AreEqual ((Byte) 0xF0,   rig.GetRegisters().sp);
                Assert::AreEqual ((Word) 0x0300, rig.GetRegisters().pc, rig.Where ("an unknown name never lands on PC").c_str());
            }
        }



        //  A missing or malformed value is the evaluator's error.
        TEST_METHOD (R_MissingOrMalformedValue_IsTheEvaluatorsError)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                rig.SetRegisters (0x0300, 0x11, 0x00, 0x00, 0x30, 0xF0);

                rig.RunFails ("R A",       "invalid arguments", "The expression is incomplete.");
                rig.RunFails ("R A=",      "invalid arguments", "The expression is incomplete.");
                reply = rig.Run ("R A 41 42");
                Assert::AreEqual (std::string ("invalid arguments"), reply.error.label, rig.Where ("R A 41 42").c_str());
                Assert::IsTrue   (reply.error.detail.starts_with ("An operator was expected at column "), rig.Where (reply.error.detail).c_str());
                rig.RunFails ("R A 4G",    "invalid arguments", "4G is not a hex number.");
                rig.RunFails ("R A NOSUCH", "invalid arguments", "NOSUCH is not a symbol, register, or hex number.");
                Assert::AreEqual ((Byte) 0x11, rig.GetRegisters().a);
            }
        }



        //  The handler checks the name itself, rather than taking anything it
        //  does not know as PC. No parser produces such a command, so it is
        //  given to the session directly, as batch mode gives BUDGET and MODE.
        TEST_METHOD (SetRegister_WithANameNoParserProduces_IsRefused)
        {
            CommandPathRig  rig (CommandPath::Batch);
            DebugCommand    command;
            Reply           reply;



            rig.SetRegisters (0x0300, 0x11, 0x00, 0x00, 0x30, 0xF0);

            command.verb       = DebugVerb::SetRegister;
            command.sourceName = "R";
            command.text       = "SP";
            command.a1         = 0x0010;
            command.hasA1      = true;

            reply = rig.GetSession().Execute (command);
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("invalid arguments"), reply.error.label);
            Assert::AreEqual (std::string ("SP is not a register. The registers are A, X, Y, P, S, and PC."), reply.error.detail);
            Assert::AreEqual ((Word) 0x0300, rig.GetRegisters().pc, L"PC is untouched");
        }



        //  WinDbg's r: its names, including sp and fl, 0x and 0n numbers, and
        //  the reply in its own layout.
        TEST_METHOD (WinDbg_R_SetsWithWinDbgsNames)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                LoadObserver (rig);
                rig.SetRegisters (0x0000, 0x00, 0x00, 0x00, 0x00, 0xFF);

                rig.AssertText (rig.RunOk ("r a=41",      CommandMode::WinDbg), { "a=41 x=00 y=00 sp=ff pc=0000 nv-bdizc" }, "r a=41");
                rig.AssertText (rig.RunOk ("r x=0n16",    CommandMode::WinDbg), { "a=41 x=10 y=00 sp=ff pc=0000 nv-bdizc" }, "r x=0n16");
                rig.AssertText (rig.RunOk ("r y = 22",    CommandMode::WinDbg), { "a=41 x=10 y=22 sp=ff pc=0000 nv-bdizc" }, "r y = 22");
                rig.AssertText (rig.RunOk ("r fl=c7",     CommandMode::WinDbg), { "a=41 x=10 y=22 sp=ff pc=0000 NV-bdIZC" }, "r fl=c7");
                rig.AssertText (rig.RunOk ("r sp = f0",   CommandMode::WinDbg), { "a=41 x=10 y=22 sp=f0 pc=0000 NV-bdIZC" }, "r sp = f0");
                rig.AssertText (rig.RunOk ("r pc=0x300",  CommandMode::WinDbg), { "a=41 x=10 y=22 sp=f0 pc=0300 NV-bdIZC" }, "r pc=0x300");

                rig.RunOk ("r p=c7", CommandMode::WinDbg);
                Assert::AreEqual ((Byte) 0xC7, rig.GetRegisters().p, rig.Where ("p is fl").c_str());

                rig.RunFails ("r q=1",  "invalid arguments", "q is not a register. The registers are a, x, y, sp, pc and fl.", CommandMode::WinDbg);
                rig.RunFails ("r s=f0", "invalid arguments", "s is not a register. The registers are a, x, y, sp, pc and fl.", CommandMode::WinDbg);
                reply = rig.Run ("r a=1 x=2", CommandMode::WinDbg);
                Assert::AreEqual (std::string ("invalid arguments"), reply.error.label, rig.Where ("r a=1 x=2").c_str());
                Assert::IsTrue   (reply.error.detail.starts_with ("An operator was expected at column "), rig.Where (reply.error.detail).c_str());
                Assert::AreEqual ((Byte) 0xF0, rig.GetRegisters().sp);

                rig.RunOk ("g 30a", CommandMode::WinDbg);
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x41, rig.Read (0x0010), rig.Where ("the run used the A r set").c_str());
                Assert::AreEqual ((Byte) 0xF7, rig.Read (0x01F0), rig.Where ("and the P and S").c_str());
            }
        }



        //  Each dialect's form of the set: Monitor's `/R`, GSSquared's
        //  REGISTER (its `r` steps and takes nothing), and WinDbg's `!R`.
        TEST_METHOD (Dialects_SetARegister)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                rig.AssertText (rig.RunOk ("/R A 41", CommandMode::Monitor), { "A=41 X=00 Y=00 P=30 S=F0" }, "/R A 41");
                Assert::AreEqual ((Byte) 0x41, rig.GetRegisters().a);

                rig.AssertText (rig.RunOk ("/R PC 400", CommandMode::Monitor), { "A=41 X=00 Y=00 P=30 S=F0" }, "/R PC 400");
                Assert::AreEqual ((Word) 0x0400, rig.GetRegisters().pc, rig.Where ("the Monitor's line has no PC, but PC was set").c_str());

                rig.Run ("R A 55", CommandMode::Monitor);
                Assert::AreEqual ((Byte) 0x41, rig.GetRegisters().a, rig.Where ("a bare R in the Monitor reads a file").c_str());

                rig.AssertText (rig.RunOk ("REGISTER X 42", CommandMode::GSSquared), { "A:41 X:42 Y:00 P:30 S:F0 PC:0400  ..RB...." }, "REGISTER X 42");
                Assert::AreEqual ((Byte) 0x42, rig.GetRegisters().x);

                rig.RunFails ("r A 43", "invalid arguments", "r takes nothing after it.", CommandMode::GSSquared);
                Assert::AreEqual ((Byte) 0x41, rig.GetRegisters().a);

                rig.AssertText (rig.RunOk ("!R Y 44", CommandMode::WinDbg), { "a=41 x=42 y=44 sp=f0 pc=0400 nv-Bdizc" }, "!R Y 44");
                Assert::AreEqual ((Byte) 0x44, rig.GetRegisters().y);

                rig.RunOk ("R A 45", CommandMode::Casso);
                Assert::AreEqual ((Byte) 0x45, rig.GetRegisters().a, rig.Where ("Casso mode sets as AppleWin mode does").c_str());
            }
        }



        //  Bit 5 and B are stored as given, so the register can hold what PHP
        //  never pushes: the pane shows it, WinDbg's dash hides it, and PHP
        //  still pushes both set.
        TEST_METHOD (R_P_StoresBit5AndBExactly)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0300, { 0x08, 0xEA });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x30, 0xF0);

                rig.AssertText (rig.RunOk ("R P 00"), { "A:00 X:00 Y:00 P:00 S:F0 PC:0300  ........" }, "R P 00");
                rig.AssertText (rig.RunOk ("r", CommandMode::WinDbg), { "a=00 x=00 y=00 sp=f0 pc=0300 nv-bdizc" }, "r");

                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x30, rig.Read (0x01F0), rig.Where ("PHP pushed B and bit 5 set").c_str());
            }
        }
    };





    TEST_CLASS (FlagCommandTests)
    {
    public:

        static constexpr const char * kLetters = "CZIDBRVN";



        //  Each set command sets its bit and no other; each clear clears its
        //  bit and no other. Every name form: SEx and CLx, Sx and Rx, and SE
        //  and CL with the letter after them in either case.
        TEST_METHOD (EveryFlagCommand_ChangesOnlyItsBit)
        {
            size_t  checked = 0;



            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                for (int i = 0; i < 8; ++i)
                {
                    std::string  letter (1, kLetters[i]);
                    std::string  lower  (1, (char) tolower (kLetters[i]));
                    Byte         bit    = (Byte) (1 << i);



                    for (const std::string & set : { "SE" + letter, "S" + letter, "SE " + letter, "se " + lower, "SE" + letter + " junk" })
                    {
                        rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x00, 0xF0);
                        rig.RunOk (set);
                        Assert::AreEqual (bit, rig.GetRegisters().p, rig.Where (set).c_str());
                    }

                    for (const std::string & clear : { "CL" + letter, "R" + letter, "CL " + letter, "cl " + lower })
                    {
                        rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0xFF, 0xF0);
                        rig.RunOk (clear);
                        Assert::AreEqual ((Byte) ~bit, rig.GetRegisters().p, rig.Where (clear).c_str());
                    }

                    ++checked;
                }

                rig.SetRegisters (0x1234, 0x41, 0x01, 0x02, 0x00, 0xF0);
                rig.AssertText (rig.RunOk ("SEC"), { "A:41 X:01 Y:02 P:01 S:F0 PC:1234  .......C" }, "SEC");
                rig.AssertText (rig.RunOk ("SEN"), { "A:41 X:01 Y:02 P:81 S:F0 PC:1234  N......C" }, "SEN");
                rig.AssertText (rig.RunOk ("CLC"), { "A:41 X:01 Y:02 P:80 S:F0 PC:1234  N......." }, "CLC");
            }

            Assert::AreEqual ((size_t) 24, checked);
        }



        //  A letter that is not a flag, none, or two, is refused and changes
        //  nothing.
        TEST_METHOD (BadFlagLetter_IsRefused)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x5A, 0xF0);

                for (const char * line : { "SE Q", "SE", "CL", "CL CZ", "SE 1" })
                {
                    Reply  reply = rig.RunFails (line, "invalid arguments", "A flag is one of C, Z, I, D, B, R, V, or N.");



                    rig.AssertText (reply, { "Error: invalid arguments", "       A flag is one of C, Z, I, D, B, R, V, or N." }, line);
                }

                Assert::AreEqual ((Byte) 0x5A, rig.GetRegisters().p);
            }
        }



        //  The handler repeats the parser's check. No parser can hand it a bad
        //  letter, so the command is given to the session directly.
        TEST_METHOD (FlagCommand_TheHandlersOwnCheck)
        {
            CommandPathRig  rig (CommandPath::Batch);



            rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x5A, 0xF0);

            for (DebugVerb verb : { DebugVerb::SetFlag, DebugVerb::ClearFlag })
            {
                for (const char * text : { "Q", "", "CZ" })
                {
                    DebugCommand  command;
                    Reply         reply;



                    command.verb       = verb;
                    command.sourceName = "SE";
                    command.text       = text;

                    reply = rig.GetSession().Execute (command);
                    Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
                    Assert::AreEqual (std::string ("A flag is one of C, Z, I, D, B, R, V, or N."), reply.error.detail);
                }
            }

            Assert::AreEqual ((Byte) 0x5A, rig.GetRegisters().p);
        }



        //  Monitor's `/`, GSSquared's bare names (none of them is a GSSquared
        //  word or a hex word), and WinDbg's `!` and `r fl=`.
        TEST_METHOD (Dialects_SetAndClearFlags)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x00, 0xF0);

                rig.AssertText (rig.RunOk ("/SEC", CommandMode::Monitor), { "A=00 X=00 Y=00 P=01 S=F0" }, "/SEC");
                rig.RunOk ("/CLC", CommandMode::Monitor);
                Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().p);

                rig.AssertText (rig.RunOk ("SEC", CommandMode::GSSquared), { "A:00 X:00 Y:00 P:01 S:F0 PC:0300  .......C" }, "SEC");
                rig.RunOk ("SE D", CommandMode::GSSquared);
                Assert::AreEqual ((Byte) 0x09, rig.GetRegisters().p);
                rig.RunOk ("CL C", CommandMode::GSSquared);
                rig.RunOk ("RD",   CommandMode::GSSquared);
                Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().p);

                rig.AssertText (rig.RunOk ("!SEV", CommandMode::WinDbg), { "a=00 x=00 y=00 sp=f0 pc=0300 nV-bdizc" }, "!SEV");
                rig.RunOk ("!CLV", CommandMode::WinDbg);
                Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().p);
                rig.RunOk ("r fl=31", CommandMode::WinDbg);
                Assert::AreEqual ((Byte) 0x31, rig.GetRegisters().p);
            }
        }



        //  A flag changed by command is the flag the next instruction reads:
        //  BCS after SEC and CLC, BVS after SEV and CLV, ADC after SED and CLD.
        TEST_METHOD (AChangedFlag_GovernsTheNextRun)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0300, { 0xB0, 0x02 });                 // BCS $0304
                rig.Store (0x0310, { 0x70, 0x02 });                 // BVS $0314
                rig.Store (0x0320, { 0x18, 0x69, 0x01 });           // CLC, ADC #$01

                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x04, 0xF0);
                rig.RunOk ("SEC");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0304, rig.GetRegisters().pc, rig.Where ("SEC: the branch is taken").c_str());

                rig.RunOk ("R PC 300");
                rig.RunOk ("CLC");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0302, rig.GetRegisters().pc, rig.Where ("CLC: it is not").c_str());

                rig.RunOk ("R PC 310");
                rig.RunOk ("SEV");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0314, rig.GetRegisters().pc, rig.Where ("SEV").c_str());

                rig.RunOk ("R PC 310");
                rig.RunOk ("CLV");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0312, rig.GetRegisters().pc, rig.Where ("CLV").c_str());

                rig.RunOk ("R PC 320");
                rig.RunOk ("R A 09");
                rig.RunOk ("SED");
                rig.RunOk ("T 2");
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x10, rig.GetRegisters().a, rig.Where ("SED: $09 + 1 is decimal 10").c_str());

                rig.RunOk ("R PC 320");
                rig.RunOk ("R A 09");
                rig.RunOk ("CLD");
                rig.RunOk ("T 2");
                rig.FinishRun();
                Assert::AreEqual ((Byte) 0x0A, rig.GetRegisters().a, rig.Where ("CLD: binary").c_str());
            }
        }



        //  An interrupt waits while I is set and is taken once CLI clears it.
        TEST_METHOD (CLI_LetsAPendingInterruptIn)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                IrqSourceId     source  = 0;
                HRESULT         hr      = S_OK;
                Byte            low     = 0;
                Byte            high    = 0;
                Word            handler = 0;



                rig.Store (0x0300, { 0xEA, 0xEA, 0xEA });
                rig.SetRegisters (0x0300, 0x00, 0x00, 0x00, 0x04, 0xF0);

                rig.GetSession().GetTarget().TryPeek (0xFFFE, low);
                rig.GetSession().GetTarget().TryPeek (0xFFFF, high);
                handler = (Word) (low | (high << 8));

                hr = rig.GetMachine().GetInterruptController().RegisterSource (source);
                Assert::IsTrue (SUCCEEDED (hr));
                rig.GetMachine().GetInterruptController().Assert (source);

                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) 0x0301, rig.GetRegisters().pc, rig.Where ("I set: the interrupt waits").c_str());

                rig.RunOk ("CLI");
                rig.RunOk ("T");
                rig.FinishRun();
                Assert::AreEqual ((Word) handler, rig.GetRegisters().pc, rig.Where ("I clear: it is taken, to the IRQ vector").c_str());
                Assert::IsTrue   ((rig.GetRegisters().p & 0x04) != 0, rig.Where ("and sets I on the way in").c_str());
                Assert::AreEqual ((Word) 0x0301, (Word) (rig.Read (0x01EF) | (rig.Read (0x01F0) << 8)),
                                  rig.Where ("returning to the instruction it came before").c_str());
            }
        }
    };
}
