#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Debugger/DebugMemoryView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryViewPatchTests
//
//  A memory window's edit (FR-037): RAM takes it as a write, ROM takes it into
//  the image the CPU reads from, and an I/O address refuses it.
//
//  THE CPU IS THE ORACLE. A patch is only a patch if the machine then reads the
//  new byte, so every ROM case checks a bus read as well as the debugger's peek.
//  Bus reads below $C000 and above $CFFF have no side effects, so they are safe
//  to use here; in $C100-$CFFF the peek is the oracle, as in the parity tests.
//
//  THE //C FLIPS ITS ROM BANK AT $C028, re-copying the selected 16K bank into
//  the $Cxxx router and the language card. A patch has to live in the bank's
//  own image or the next flip undoes it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugMemoryViewPatchTests)
    {
    public:

        static constexpr Word  kRomAddress      = 0xF800;
        static constexpr Word  kInternalCxxx    = 0xC300;
        static constexpr Word  kRamAddress      = 0x0300;
        static constexpr Word  kIoAddress       = 0xC030;
        static constexpr Word  kIntCxRomOn      = 0xC007;
        static constexpr Word  kRomBankToggle   = 0xC028;



        //  A value different from what is there, so a patch that did nothing
        //  cannot pass by coincidence.
        static Byte Different (DebugMemoryView & view, Word address)
        {
            Byte  current = 0;



            Assert::IsTrue (view.TryPeek (address, current));
            return (Byte) (current ^ 0xA5);
        }



        static void AssertRomPatchReadByCpu (const char * machineId)
        {
            TestMachine      machine (std::string (machineId), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);
            Byte             patched = Different (view, kRomAddress);
            Byte             peeked  = 0;
            std::wstring     where   = std::wstring (machineId, machineId + strlen (machineId));



            Assert::IsTrue   (view.GetRegion (kRomAddress) == MemoryRegion::Rom, where.c_str());
            Assert::IsTrue   (view.TryPatch  (kRomAddress, patched), where.c_str());
            Assert::IsTrue   (view.TryPeek   (kRomAddress, peeked));
            Assert::AreEqual (patched, peeked, where.c_str());
            Assert::AreEqual (patched, machine.GetMemoryBus().ReadByte (kRomAddress), L"the CPU reads the patch");
        }



        TEST_METHOD (Rom_PatchIsWhatTheCpuReads_EveryMachine)
        {
            for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
            {
                AssertRomPatchReadByCpu (id);
            }
        }


        TEST_METHOD (InternalCxxxRom_PatchIsWhatThePeekReads)
        {
            TestMachine      machine (std::string ("Apple2eEnhanced"), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);
            Byte             patched = 0;
            Byte             peeked  = 0;



            machine.GetMemoryBus().WriteByte (kIntCxRomOn, 0);
            patched = Different (view, kInternalCxxx);

            Assert::IsTrue   (view.GetRegion (kInternalCxxx) == MemoryRegion::Rom);
            Assert::IsTrue   (view.TryPatch  (kInternalCxxx, patched));
            Assert::IsTrue   (view.TryPeek   (kInternalCxxx, peeked));
            Assert::AreEqual (patched, peeked);
        }


        TEST_METHOD (Apple2c_PatchSurvivesARomBankFlip)
        {
            TestMachine      machine (std::string ("Apple2c"), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);
            Byte             romPatch  = Different (view, kRomAddress);
            Byte             cxxxPatch = Different (view, kInternalCxxx);
            Byte             peeked    = 0;



            Assert::IsTrue (view.TryPatch (kRomAddress,   romPatch));
            Assert::IsTrue (view.TryPatch (kInternalCxxx, cxxxPatch));

            //  Over to the other bank and back.
            (void) machine.GetMemoryBus().ReadByte (kRomBankToggle);
            (void) machine.GetMemoryBus().ReadByte (kRomBankToggle);

            Assert::AreEqual (romPatch, machine.GetMemoryBus().ReadByte (kRomAddress), L"the $F800 patch came back with its bank");
            Assert::IsTrue   (view.TryPeek (kInternalCxxx, peeked));
            Assert::AreEqual (cxxxPatch, peeked, L"so did the $C300 patch");
        }


        TEST_METHOD (Apple2c_APatchStaysInItsOwnBank)
        {
            TestMachine      machine (std::string ("Apple2c"), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);
            Byte             otherBank = 0;
            Byte             patch     = 0;



            (void) machine.GetMemoryBus().ReadByte (kRomBankToggle);
            otherBank = machine.GetMemoryBus().ReadByte (kRomAddress);
            (void) machine.GetMemoryBus().ReadByte (kRomBankToggle);

            patch = (Byte) (otherBank ^ 0x5A);
            Assert::IsTrue (view.TryPatch (kRomAddress, patch));

            (void) machine.GetMemoryBus().ReadByte (kRomBankToggle);
            Assert::AreEqual (otherBank, machine.GetMemoryBus().ReadByte (kRomAddress), L"the other bank is untouched");
        }


        TEST_METHOD (Ram_PatchIsAWrite)
        {
            TestMachine      machine (std::string ("Apple2e"), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);



            Assert::IsTrue   (view.TryPatch (kRamAddress, 0x5A));
            Assert::AreEqual ((Byte) 0x5A, machine.GetMemoryBus().ReadByte (kRamAddress));
        }


        TEST_METHOD (Io_IsRefused)
        {
            TestMachine      machine (std::string ("Apple2e"), TestMachine::Slots::Empty);
            DebugMemoryView  view    (machine);



            Assert::IsFalse (view.TryPatch (kIoAddress, 0x00), L"the speaker toggle is written by OUT, never by an edit");
        }
    };
}
