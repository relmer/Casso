#include "Pch.h"

#include "Debugger/Handlers/RegisterHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHandlersTests
//
//  R, the flag commands and the stack commands, through the parser and the
//  formatter, against the mock target.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (RegisterHandlersTests)
    {
    public:

        using Rig = HandlerRig<RegisterHandlers>;



        TEST_METHOD (R_ShowsRegisters_AndAlias)
        {
            Rig  rig;



            rig.target.registers = { 0x0300, 0x41, 0x01, 0x02, 0xFD, 0x30 };

            Assert::AreEqual (std::string ("A:41 X:01 Y:02 P:30 S:FD PC:0300  ..RB...."), rig.RunOk ("R").text.at (0));
            Assert::AreEqual (std::string ("A:41 X:01 Y:02 P:30 S:FD PC:0300  ..RB...."), rig.RunOk ("register").text.at (0));
        }



        TEST_METHOD (R_SetsRegister_ByteRangeChecked)
        {
            Rig  rig;



            rig.RunOk ("R A=41");
            Assert::AreEqual ((uint8_t) 0x41, rig.target.registers.a);

            rig.RunOk ("r pc = FA62");
            Assert::AreEqual ((uint16_t) 0xFA62, rig.target.registers.pc);

            rig.RunOk ("R S 1FE");
            Assert::AreEqual ((uint8_t) 0xFE, rig.target.registers.sp);

            rig.RunOk ("R X #10");
            Assert::AreEqual ((uint8_t) 10, rig.target.registers.x);

            Assert::AreEqual (std::string ("A:41 X:0A Y:00 P:30 S:FE PC:FA62  ..RB...."), rig.RunOk ("R Y 0").text.at (0));

            rig.RunFails ("R X 100", "value out of range");
            Assert::AreEqual ((uint8_t) 10, rig.target.registers.x, L"a rejected value changes nothing");
            rig.RunFails ("R Q 1",   "invalid arguments");
        }



        TEST_METHOD (Flags_SetAndClear_EveryLetter)
        {
            static constexpr const char * kLetters = "CZIDBRVN";
            Rig     rig;
            size_t  checked = 0;



            rig.target.registers.p = 0;

            for (size_t i = 0; kLetters[i] != '\0'; ++i)
            {
                std::string  letter (1, kLetters[i]);
                Byte         bit = (Byte) (1 << i);



                rig.RunOk ("SE" + letter);
                Assert::AreEqual (bit, rig.target.registers.p, L"SEx sets only its bit");

                rig.RunOk ("CL " + letter);
                Assert::AreEqual ((Byte) 0, rig.target.registers.p, L"CL x clears it");

                rig.RunOk ("S" + letter);
                Assert::AreEqual (bit, rig.target.registers.p, L"the Sx alias sets it");

                rig.RunOk ("R" + letter);
                Assert::AreEqual ((Byte) 0, rig.target.registers.p, L"the Rx alias clears it");
                ++checked;
            }

            Assert::AreEqual ((size_t) 8, checked);
            Assert::AreEqual (std::string ("A:00 X:00 Y:00 P:01 S:FF PC:0300  .......C"), rig.RunOk ("SEC").text.at (0));
            rig.RunFails ("SE Q", "invalid arguments");
        }



        TEST_METHOD (Stack_PushPopAndView)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunOk ("PUSH 12 34");
            Assert::AreEqual ((Byte) 0x12,  rig.target.memory[0x01FF]);
            Assert::AreEqual ((Byte) 0x34,  rig.target.memory[0x01FE]);
            Assert::AreEqual ((uint8_t) 0xFD, rig.target.registers.sp);
            Assert::AreEqual ((size_t) 3,   reply.text.size());
            Assert::AreEqual (std::string ("S:FD"),     reply.text[0]);
            Assert::AreEqual (std::string ("01FF: 12"), reply.text[1]);
            Assert::AreEqual (std::string ("01FE: 34"), reply.text[2]);

            reply = rig.RunOk ("POP");
            Assert::AreEqual ((uint8_t) 0xFE, rig.target.registers.sp);
            Assert::AreEqual ((uint8_t) 0x34, rig.target.registers.a);

            rig.RunOk ("R S FD");
            reply = rig.RunOk ("PPOP");
            Assert::AreEqual ((uint8_t) 0xFF,    rig.target.registers.sp);
            Assert::AreEqual ((uint16_t) 0x1234, rig.target.registers.pc, L"a word is pulled as it lies, low byte first");

            reply = rig.RunOk ("STACK");
            Assert::AreEqual ((size_t) 1, reply.text.size(), L"an empty stack shows only the pointer");
            Assert::AreEqual (std::string ("S:FF"), reply.text[0]);

            rig.RunFails ("PUSH", "invalid arguments");
        }
    };
}
