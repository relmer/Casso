#include "Pch.h"

#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlersTests
//
//  The data directives, the block list, U over code and data, and A.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DataDirectiveHandlersTests)
    {
    public:

        using Rig = HandlerRig<DataDirectiveHandlers>;

        struct CpuRig : Rig
        {
            TestCpu  cpu;

            CpuRig()
            {
                cpu.InitForTest();
                target.instructionSet = cpu.GetInstructionSet();
            }
        };

        static void Store (Rig & rig, Word address, std::initializer_list<Byte> bytes)
        {
            for (Byte b : bytes)
            {
                rig.target.memory[address++] = b;
            }
        }



        TEST_METHOD (Directives_NameAndWidth_ListedInAddressOrder)
        {
            Rig                       rig;
            std::vector<std::string>  list;



            Assert::AreEqual (std::string ("B_0300       $0300-$0303  bytes"),   rig.RunOk ("DB 300:303").text.at (0));
            Assert::AreEqual (std::string ("TABLE        $0310-$0313  words"),   rig.RunOk ("DW TABLE 310:313").text.at (0));
            Assert::AreEqual (std::string ("T_0320       $0320-$0324  text"),    rig.RunOk ("ASC 320,5").text.at (0));
            Assert::AreEqual (std::string ("A_0330       $0330-$0331  address"), rig.RunOk ("DA 330").text.at (0), L"one address is two bytes");
            Assert::AreEqual (std::string ("F_0340       $0340-$0344  float"),   rig.RunOk ("DF 340").text.at (0), L"one float is five bytes");
            Assert::AreEqual (std::string ("B_02F0       $02F0-$02F0  bytes"),   rig.RunOk ("Z 2F0").text.at (0), L"Z is DB");
            Assert::AreEqual (std::string ("NAME         $0360-$0360  bytes"),   rig.RunOk ("DB8 NAME = 360").text.at (0));

            list = rig.RunOk ("B").text;
            Assert::AreEqual ((size_t) 7, list.size());
            Assert::IsTrue   (list[0].starts_with ("B_02F0"), L"listed by address");
            Assert::IsTrue   (list[6].starts_with ("NAME"));
            Assert::AreEqual ((size_t) 7, rig.RunOk ("DB").text.size(), L"a directive with no address lists");
        }



        TEST_METHOD (X_RemovesTrimsAndSplits)
        {
            Rig                       rig;
            std::vector<std::string>  list;



            rig.RunOk ("DB 300:30F");

            list = rig.RunOk ("X 300:303").text;
            Assert::AreEqual ((size_t) 1, list.size());
            Assert::AreEqual (std::string ("B_0300       $0304-$030F  bytes"), list[0], L"trimmed, name kept");

            list = rig.RunOk ("X 308:309").text;
            Assert::AreEqual ((size_t) 2, list.size());
            Assert::AreEqual (std::string ("B_0300       $0304-$0307  bytes"), list[0]);
            Assert::AreEqual (std::string ("B_0300       $030A-$030F  bytes"), list[1]);

            rig.RunOk ("DW 306:30B");
            list = rig.RunOk ("B").text;
            Assert::AreEqual ((size_t) 3, list.size(), L"a new block takes its range from the ones it covers");
            Assert::AreEqual (std::string ("W_0306       $0306-$030B  words"), list[1]);

            Assert::AreEqual (std::string ("No data blocks."), rig.RunOk ("X 300:3FF").text.at (0));
            rig.RunFails ("X", "invalid arguments");
        }



        TEST_METHOD (U_ShowsLoadedSymbols)
        {
            CpuRig                    rig;
            std::vector<std::string>  lines;



            //  $0300: STA ($06),Y / BNE $0300, with a name for each address.
            Store (rig, 0x0300, { 0x91, 0x06, 0xD0, 0xFC });
            rig.session.GetSymbols().Add (SymbolTableId::User, "LOOP", 0x0300);
            rig.session.GetSymbols().Add (SymbolTableId::User, "PTR",  0x0006);

            lines = rig.RunOk ("U 300:303").text;
            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("0300: 91 06    LOOP STA  (PTR),Y"), lines[0]);
            Assert::AreEqual (std::string ("0302: D0 FC         BNE  LOOP"),    lines[1]);
        }



        TEST_METHOD (U_HonorsDataBlocks_AndContinues)
        {
            CpuRig                    rig;
            std::vector<std::string>  lines;



            // $0300: LDA #$41, four data bytes, RTS, then a word table, text,
            // an address and a float.
            Store (rig, 0x0300, { 0xA9, 0x41, 0x01, 0x02, 0x03, 0x04, 0x60 });
            Store (rig, 0x0307, { 0x34, 0x12, 0x78, 0x56 });
            Store (rig, 0x030B, { 0xC8, 0xC9, 0x8D });
            Store (rig, 0x030E, { 0xED, 0xFD });
            Store (rig, 0x0310, { 0x81, 0x40, 0x00, 0x00, 0x00 });
            rig.RunOk ("DB 302:305");
            rig.RunOk ("DW2 307:30A");
            rig.RunOk ("ASC 30B:30D");
            rig.RunOk ("DA 30E");
            rig.RunOk ("DF 310");

            lines = rig.RunOk ("U 300:314").text;
            Assert::AreEqual ((size_t) 7, lines.size());
            Assert::AreEqual (std::string ("0300: A9 41           LDA  #$41"),                lines[0]);
            Assert::AreEqual (std::string ("0302: 01 02 03 04 B_0302 DB   $01,$02,$03,$04"), lines[1]);
            Assert::AreEqual (std::string ("0306: 60              RTS"),                      lines[2]);
            Assert::AreEqual (std::string ("0307: 34 12 78 56 W_0307 DW   $1234,$5678"),     lines[3]);
            Assert::AreEqual (std::string ("030B: C8 C9 8D T_030B ASC  \"HI.\""),            lines[4]);
            Assert::AreEqual (std::string ("030E: ED FD    A_030E DA   $FDED"),              lines[5]);
            Assert::AreEqual (std::string ("0310: 81 40 00 00 00 F_0310 DF   1.5"),          lines[6]);

            lines = rig.RunOk ("U").text;
            Assert::AreEqual ((size_t) 20, lines.size(), L"U alone shows 20 lines");
            Assert::IsTrue   (lines[0].starts_with ("0315:"), L"from where the last listing ended");

            Assert::IsTrue   (rig.RunOk ("300L").text.at (0).starts_with ("0300: A9 41"), L"the Monitor form lists too");

            rig.RunOk ("DB8 320:32F");
            lines = rig.RunOk ("U 320:32F").text;
            Assert::AreEqual ((size_t) 2, lines.size(), L"eight bytes per line");
            Assert::IsTrue   (lines[1].starts_with ("0328:"));
        }



        TEST_METHOD (A_EntersAssembly_AtAddressOrPc)
        {
            CpuRig  rig;



            Assert::AreEqual (std::string ("Assembling at $0300. Enter a blank line to stop."), rig.RunOk ("A 300").text.at (0));
            Assert::IsTrue   (rig.session.IsAssembling());

            rig.RunOk ("LDA #$41");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            rig.RunOk ("");
            Assert::IsFalse  (rig.session.IsAssembling());

            rig.target.registers.pc = 0x0400;
            Assert::AreEqual (std::string ("Assembling at $0400. Enter a blank line to stop."), rig.RunOk ("A").text.at (0));
            rig.RunOk ("RTS");
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0400]);
        }
    };
}
