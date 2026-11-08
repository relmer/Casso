#include "Pch.h"

#include "AppleWinRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRegisterCommandTests
//
//  R and REGISTER, the flag commands in all three of their forms, the stack
//  commands, and =, run as a batch script on a real //e, with the registers
//  and memory read back from the machine afterward.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinRegisterCommandTests)
    {
    public:

        //  The register line R prints, built here from the values rather than
        //  by the formatter under test.
        static std::string Registers (Byte a, Byte x, Byte y, Byte p, Byte s, Word pc)
        {
            static constexpr const char  kLetters[] = "NVRBDIZC";
            std::string                  flags;



            for (int bit = 0; bit < 8; ++bit)
            {
                flags += (p & (0x80 >> bit)) ? kLetters[bit] : '.';
            }

            return std::format ("A:{:02X} X:{:02X} Y:{:02X} P:{:02X} S:{:02X} PC:{:04X}  {}", a, x, y, p, s, pc, flags);
        }



        //  The batch machine starts with A, X and Y clear, P $24 and S $FD.
        TEST_METHOD (R_AndREGISTER_ShowEveryRegisterAndEachFlag)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "R PC 300", "R", "REGISTER", "register", "R P FF", "R P 0", "r p 81" }));

            rig.ExpectBlock (0, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (1, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (2, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (3, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (4, { "A:00 X:00 Y:00 P:FF S:FD PC:0300  NVRBDIZC" });
            rig.ExpectBlock (5, { "A:00 X:00 Y:00 P:00 S:FD PC:0300  ........" });
            rig.ExpectBlock (6, { "A:00 X:00 Y:00 P:81 S:FD PC:0300  N......C" });
        }



        TEST_METHOD (R_AsJson_IsTheRegistersAndEachFlagByName)
        {
            AppleWinBatchRig         rig;
            std::vector<JsonValue>   replies;
            const JsonValue        * data  = nullptr;
            const JsonValue        * flags = nullptr;
            std::string              kind;
            int                      value = 0;
            bool                     isSet = false;



            rig.options.json = true;
            Assert::AreEqual (0, rig.Run ({ "R PC 1234", "R A 81", "R X 7F", "R Y 1", "R P A5", "R S E0", "R" }));

            replies = rig.GetReplies();
            Assert::AreEqual ((size_t) 7, replies.size());
            Assert::IsTrue   (replies[6].HasObject ("data", data));
            Assert::IsTrue   (data->HasString ("kind", kind));
            Assert::AreEqual (std::string ("registers"), kind);

            for (const auto & [name, expected] : std::initializer_list<std::pair<const char *, int>> { { "a", 0x81 }, { "x", 0x7F }, { "y", 1 }, { "p", 0xA5 }, { "s", 0xE0 }, { "pc", 0x1234 } })
            {
                Assert::IsTrue   (data->HasInt (name, value), AppleWinBatchRig::Widen (name).c_str());
                Assert::AreEqual (expected, value, AppleWinBatchRig::Widen (name).c_str());
            }

            //  $A5 is N, R, I and C.
            Assert::IsTrue (data->HasObject ("flags", flags));

            for (const auto & [name, expected] : std::initializer_list<std::pair<const char *, bool>> { { "n", true }, { "v", false }, { "r", true }, { "b", false }, { "d", false }, { "i", true }, { "z", false }, { "c", true } })
            {
                Assert::IsTrue   (flags->HasBool (name, isSet), AppleWinBatchRig::Widen (name).c_str());
                Assert::AreEqual (expected, isSet, AppleWinBatchRig::Widen (name).c_str());
            }
        }



        //  Each register by every form R takes: a space, =, spaces around =,
        //  # for decimal, and the register letter in either case.
        TEST_METHOD (R_SetsEachRegister_InEveryForm)
        {
            AppleWinBatchRig  rig;
            Cpu6502Registers  after;



            Assert::AreEqual (0, rig.Run ({ "R PC 300", "R A 41", "R A=42", "r pc = FA62", "R X #10", "r x=#11", "R Y 0", "R Y 7", "R P 30", "R S FD", "R PC HOME", "R A 'Z'" }));

            rig.ExpectBlock (1,  { Registers (0x41, 0x00, 0x00, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (2,  { Registers (0x42, 0x00, 0x00, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (3,  { Registers (0x42, 0x00, 0x00, 0x24, 0xFD, 0xFA62) });
            rig.ExpectBlock (4,  { Registers (0x42, 0x0A, 0x00, 0x24, 0xFD, 0xFA62) });
            rig.ExpectBlock (5,  { Registers (0x42, 0x0B, 0x00, 0x24, 0xFD, 0xFA62) });
            rig.ExpectBlock (6,  { Registers (0x42, 0x0B, 0x00, 0x24, 0xFD, 0xFA62) });
            rig.ExpectBlock (7,  { Registers (0x42, 0x0B, 0x07, 0x24, 0xFD, 0xFA62) });
            rig.ExpectBlock (8,  { Registers (0x42, 0x0B, 0x07, 0x30, 0xFD, 0xFA62) });
            rig.ExpectBlock (9,  { Registers (0x42, 0x0B, 0x07, 0x30, 0xFD, 0xFA62) });
            rig.ExpectBlock (10, { Registers (0x42, 0x0B, 0x07, 0x30, 0xFD, 0xFC58) });
            rig.ExpectBlock (11, { Registers (0x5A, 0x0B, 0x07, 0x30, 0xFD, 0xFC58) });

            after = rig.GetRegisters();
            Assert::AreEqual ((Byte) 0x5A,   after.a);
            Assert::AreEqual ((Byte) 0x0B,   after.x);
            Assert::AreEqual ((Byte) 0x07,   after.y);
            Assert::AreEqual ((Byte) 0x30,   after.p);
            Assert::AreEqual ((Byte) 0xFD,   after.sp);
            Assert::AreEqual ((Word) 0xFC58, after.pc);
        }



        //  S takes a byte, or an address on the stack page whose low byte it
        //  keeps; $0200 is neither.
        TEST_METHOD (R_S_TakesAByteOrAStackPageAddress)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "R PC 300", "R S FD", "R S 1FE", "R S 100", "R S 1FF", "R S 200", "R S 0" }));

            rig.ExpectBlock (1, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (2, { Registers (0, 0, 0, 0x24, 0xFE, 0x0300) });
            rig.ExpectBlock (3, { Registers (0, 0, 0, 0x24, 0x00, 0x0300) });
            rig.ExpectBlock (4, { Registers (0, 0, 0, 0x24, 0xFF, 0x0300) });
            rig.ExpectBlock (5, { "Error: value out of range", "       S takes a byte, $00-$FF." });
            rig.ExpectBlock (6, { Registers (0, 0, 0, 0x24, 0x00, 0x0300) });
            Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().sp);
        }



        //  A byte register refuses a word and keeps its value; a name that is
        //  no register, SP among them, is refused before anything is read.
        TEST_METHOD (R_Errors_LeaveTheRegisterAsItWas)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "R X 5", "R X 100", "R A 100", "R Y 1FF", "R P 100", "R Q 1", "R SP 1FF", "R A", "R PC 10000", "R PC = 10000", "R X" }));

            rig.ExpectBlock (1,  { "Error: value out of range", "       X takes a byte, $00-$FF." });
            rig.ExpectBlock (2,  { "Error: value out of range", "       A takes a byte, $00-$FF." });
            rig.ExpectBlock (3,  { "Error: value out of range", "       Y takes a byte, $00-$FF." });
            rig.ExpectBlock (4,  { "Error: value out of range", "       P takes a byte, $00-$FF." });
            rig.ExpectBlock (5,  { "Error: invalid arguments", "       Q is not a register. The registers are A, X, Y, P, S, and PC." });
            rig.ExpectBlock (6,  { "Error: invalid arguments", "       SP is not a register. The registers are A, X, Y, P, S, and PC." });
            rig.ExpectBlock (7,  { "Error: invalid arguments", "       The expression is incomplete." });
            rig.ExpectBlock (8,  { "Error: invalid arguments", "       10000 is outside $0000-$FFFF." });
            rig.ExpectBlock (9,  { "Error: invalid arguments", "       10000 is outside $0000-$FFFF." });
            rig.ExpectBlock (10, { "Error: invalid arguments", "       The expression is incomplete." });

            Assert::AreEqual ((Byte) 0x05, rig.GetRegisters().x, L"X kept its value");
            Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().a, L"A kept its value");
            Assert::AreEqual ((Byte) 0x24, rig.GetRegisters().p, L"P kept its value");
        }



        //  R =41 gives a value and no register.
        TEST_METHOD (R_ValueWithNoRegister_IsAnError)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "R =41" }));
            rig.ExpectBlock (0, { "Error: invalid arguments", "       Give the register before its value. The registers are A, X, Y, P, S, and PC." });
            Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().a);
        }



        //  Every flag set and cleared in turn, by each of the three forms, so
        //  each step changes exactly its own bit of P.
        TEST_METHOD (Flags_EachFormSetsOrClearsOnlyItsBit)
        {
            static constexpr const char  kLetters[] = "CZIDBRVN";
            AppleWinBatchRig             rig;
            std::vector<std::string>     lines  = { "R PC 300", "R P 0" };
            std::vector<Byte>            values;
            Byte                         p      = 0;



            for (int form = 0; form < 3; ++form)
            {
                for (int bit = 0; bit < 8; ++bit)
                {
                    lines.push_back (form == 0 ? std::format ("SE{}", kLetters[bit])
                                   : form == 1 ? std::format ("S{}", kLetters[bit])
                                   :             std::format ("SE {}", (char) tolower (kLetters[bit])));
                    p = (Byte) (p | (1 << bit));
                    values.push_back (p);
                }

                for (int bit = 0; bit < 8; ++bit)
                {
                    lines.push_back (form == 0 ? std::format ("CL{}", kLetters[bit])
                                   : form == 1 ? std::format ("R{}", kLetters[bit])
                                   :             std::format ("cl {}", kLetters[bit]));
                    p = (Byte) (p & ~(1 << bit));
                    values.push_back (p);
                }
            }

            Assert::AreEqual (0, rig.Run (lines));

            for (size_t i = 0; i < values.size(); ++i)
            {
                rig.ExpectBlock (i + 2, { Registers (0, 0, 0, values[i], 0xFD, 0x0300) });
            }

            Assert::AreEqual ((size_t) 48, values.size());
            Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().p);
        }



        //  CL and SE need exactly one flag letter; the names that carry their
        //  flag ignore what follows them.
        TEST_METHOD (Flags_WithoutOneLetter_AreErrors_AndTheNamedFormsIgnoreArguments)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "R P 0", "CL", "CL Q", "SE CZ", "SE", "SEC 5", "CLC FROB", "SEN" }));

            for (size_t i : { 1, 2, 3, 4 })
            {
                rig.ExpectBlock (i, { "Error: invalid arguments", "       A flag is one of C, Z, I, D, B, R, V, or N." });
            }

            Assert::IsTrue   (rig.GetBlock (5)[0].find ("P:01") != std::string::npos, L"SEC 5 set C");
            Assert::IsTrue   (rig.GetBlock (6)[0].find ("P:00") != std::string::npos, L"CLC FROB cleared it");
            Assert::AreEqual ((Byte) 0x80, rig.GetRegisters().p);
        }



        //  POP pulls a byte into A; PPOP pulls a word into PC with no +1. The
        //  pointer wraps at the top of the page.
        TEST_METHOD (Pop_AndPpop_PullFromTheStack_AndWrap)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "R PC 300", "R S FC", "MEB 1FD 12 34 56", "POP", "PPOP",
                                            "R S FF", "MEB 100 77", "POP",
                                            "R S FE", "MEB 1FF 9A", "MEB 100 BC", "PPOP" }));

            rig.ExpectBlock (3,  { Registers (0x12, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (4,  { Registers (0x12, 0, 0, 0x24, 0xFF, 0x5634) });
            rig.ExpectBlock (7,  { Registers (0x77, 0, 0, 0x24, 0x00, 0x5634) });
            rig.ExpectBlock (11, { Registers (0x77, 0, 0, 0x24, 0x00, 0xBC9A) });

            Assert::AreEqual ((Word) 0xBC9A, rig.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x00,   rig.GetRegisters().sp);
        }



        //  PUSH stores at $0100+SP and moves SP down, as PHA does, and wraps
        //  from $00 to $FF; the reply is the stack as STACK shows it.
        TEST_METHOD (Push_StoresAsPhaDoes_AndWraps)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "R S FF", "PUSH 11 22", "R S 0", "PUSH 33", "PUSH", "PUSH 1234" }));

            rig.ExpectBlock (1, { "S:FD", "01FF: 11", "01FE: 22" });
            rig.ExpectBlock (3, { "S:FF" });
            rig.ExpectBlock (4, { "Error: invalid arguments", "       PUSH takes one or more bytes." });
            rig.ExpectBlock (5, { "Error: invalid arguments", "       1234 is not a byte." });

            Assert::AreEqual ((Byte) 0x11, rig.PeekRam (0x01FF));
            Assert::AreEqual ((Byte) 0x22, rig.PeekRam (0x01FE));
            Assert::AreEqual ((Byte) 0x33, rig.PeekRam (0x0100));
            Assert::AreEqual ((Byte) 0xFF, rig.GetRegisters().sp);
        }



        //  STACK lists from $01FF down to SP+1, and only the pointer when the
        //  stack is empty.
        TEST_METHOD (Stack_ListsFromTheTopDownToTheEntryAboveSp)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "MEB 1FD A1 B2 C3", "R S FF", "STACK", "R S FC", "STACK" }));

            rig.ExpectBlock (2, { "S:FF" });
            rig.ExpectBlock (4, { "S:FC", "01FF: C3", "01FE: B2", "01FD: A1" });
        }



        TEST_METHOD (Stack_AsJson_IsThePointerAndTheEntries)
        {
            AppleWinBatchRig         rig;
            std::vector<JsonValue>   replies;
            const JsonValue        * data    = nullptr;
            const JsonValue        * entries = nullptr;
            std::string              kind;
            int                      value   = 0;



            rig.options.json = true;
            Assert::AreEqual (0, rig.Run ({ "MEB 1FE B2 C3", "R S FD", "STACK" }));

            replies = rig.GetReplies();
            Assert::IsTrue   (replies[2].HasObject ("data", data));
            Assert::IsTrue   (data->HasString ("kind", kind));
            Assert::AreEqual (std::string ("stack"), kind);
            Assert::IsTrue   (data->HasInt ("sp", value));
            Assert::AreEqual (0xFD, value);
            Assert::IsTrue   (data->HasArray ("entries", entries));
            Assert::AreEqual ((size_t) 2, entries->GetArraySize());
            Assert::IsTrue   (entries->GetArrayElement (0).HasInt ("address", value));
            Assert::AreEqual (0x01FF, value);
            Assert::IsTrue   (entries->GetArrayElement (0).HasInt ("value", value));
            Assert::AreEqual (0xC3, value);
            Assert::IsTrue   (entries->GetArrayElement (1).HasInt ("address", value));
            Assert::AreEqual (0x01FE, value);
            Assert::IsTrue   (entries->GetArrayElement (1).HasInt ("value", value));
            Assert::AreEqual (0xB2, value);
        }



        //  = sets the program counter from any expression; alone it is an
        //  error, and without its space it is no command at all.
        TEST_METHOD (Equals_SetsTheProgramCounter)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "= 300", "=", "=300", "= HOME", "= PC+3" }));

            rig.ExpectBlock (0, { Registers (0, 0, 0, 0x24, 0xFD, 0x0300) });
            rig.ExpectBlock (1, { "Error: invalid arguments", "       = takes the new program counter." });
            rig.ExpectBlock (2, { "Error: unknown command", "       =300 is not a command." });
            rig.ExpectBlock (3, { Registers (0, 0, 0, 0x24, 0xFD, 0xFC58) });
            rig.ExpectBlock (4, { Registers (0, 0, 0, 0x24, 0xFD, 0xFC5B) });
            Assert::AreEqual ((Word) 0xFC5B, rig.GetRegisters().pc);
        }



        //  The window's console reaches the same handlers, and changes the
        //  machine the window is showing.
        TEST_METHOD (Console_SetsTheRegistersOfTheMachineItShows)
        {
            AppleWinConsoleRig  console;
            Reply               reply;



            reply = console.TypeOk ("R X 99");
            Assert::AreEqual ((size_t) 1, reply.text.size());
            Assert::IsTrue   (reply.text[0].starts_with ("A:"));
            Assert::AreEqual ((Byte) 0x99, console.GetRegisters().x);

            console.TypeOk ("SEC");
            Assert::AreEqual ((Byte) 0x01, (Byte) (console.GetRegisters().p & 0x01));

            console.TypeOk ("= 302");
            Assert::AreEqual ((Word) 0x0302, console.GetRegisters().pc);
        }
    };
}
