#include "Pch.h"

#include "../TestHelpers.h"
#include "Assembler.h"
#include "Devices/Printer/ParallelFirmware.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  FirmwareParityTests
//
//  Re-assembles the embedded parallel-printer firmware source with the
//  in-repo assembler and asserts it matches the embedded byte array. This is
//  the FR-003 provenance gate: the shipped firmware is original work built
//  from in-repo source, and the source and bytes can never silently drift.
//
////////////////////////////////////////////////////////////////////////////////

namespace FirmwareParityTests
{
    TEST_CLASS (FirmwareParityTests)
    {
    public:

        TEST_METHOD (SourceAssemblesToEmbeddedBytes)
        {
            TestCpu          cpu;
            cpu.InitForTest ();

            Assembler        asm6502 (cpu.GetInstructionSet ());
            AssemblyResult   result = asm6502.Assemble (s_kParallelFirmwareSource);
            size_t           count  = sizeof (s_kParallelFirmwareBytes);
            size_t           i      = 0;

            Assert::IsTrue (result.success, L"firmware source must assemble cleanly");
            Assert::AreEqual ((Word) s_kParallelFirmwareOrigin, result.startAddress);
            Assert::AreEqual (count, result.bytes.size ());

            for (i = 0; i < count; i++)
            {
                Assert::AreEqual (s_kParallelFirmwareBytes[i], result.bytes[i]);
            }
        }
    };
}
