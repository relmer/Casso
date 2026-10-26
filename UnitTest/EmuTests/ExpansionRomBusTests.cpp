#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/ExpansionRomBus.h"
#include "Machines/Apple2/Common/SlotRomPage.h"
#include "FakeExpansionRomCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace ExpansionRomBusTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  ExpansionRomBusTests
    //
    //  Apple's slot-ROM protocol on the ][ and ][+ bus: an access to
    //  a card's $Cs00 page selects its expansion ROM, any access to
    //  $CF00-$CFFF deselects every card, and $C800-$CEFF reads the selected
    //  card or the floating bus.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExpansionRomBusTests)
    {
    public:
        TEST_METHOD (SlotRomPageCoversTheSlotsPage)
        {
            FakeExpansionRomCard  card (0x11);
            SlotRomPage           page (2, card);



            Assert::AreEqual<Word> (0xC200, page.GetStart());
            Assert::AreEqual<Word> (0xC2FF, page.GetEnd());
        }


        TEST_METHOD (NothingSelectedReadsTheFloatingBus)
        {
            MemoryBus             bus;
            ExpansionRomBus       expansion (bus);
            FakeExpansionRomCard  card (0x11);



            expansion.AddCard (&card);

            Assert::AreEqual (bus.GetFloatingBusValue(), expansion.Read (0xC800));
            Assert::AreEqual<Word> (0, card.m_lastRomAddress, L"an unselected card's ROM is not read");
        }


        TEST_METHOD (PageReadSelectsAndExpansionReadsTheCard)
        {
            MemoryBus             bus;
            ExpansionRomBus       expansion (bus);
            FakeExpansionRomCard  card (0x11);
            SlotRomPage           page (2, card);



            expansion.AddCard (&card);

            Assert::AreEqual<Byte> (0x99, page.Read (0xC205));
            Assert::AreEqual<Byte> (0x11, expansion.Read (0xC800));
            Assert::AreEqual<Byte> (0x11, expansion.Read (0xCEFF));
            Assert::AreEqual<Word> (0xCEFF, card.m_lastRomAddress);
        }


        TEST_METHOD (PageWriteSelectsToo)
        {
            MemoryBus             bus;
            ExpansionRomBus       expansion (bus);
            FakeExpansionRomCard  card (0x11);
            SlotRomPage           page (2, card);



            expansion.AddCard (&card);
            page.Write (0xC200, 0x00);

            Assert::IsTrue (card.m_selected);
        }


        TEST_METHOD (AnyCfxxAccessDeselects)
        {
            const Word            addresses[] = { 0xCF00, 0xCF80, 0xCFFF };
            MemoryBus             bus;
            ExpansionRomBus       expansion (bus);
            FakeExpansionRomCard  card (0x11);
            SlotRomPage           page (2, card);



            expansion.AddCard (&card);

            for (Word address : addresses)
            {
                (void) page.Read (0xC200);
                (void) expansion.Read (address);
                Assert::IsFalse (card.m_selected, format (L"a read of ${:04X} deselects", address).c_str());

                (void) page.Read (0xC200);
                expansion.Write (address, 0);
                Assert::IsFalse (card.m_selected, format (L"a write of ${:04X} deselects", address).c_str());
            }

            Assert::AreEqual (bus.GetFloatingBusValue(), expansion.Read (0xC800));
        }


        TEST_METHOD (TwoSelectedCardsAreAnded)
        {
            MemoryBus             bus;
            ExpansionRomBus       expansion (bus);
            FakeExpansionRomCard  first (0xF0);
            FakeExpansionRomCard  second (0x3C);



            expansion.AddCard (&first);
            expansion.AddCard (&second);
            first.m_selected  = true;
            second.m_selected = true;

            Assert::AreEqual<Byte> (0x30, expansion.Read (0xC900));
        }
    };
}
