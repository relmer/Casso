#include "Pch.h"

#include "Apple3TestFixture.h"
#include "Machines/Apple3/Apple3Slots.h"
#include "Machines/Apple3/Apple3SystemVias.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SlotsTests
//
//  The four slots: an empty slot's floating bus, the shared $C800 space
//  selected by a slot's memory space and cleared by $C020 and $CFFF, and
//  the interrupt requests' route to the VIAs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SlotsTests)
{
public:

    struct SlotFixture : Apple3TestFixture
    {
        Apple3SystemVias  systemVias;
        Apple3Slots       slots;
        RecordingDevice   card2;
        RecordingDevice   card4;

        SlotFixture()
            : systemVias (mmu),
              slots      (bus, systemVias)
        {
        }
    };


    TEST_METHOD (EmptySlots_ReadTheFloatingBus)
    {
        SlotFixture  f;



        for (Word address : { (Word) 0xC090, (Word) 0xC0CF, (Word) 0xC100, (Word) 0xC4FF, (Word) 0xC800 })
        {
            Assert::AreEqual (f.bus.GetFloatingBusValue(), f.slots.Read (address));
        }
    }


    TEST_METHOD (SlotIoAndMemorySpace_GoToTheCardInThatSlot)
    {
        SlotFixture  f;



        f.slots.SetCard (2, &f.card2);
        f.slots.SetCard (4, &f.card4);

        f.slots.Write (0xC0A3, 0x11);
        f.slots.Write (0xC0C0, 0x44);

        Assert::AreEqual ((Word) 0xC0A3, f.card2.m_lastWrite);
        Assert::AreEqual ((Word) 0xC0C0, f.card4.m_lastWrite);
        Assert::AreEqual (RecordingDevice::kReadValue, f.slots.Read (0xC210));
        Assert::AreEqual ((Word) 0xC210, f.card2.m_lastRead);
    }


    TEST_METHOD (SharedSpace_FollowsTheLastSlotSelected)
    {
        SlotFixture  f;



        f.slots.SetCard (2, &f.card2);
        f.slots.SetCard (4, &f.card4);

        f.slots.Read (0xC400);
        f.slots.Read (0xC800);
        Assert::AreEqual (4, f.slots.GetSelectedSlot());
        Assert::AreEqual ((Word) 0xC800, f.card4.m_lastRead);

        f.slots.Read (0xC200);
        f.slots.Write (0xC900, 0x29);
        Assert::AreEqual ((Word) 0xC900, f.card2.m_lastWrite);

        f.slots.Read (0xC020);
        Assert::AreEqual (0, f.slots.GetSelectedSlot());

        f.slots.Read (0xC400);
        f.slots.Read (0xCFFF);
        Assert::AreEqual (0, f.slots.GetSelectedSlot());
        Assert::AreEqual ((Word) 0xC400, f.card4.m_lastRead, L"the $CFFF access goes to no card");
    }


    TEST_METHOD (Interrupts_SetTheViaInputs)
    {
        SlotFixture  f;



        f.slots.SetInterrupt (3, true);
        Assert::IsTrue  (f.slots.IsRequesting (3));
        Assert::AreEqual ((Byte) 0, (Byte) (f.systemVias.GetE().GetPortA() & Apple3SystemVias::kSlot3Irq));
        Assert::IsFalse (f.systemVias.GetD().GetCa1());

        f.slots.SetInterrupt (3, false);
        Assert::IsFalse (f.slots.IsRequesting (3));
        Assert::IsTrue  (f.systemVias.GetD().GetCa1());
    }
};
