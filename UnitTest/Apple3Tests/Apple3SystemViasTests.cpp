#include "Pch.h"

#include "Apple3TestFixture.h"
#include "Machines/Apple3/Apple3SystemVias.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SystemViasTests
//
//  The two system VIAs through the bus: their registers at $FFD0-$FFEF, the
//  environment, zero page and bank registers they drive into the memory
//  map, the Apple II lockout, the pull-ups after a reset, and the slot
//  interrupt inputs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SystemViasTests)
{
public:

    static constexpr Word  kDOrb  = 0xFFD0;
    static constexpr Word  kDDdrb = 0xFFD2;
    static constexpr Word  kDDdra = 0xFFD3;
    static constexpr Word  kDIfr  = 0xFFDD;
    static constexpr Word  kDOra  = 0xFFDF;
    static constexpr Word  kEDdra = 0xFFE3;
    static constexpr Word  kEOra  = 0xFFEF;


    struct ViaFixture : Apple3TestFixture
    {
        Apple3SystemVias  systemVias;

        ViaFixture()
            : systemVias (mmu)
        {
            mmu.AttachVias (&systemVias);
        }
    };


    TEST_METHOD (AfterReset_PullUpsSelectTheRom)
    {
        ViaFixture  f;



        Assert::AreEqual ((Byte) 0xFF, f.mmu.GetEnvironment());
        Assert::AreEqual ((Byte) 0x01, f.bus.ReadByte (0xF000));
        Assert::IsFalse  (f.mmu.IsLockedOut());
    }


    TEST_METHOD (DPortA_IsTheEnvironmentRegister)
    {
        ViaFixture  f;



        f.bus.WriteByte (kDDdra, 0xFF);
        f.bus.WriteByte (kDOra,  0x74);

        Assert::AreEqual ((Byte) 0x74, f.mmu.GetEnvironment());
        Assert::AreEqual ((Byte) 0x74, f.bus.ReadByte (kDOra));
    }


    TEST_METHOD (DPortB_IsTheZeroPageRegister)
    {
        ViaFixture  f;



        f.bus.WriteByte (kDDdrb, 0xFF);
        f.bus.WriteByte (kDOrb,  0x1A);
        f.bus.WriteByte (0x0010, 0x77);

        Assert::AreEqual ((Byte) 0x1A, f.mmu.GetZeroPage());
        Assert::AreEqual ((Byte) 0x77, f.mmu.PeekSystem (0x1A10));
    }


    TEST_METHOD (EPortA_IsTheBankRegister)
    {
        ViaFixture  f;



        f.bus.WriteByte (kEDdra, 0x0F);
        f.bus.WriteByte (kEOra,  0x02);
        f.bus.WriteByte (0x2000, 0x22);

        Assert::AreEqual ((Byte) 0x02, f.mmu.GetBank());
        Assert::AreEqual ((Byte) 0x22, f.mmu.PeekPhysical (2, 0x0000));
    }


    TEST_METHOD (EPortABit6DrivenLow_LocksOutTheRegisters)
    {
        ViaFixture  f;



        f.bus.WriteByte (kDDdra, 0xFF);
        f.bus.WriteByte (kDOra,  0x74);
        f.bus.WriteByte (kEOra,  0x41);
        f.bus.WriteByte (kEDdra, 0x4F);
        Assert::IsFalse (f.mmu.IsLockedOut(), L"bit 6 driven high");

        f.bus.WriteByte (kEOra,  0x01);
        Assert::IsTrue (f.mmu.IsLockedOut());

        f.bus.WriteByte (kEOra, 0x02);
        Assert::AreEqual ((Byte) 0x01, f.mmu.GetBank(), L"the VIA is no longer at $FFEF");
        Assert::AreEqual ((Byte) 0x02, f.mmu.PeekSystem (kEOra), L"the write went to RAM");

        f.systemVias.Reset();
        Assert::IsFalse (f.mmu.IsLockedOut(), L"a reset ends the lockout");
    }


    TEST_METHOD (SlotInterrupts_SetTheirInputsAndTheAnySlotLine)
    {
        ViaFixture  f;



        f.systemVias.SetSlotInterrupt (4, true);
        Assert::AreEqual ((Byte) 0x00, (Byte) (f.systemVias.GetE().GetPortA() & Apple3SystemVias::kSlot4Irq));
        Assert::AreEqual ((Byte) Apple3SystemVias::kSlot3Irq, (Byte) (f.systemVias.GetE().GetPortA() & Apple3SystemVias::kSlot3Irq));
        Assert::AreEqual ((Byte) Via6522::kIrqCa1, (Byte) (f.bus.ReadByte (kDIfr) & Via6522::kIrqCa1), L"falling edge on the D VIA's CA1");
        Assert::IsFalse  (f.systemVias.GetD().GetCa1());

        f.systemVias.SetSlotInterrupt (1, true);
        f.systemVias.SetSlotInterrupt (4, false);
        Assert::IsFalse (f.systemVias.GetD().GetCa1(), L"slot 1 still requests");

        f.systemVias.SetSlotInterrupt (1, false);
        Assert::IsTrue (f.systemVias.GetD().GetCa1());
    }
};
