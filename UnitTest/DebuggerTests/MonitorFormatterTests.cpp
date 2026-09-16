#include "Pch.h"

#include "Debugger/MonitorFormatter.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorFormatterTests
    //
    //  The Monitor's own output layouts, from contracts/command-modes.md.
    //
    //  These are the formats a reader recognizes from a real `*` prompt, and
    //  a script that greps the output depends on them exactly, so they are
    //  asserted as whole lines rather than by substring.
    //
    //  The layout does not vary by machine: the same reply renders the same
    //  text on a ][ and on a //c, which is what lets a batch script's
    //  expected output be one file.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorFormatterTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        //  The lines a reply renders to.
        static std::vector<std::string> Render (Reply & reply)
        {
            MonitorFormatter::Format (reply);
            return reply.text;
        }



        static Reply MakeReply (const ReplyData & data)
        {
            Reply  reply;

            reply.data = data;
            return reply;
        }



        static MemoryRow MakeRow (Word address, std::initializer_list<Byte> bytes)
        {
            MemoryRow  row;

            row.address = address;
            row.region  = MemoryRegion::MainRam;

            for (Byte value : bytes)
            {
                row.bytes.push_back (value);
            }

            return row;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Examining
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Examine_PrintsTheAddressThenTheBytes)
        {
            MemoryData                data;
            Reply                     reply;
            std::vector<std::string>  lines;



            data.rows.push_back (MakeRow (0x0300, { 0xA9, 0x00, 0x8D, 0x00, 0x03, 0x60 }));
            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (size_t (1), lines.size());
            Assert::AreEqual (std::string ("0300- A9 00 8D 00 03 60"), lines[0]);
        }



        //  ROWS BREAK ON EIGHT-BYTE BOUNDARIES, NOT ON THE RANGE. `303.30F`
        //  prints five bytes against $0303 and then eight against $0308,
        //  because the Monitor labels a row with the address that starts it
        //  rather than with wherever the reader began.
        TEST_METHOD (Examine_RowsAlignToEightByteBoundaries)
        {
            MemoryData                data;
            Reply                     reply;
            std::vector<std::string>  lines;



            data.rows.push_back (MakeRow (0x0303, { 0x01, 0x02, 0x03, 0x04, 0x05 }));
            data.rows.push_back (MakeRow (0x0308, { 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D }));

            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (size_t (2), lines.size());
            Assert::AreEqual (std::string ("0303- 01 02 03 04 05"),             lines[0]);
            Assert::AreEqual (std::string ("0308- 06 07 08 09 0A 0B 0C 0D"),    lines[1]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Listing
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (List_PrintsAddressBytesMnemonicAndOperand)
        {
            DisassemblyData           data;
            DisassemblyLine           line;
            Reply                     reply;
            std::vector<std::string>  lines;



            line.instruction.address  = 0x0300;
            line.instruction.bytes    = { 0xA9, 0x00 };
            line.instruction.mnemonic = "LDA";
            line.instruction.operand  = "#$00";

            data.lines.push_back (line);
            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (size_t (1), lines.size());
            Assert::AreEqual (std::string ("0300-   A9 00       LDA   #$00"), lines[0]);
        }



        //  A three-byte instruction and a one-byte instruction keep the
        //  mnemonic in the same column.
        TEST_METHOD (List_KeepsTheMnemonicColumn)
        {
            DisassemblyData           data;
            DisassemblyLine           three;
            DisassemblyLine           one;
            Reply                     reply;
            std::vector<std::string>  lines;



            three.instruction.address  = 0x0302;
            three.instruction.bytes    = { 0x20, 0xED, 0xFD };
            three.instruction.mnemonic = "JSR";
            three.instruction.operand  = "$FDED";

            one.instruction.address  = 0x0305;
            one.instruction.bytes    = { 0x60 };
            one.instruction.mnemonic = "RTS";

            data.lines.push_back (three);
            data.lines.push_back (one);

            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (std::string ("0302-   20 ED FD    JSR   $FDED"), lines.at (0));
            Assert::AreEqual (std::string ("0305-   60          RTS"),         lines.at (1));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Verify, registers, arithmetic, search
        //
        ////////////////////////////////////////////////////////////////////////

        //  One line per difference, the source byte in parentheses, and
        //  nothing at all when the ranges match.
        TEST_METHOD (Verify_PrintsOneLinePerDifference)
        {
            CompareData               data;
            Reply                     reply;
            std::vector<std::string>  lines;



            data.compared = 16;
            data.differences.push_back (CompareDifference { 0x0303, 0x41, 0x0403, 0x42 });

            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (size_t (1), lines.size());
            Assert::AreEqual (std::string ("0303-41 (42)"), lines[0]);
        }



        TEST_METHOD (Verify_PrintsNothingWhenTheRangesMatch)
        {
            CompareData  data;
            Reply        reply;



            data.compared = 16;
            reply         = MakeReply (data);

            Assert::IsTrue (Render (reply).empty(), L"a verify that found nothing says nothing");
        }



        //  The Monitor's register line has no PC in it.
        TEST_METHOD (Registers_AreTheMonitorLine)
        {
            Cpu6502Registers  registers = {};
            RegistersData     data;
            Reply             reply;



            registers.a  = 0x00;
            registers.x  = 0x00;
            registers.y  = 0x00;
            registers.p  = 0x30;
            registers.sp = 0xFF;
            registers.pc = 0x0300;

            Assert::AreEqual (std::string ("A=00 X=00 Y=00 P=30 S=FF"), MonitorFormatter::FormatRegisters (registers));

            data.registers = registers;
            reply          = MakeReply (data);

            Assert::AreEqual (std::string ("A=00 X=00 Y=00 P=30 S=FF"), Render (reply).at (0));
        }



        //  A stop prints the registers; with the instruction line the step's
        //  reply carries, that is the original ]['s step display.
        TEST_METHOD (Stop_PrintsTheRegisterLine)
        {
            StopEvent  stop;



            stop.reason       = StopReason::Step;
            stop.pc           = 0x0302;
            stop.registers.a  = 0x41;
            stop.registers.p  = 0x30;
            stop.registers.sp = 0xFF;

            Assert::AreEqual (std::string ("A=41 X=00 Y=00 P=30 S=FF"), MonitorFormatter::FormatStop (stop));
        }



        //  Eight-bit, as the Monitor's arithmetic is: the handler truncates
        //  FF+FF to FE and this prints the two digits.
        TEST_METHOD (Arithmetic_PrintsEqualsAndTwoDigits)
        {
            CalcData  data;
            Reply     reply;



            data.value = 0x00FE;
            reply      = MakeReply (data);

            Assert::AreEqual (std::string ("=FE"), Render (reply).at (0));
        }



        TEST_METHOD (SearchHits_AreBareAddresses)
        {
            SearchHitsData            data;
            Reply                     reply;
            std::vector<std::string>  lines;



            data.addresses.push_back (0x0301);
            data.addresses.push_back (0x03FE);

            reply = MakeReply (data);
            lines = Render (reply);

            Assert::AreEqual (size_t (2), lines.size());
            Assert::AreEqual (std::string ("0301"), lines[0]);
            Assert::AreEqual (std::string ("03FE"), lines[1]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Errors
        //
        ////////////////////////////////////////////////////////////////////////

        //  THE MONITOR'S `ERR` AND THE REASON BOTH. A script reading Monitor
        //  output looks for ERR, which is all the real Monitor gives it; a
        //  person needs to know which error it was, and the ROM never told
        //  them. Both are printed rather than choosing between them.
        TEST_METHOD (AnError_IsErrThenTheTwoLineReason)
        {
            Reply                     reply;
            std::vector<std::string>  lines;



            reply.SetError (CommandStatus::Unknown, "unknown command", "FROB is not a command.");
            lines = Render (reply);

            Assert::AreEqual (size_t (3), lines.size());
            Assert::AreEqual (std::string ("ERR"),                       lines[0]);
            Assert::AreEqual (std::string ("Error: unknown command"),    lines[1]);
            Assert::AreEqual (std::string ("       FROB is not a command."), lines[2]);
        }
    };
}
