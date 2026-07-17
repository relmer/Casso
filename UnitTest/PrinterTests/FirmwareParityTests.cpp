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
            cpu.InitForTest();

            Assembler        asm6502 (cpu.GetInstructionSet());
            AssemblyResult   result = asm6502.Assemble (s_kParallelFirmwareSource);
            size_t           count  = sizeof (s_kParallelFirmwareBytes);
            size_t           i      = 0;

            Assert::IsTrue (result.success, L"firmware source must assemble cleanly");
            Assert::AreEqual ((Word) s_kParallelFirmwareOrigin, result.startAddress);
            Assert::AreEqual (count, result.bytes.size());

            for (i = 0; i < count; i++)
            {
                Assert::AreEqual (s_kParallelFirmwareBytes[i], result.bytes[i]);
            }
        }


        // A printer card MUST NOT advertise the Pascal 1.1 intelligent-firmware
        // signature ($Cn05=$38 / $Cn07=$18 / $Cn0B=$01): on the Apple IIe that
        // makes PR#n hook the card for INPUT too, and our output-only stub then
        // floods the printer with echoed garbage (the "PR#1 spams Y" bug). This
        // pins the card as a dumb, output-only card so PR#n stays CSW-only.
        TEST_METHOD (DoesNotClaimThePascal11FirmwareSignature)
        {
            bool  isPascalFirmware = (s_kParallelFirmwareBytes[0x05] == 0x38) &&
                                     (s_kParallelFirmwareBytes[0x07] == 0x18) &&
                                     (s_kParallelFirmwareBytes[0x0B] == 0x01);

            Assert::IsFalse (isPascalFirmware,
                             L"the printer card must not claim the Pascal 1.1 protocol");
            Assert::AreEqual ((Byte) 0x4C, s_kParallelFirmwareBytes[0],
                              L"$Cn00 is still JMP (the CSW output entry)");
        }


        // Loads the embedded firmware at the slot-1 ROM page, marks the card
        // ready, and enters OUTPUT the way COUT does (JMP $C100 with the
        // character in A). Runs to the RTS sentinel with a hard step cap so a
        // runaway fails instead of hanging. Returns the step count.
        static int RunOutput (TestCpu & cpu, Byte character, int cap = 400)
        {
            int  steps = 0;

            cpu.InitForTest();

            for (size_t i = 0; i < sizeof (s_kParallelFirmwareBytes); i++)
            {
                cpu.Poke ((Word) (s_kParallelFirmwareOrigin + i), s_kParallelFirmwareBytes[i]);
            }

            cpu.Poke (0xC091, 0x83);        // slot-1 status port: ready (bit 7 set)
            cpu.RegSP () = 0xFF;
            cpu.DoPushWord (0x7FFF);        // RTS returns here + 1 == 0x8000
            cpu.RegA ()  = character;
            cpu.RegPC () = (Word) s_kParallelFirmwareOrigin;

            while (cpu.RegPC () != 0x8000 && steps < cap)
            {
                cpu.Step ();
                steps++;
            }

            return steps;
        }


        TEST_METHOD (OutputLatchesCharacterAndReturnsCleanly)
        {
            TestCpu  cpu;
            int      steps = RunOutput (cpu, 0xC1);   // 'A' | high bit

            Assert::IsTrue    (steps < 400, L"OUTPUT must terminate -- no runaway");
            Assert::AreEqual  ((Byte) 0xC1, cpu.Peek (0xC090), L"character latched to the slot-1 data port");
            Assert::AreEqual  ((Byte) 0xC1, cpu.RegA (),       L"A (the character) preserved for COUT");
            Assert::AreEqual  ((Byte) 0x10, cpu.RegX (),       L"slot discovered as 1 -> index slot*16");
        }


        TEST_METHOD (OutputInjectsLineFeedAfterCarriageReturn)
        {
            // US6: BASIC / DOS listings send bare CRs; the card adds the LF.
            TestCpu  cpu;
            int      steps = RunOutput (cpu, 0x8D);   // CR (high bit)

            Assert::IsTrue   (steps < 400, L"the CR + LF path must terminate");
            Assert::AreEqual ((Byte) 0x0A, cpu.Peek (0xC090), L"LF written to the data port after the CR");
            Assert::AreEqual ((Byte) 0x8D, cpu.RegA (),       L"the CR character is preserved for COUT");
        }


        TEST_METHOD (OutputPrintableCharDoesNotInjectLineFeed)
        {
            // A non-CR character latches once and returns -- no stray LF.
            TestCpu  cpu;
            RunOutput (cpu, 0xD9);   // 'Y' | high bit

            Assert::AreEqual ((Byte) 0xD9, cpu.Peek (0xC090), L"the character (not an LF) is the last byte latched");
        }
    };
}
