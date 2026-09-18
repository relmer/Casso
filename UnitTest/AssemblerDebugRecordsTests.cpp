#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"
#include "MockFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerDebugRecordsTests
//
//  Which source line produced each byte range, which is what a debug file's
//  line records are written from (FR-033, FR-033a).
//
//  A RANGE CARRIES EVERY LINE THAT PRODUCED IT, OUTERMOST FIRST. A line in the
//  source proper, or in an included file, is one position. A line a macro
//  expansion produced is the invocation, then each macro body line down to the
//  one that assembled the bytes, so a two-level macro gives three.
//
//  An empty file name is the top-level input, as it is for diagnostics.
//
////////////////////////////////////////////////////////////////////////////////

namespace AssemblerDebugRecordsTests
{
    static void AssertPositions (const DebugLineRecord & record, std::initializer_list<std::pair<const char *, int>> expected)
    {
        size_t  index = 0;



        Assert::AreEqual (expected.size(), record.positions.size());

        for (const std::pair<const char *, int> & each : expected)
        {
            Assert::AreEqual (std::string (each.first), record.positions[index].file);
            Assert::AreEqual (each.second,              record.positions[index].line);
            index++;
        }
    }



    static const DebugLineRecord & RecordAt (const AssemblyResult & result, Word address)
    {
        for (const DebugLineRecord & record : result.debugLines)
        {
            if (record.address == address)
            {
                return record;
            }
        }

        Assert::Fail (std::format (L"no record at ${:04X}", address).c_str());
        return result.debugLines.front();
    }





    TEST_CLASS (As65Tests)
    {
    public:

        static AssemblyResult AssembleSample()
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;



            reader.files["defs.inc"] = "    ldx #2\n";
            opts.fileReader          = &reader;

            return Assembler (cpu.GetInstructionSet(), opts).Assemble (
                "    .org $0300\n"          //  1
                "    include \"defs.inc\"\n" //  2
                "inner macro\n"              //  3
                "    nop\n"                  //  4
                "    endm\n"                 //  5
                "outer macro\n"              //  6
                "    lda #1\n"               //  7
                "    inner\n"                //  8
                "    endm\n"                 //  9
                "    outer\n"                // 10
                "    rts\n");                // 11
        }



        TEST_METHOD (EveryByteHasARecord)
        {
            AssemblyResult  result = AssembleSample();
            size_t          bytes  = 0;



            Assert::IsTrue (result.success);

            for (const DebugLineRecord & record : result.debugLines)
            {
                bytes += record.size;
            }

            Assert::AreEqual (result.bytes.size(), bytes, L"no byte is left without a line");
        }


        TEST_METHOD (AnIncludedLineNamesItsFile)
        {
            AssertPositions (RecordAt (AssembleSample(), 0x0300), { { "defs.inc", 1 } });
        }


        TEST_METHOD (AMacroLineNamesTheInvocationThenTheBody)
        {
            AssemblyResult  result = AssembleSample();



            AssertPositions (RecordAt (result, 0x0302), { { "", 10 }, { "", 7 } });
            Assert::AreEqual ((size_t) 2, RecordAt (result, 0x0302).size);
        }


        TEST_METHOD (ANestedMacroLineNamesEveryLevel)
        {
            AssertPositions (RecordAt (AssembleSample(), 0x0304), { { "", 10 }, { "", 8 }, { "", 4 } });
        }


        TEST_METHOD (APlainLineIsOnePosition)
        {
            AssertPositions (RecordAt (AssembleSample(), 0x0305), { { "", 11 } });
        }


        TEST_METHOD (ALineThatEmitsNothingHasNoRecord)
        {
            AssemblyResult  result = AssembleSample();



            for (const DebugLineRecord & record : result.debugLines)
            {
                Assert::IsTrue (record.size > 0);
            }

            Assert::AreEqual ((size_t) 4, result.debugLines.size(), L"ldx, lda, nop, rts");
        }
    };





    TEST_CLASS (MerlinTests)
    {
    public:

        TEST_METHOD (AMerlinMacroNamesTheInvocationThenTheBody)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;



            opts.dialect = DialectId::Merlin;
            result       = Assembler (cpu.GetInstructionSet(), opts).Assemble (
                " ORG $300\n"       // 1
                "SETONE MAC\n"      // 2
                " LDA #$01\n"       // 3
                " <<<\n"            // 4
                " SETONE\n"         // 5
                " RTS\n");          // 6

            Assert::IsTrue  (result.success);
            AssertPositions (RecordAt (result, 0x0300), { { "", 5 }, { "", 3 } });
            AssertPositions (RecordAt (result, 0x0302), { { "", 6 } });
        }
    };
}
