#include "Pch.h"

#include "Cpu6502.h"
#include "CycleReference.h"
#include "Core/Cpu65C02Table.h"
#include "Core/TextEncoding.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CycleReferenceTests
//
//  The generator of docs/cycle-reference.md, checked without the file: every
//  opcode has a row, the conditional cycles are stated and the disputed slots
//  are listed. The comparison with the copy in the tree writes the regenerated
//  copy to the system temp folder, so it is in the scenario suite's
//  CycleReferenceTests.
//
//  The tables come from the SHIPPING types (Cpu6502 and the CMOS table the
//  assembler is handed), not from the test doubles, so what is pinned is what
//  the product bills.
//
////////////////////////////////////////////////////////////////////////////////

namespace CycleReferenceTests
{
    TEST_CLASS (CycleReferenceTests)
    {
    public:

        static constexpr int  kOpcodeCount = CycleReference::kOpcodeCount;


        //////////////////////////////////////////////////////////////////////
        //  Document_CoversEveryOpcode
        //
        //  A generator that produced nothing would compare equal to a document
        //  that also had nothing in it, and both would look healthy. Count the
        //  rows before trusting the comparison in the scenario suite's
        //  Document_MatchesTheInstructionTables.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_CoversEveryOpcode)
        {
            std::string  generated = BuildDocument();
            int          rows      = 0;
            int          opcode    = 0;

            for (opcode = 0; opcode < kOpcodeCount; ++opcode)
            {
                std::string  marker = std::format ("\n| ${:02X} | ", opcode);

                if (generated.find (marker) != std::string::npos)
                {
                    ++rows;
                }
            }

            Assert::AreEqual (kOpcodeCount, rows, L"the generated reference must carry one row per opcode");
        }


        //////////////////////////////////////////////////////////////////////
        //  Document_StatesTheConditionalCycles
        //
        //  The counts in the table are useless without the three run-time
        //  additions that are deliberately not in them. Pin the prose so a
        //  future edit cannot quietly drop it.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_StatesTheConditionalCycles)
        {
            std::string  generated = BuildDocument();

            Assert::AreNotEqual (std::string::npos, generated.find ("Page crossing, +1"),   L"page-crossing rule");
            Assert::AreNotEqual (std::string::npos, generated.find ("Branch taken, +1"),    L"branch rule");
            Assert::AreNotEqual (std::string::npos, generated.find ("Decimal arithmetic"),  L"65C02 decimal rule");
        }


        //////////////////////////////////////////////////////////////////////
        //  Document_NamesTheDisputedSlots
        //
        //  The three opcodes where Casso and the upstream vectors disagree are
        //  the document's only claim about its own trustworthiness, so a reader
        //  must not have to go looking for them.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_NamesTheDisputedSlots)
        {
            std::string  generated = BuildDocument();

            Assert::AreNotEqual (std::string::npos, generated.find ("`$DB`"), L"the skipped opcode");
            Assert::AreNotEqual (std::string::npos, generated.find ("`$5C`"), L"the first cycle-exempt opcode");
            Assert::AreNotEqual (std::string::npos, generated.find ("`$CB`"), L"the second cycle-exempt opcode");
        }


    private:

        //////////////////////////////////////////////////////////////////////
        //  BuildDocument
        //////////////////////////////////////////////////////////////////////

        static std::string BuildDocument()
        {
            Cpu6502  nmos;

            return CycleReference::Format (nmos.GetInstructionSet(), GetCpu65C02InstructionSet());
        }
    };
}
