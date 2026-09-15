#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "EmuTests/FixtureProvider.h"
#include "Debugger/DebugMemoryView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryViewTests
//
//  The debugger's peek must agree with what the CPU would read under every
//  banking state, on every machine, and must never disturb the machine doing
//  it. Outside $C000-$CFFF a bus read has no side effects, so it is the oracle
//  there. In $C100-$CFFF a bus read latches the ROM router, so the oracle is
//  the ROM image the switches select, read from the fixture files.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugMemoryViewTests)
    {
    public:

        static constexpr const char * kMachines[] = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" };

        static constexpr Word  kIoFirst      = 0xC000;
        static constexpr Word  kSlotRomLast  = 0xCFFF;
        static constexpr int   kSampleStride = 0x0D;

        // //e soft switches, by effect.
        static constexpr Word  kRamRdOn      = 0xC003;
        static constexpr Word  kRamWrtOn     = 0xC005;
        static constexpr Word  kIntCxRomOff  = 0xC006;
        static constexpr Word  kIntCxRomOn   = 0xC007;
        static constexpr Word  kAltZpOn      = 0xC009;
        static constexpr Word  kIntC3RomOn   = 0xC00A;
        static constexpr Word  kStore80On    = 0xC001;
        static constexpr Word  kPage2On      = 0xC055;
        static constexpr Word  kLcBank2Ram   = 0xC080;
        static constexpr Word  kLcBank2RamWr = 0xC083;
        static constexpr Word  kLcRom        = 0xC082;
        static constexpr Word  kLcBank1Ram   = 0xC088;



        ////////////////////////////////////////////////////////////////////////
        //
        //  SweepParityOutsideCxxx
        //
        //  Every kSampleStride-th address outside $C000-$CFFF: the peek
        //  succeeds and equals a bus read. Returns the addresses checked.
        //
        ////////////////////////////////////////////////////////////////////////

        static size_t SweepParityOutsideCxxx (TestMachine & machine, const std::wstring & context)
        {
            DebugMemoryView  view (machine);
            Byte             peeked  = 0;
            size_t           checked = 0;



            for (int address = 0; address <= 0xFFFF; address += kSampleStride)
            {
                std::wstring where = std::format (L"{} ${:04X}", context, address);



                if (address >= kIoFirst && address <= kSlotRomLast)
                {
                    continue;
                }

                Assert::IsTrue   (view.TryPeek ((Word) address, peeked), where.c_str());
                Assert::AreEqual (machine.GetMemoryBus().ReadByte ((Word) address), peeked, where.c_str());
                ++checked;
            }

            return checked;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  LoadRom
        //
        ////////////////////////////////////////////////////////////////////////

        static std::vector<uint8_t> LoadRom (const std::string & name)
        {
            FixtureProvider      provider;
            std::vector<uint8_t> bytes;
            HRESULT              hr = S_OK;



            hr = provider.OpenFixture (name, bytes);
            Assert::AreEqual (S_OK, hr, std::wstring (name.begin(), name.end()).c_str());
            Assert::IsFalse  (bytes.empty());

            return bytes;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ExpectPage
        //
        //  The peeks across one $Cn00 page equal the image starting at offset.
        //
        ////////////////////////////////////////////////////////////////////////

        static void ExpectPage (TestMachine & machine, Word pageStart, const std::vector<uint8_t> & image, size_t offset, const wchar_t * context)
        {
            static constexpr int  kPageSize = 0x100;

            DebugMemoryView  view (machine);
            Byte             peeked = 0;



            Assert::IsTrue (offset + kPageSize <= image.size(), context);

            for (int i = 0; i < kPageSize; ++i)
            {
                Assert::IsTrue   (view.TryPeek ((Word) (pageStart + i), peeked), context);
                Assert::AreEqual ((Byte) image[offset + i], peeked, context);
            }
        }



        TEST_METHOD (Parity_OutsideCxxx_AllMachinesAndBankingStates)
        {
            size_t checked = 0;



            for (const char * id : kMachines)
            {
                TestMachine   machine (id, TestMachine::Slots::Empty);
                std::string   narrow  (id);
                std::wstring  name    (narrow.begin(), narrow.end());



                checked += SweepParityOutsideCxxx (machine, name + L" baseline");

                machine.GetMemoryBus().ReadByte (kLcBank2Ram);
                checked += SweepParityOutsideCxxx (machine, name + L" LC bank 2 RAM");

                machine.GetMemoryBus().ReadByte (kLcBank1Ram);
                checked += SweepParityOutsideCxxx (machine, name + L" LC bank 1 RAM");

                if (machine.GetMmu() == nullptr)
                {
                    continue;
                }

                machine.GetMemoryBus().WriteByte (kRamRdOn,  0);
                machine.GetMemoryBus().WriteByte (kRamWrtOn, 0);
                checked += SweepParityOutsideCxxx (machine, name + L" RAMRD RAMWRT");

                machine.GetMemoryBus().WriteByte (kAltZpOn, 0);
                checked += SweepParityOutsideCxxx (machine, name + L" ALTZP");

                machine.GetMemoryBus().WriteByte (kStore80On, 0);
                machine.GetMemoryBus().ReadByte  (kPage2On);
                checked += SweepParityOutsideCxxx (machine, name + L" 80STORE PAGE2");
            }

            Assert::IsTrue (checked > 0);
        }



        TEST_METHOD (Cxxx_FollowsRomSelection_Apple2e)
        {
            TestMachine           machine  ("Apple2e", TestMachine::Slots::DiskOnly);
            std::vector<uint8_t>  internal = LoadRom ("Apple2e.rom");
            std::vector<uint8_t>  disk2    = LoadRom ("Disk2.rom");



            machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);
            ExpectPage (machine, 0xC600, disk2, 0, L"INTCXROM off: slot 6 ROM");

            machine.GetMemoryBus().WriteByte (kIntCxRomOn, 0);
            ExpectPage (machine, 0xC600, internal, 0x600, L"INTCXROM on: internal $C600");
            ExpectPage (machine, 0xC100, internal, 0x100, L"INTCXROM on: internal $C100");

            machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);
            machine.GetMemoryBus().WriteByte (kIntC3RomOn,  0);
            ExpectPage (machine, 0xC300, internal, 0x300, L"SLOTC3ROM off: internal $C300");
        }



        TEST_METHOD (Cxxx_InternalFirmware_Apple2c)
        {
            TestMachine           machine  ("Apple2c");
            std::vector<uint8_t>  firmware = LoadRom ("Apple2c.rom");



            ExpectPage (machine, 0xC100, firmware, 0x100, L"//c $C100");
            ExpectPage (machine, 0xC800, firmware, 0x800, L"//c $C800");
        }



        TEST_METHOD (Peek_ChangesNoSwitches)
        {
            TestMachine       machine ("Apple2e", TestMachine::Slots::DiskOnly);
            DebugMemoryView   view (machine);
            Apple2eMmu    * mmu    = machine.GetMmu();
            LanguageCard  * lc     = machine.GetRefs().languageCard;
            Byte            peeked = 0;
            int             before = 0;



            Assert::IsNotNull (mmu);
            Assert::IsNotNull (lc);

            // One odd read arms the language card's pre-write counter; the
            // expansion ROM window is open, which a $CFFF read would close,
            // and $C3xx with SLOTC3ROM off would reopen it.
            machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);
            machine.GetMemoryBus().WriteByte (kIntC3RomOn,  0);
            machine.GetMemoryBus().ReadByte  (0xC081);
            mmu->SetIntC8Rom (true);

            before = lc->GetPreWriteCount();

            for (int address = 0; address <= 0xFFFF; ++address)
            {
                view.TryPeek ((Word) address, peeked);
            }

            Assert::IsTrue   (mmu->GetIntC8Rom());
            Assert::IsFalse  (mmu->GetIntCxRom());
            Assert::IsFalse  (mmu->GetRamRd());
            Assert::IsFalse  (mmu->GetAltZp());
            Assert::AreEqual (before, lc->GetPreWriteCount());

            mmu->SetIntC8Rom (false);

            for (int address = 0xC300; address <= 0xC3FF; ++address)
            {
                view.TryPeek ((Word) address, peeked);
            }

            Assert::IsFalse (mmu->GetIntC8Rom());
        }



        TEST_METHOD (Regions_Apple2e)
        {
            TestMachine      machine ("Apple2e", TestMachine::Slots::DiskOnly);
            DebugMemoryView  view (machine);



            Assert::AreEqual ((int) MemoryRegion::MainRam, (int) view.GetRegion (0x0300));
            Assert::AreEqual ((int) MemoryRegion::Io,      (int) view.GetRegion (0xC030));

            machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);
            Assert::AreEqual ((int) MemoryRegion::SlotRom, (int) view.GetRegion (0xC600));

            machine.GetMemoryBus().WriteByte (kIntCxRomOn, 0);
            Assert::AreEqual ((int) MemoryRegion::Rom,     (int) view.GetRegion (0xC600));

            machine.GetMemoryBus().ReadByte (kLcRom);
            Assert::AreEqual ((int) MemoryRegion::Rom,     (int) view.GetRegion (0xFFFC));

            machine.GetMemoryBus().ReadByte (kLcBank1Ram);
            Assert::AreEqual ((int) MemoryRegion::LcBank1, (int) view.GetRegion (0xD000));

            machine.GetMemoryBus().ReadByte (kLcBank2Ram);
            Assert::AreEqual ((int) MemoryRegion::LcBank2, (int) view.GetRegion (0xD000));

            machine.GetMemoryBus().WriteByte (kRamRdOn, 0);
            Assert::AreEqual ((int) MemoryRegion::AuxRam,  (int) view.GetRegion (0x0300));
        }



        TEST_METHOD (Poke_RamWritable_RomAndIoNot)
        {
            TestMachine      machine ("Apple2e", TestMachine::Slots::Empty);
            DebugMemoryView  view (machine);
            Byte             before = 0;
            Byte             after  = 0;



            Assert::IsTrue   (view.TryPoke (0x0300, 0xA9));
            Assert::AreEqual ((Byte) 0xA9, machine.GetMemoryBus().ReadByte (0x0300));

            machine.GetMemoryBus().ReadByte (kLcRom);
            view.TryPeek (0xFFFC, before);
            Assert::IsFalse  (view.TryPoke (0xFFFC, (Byte) ~before));
            view.TryPeek (0xFFFC, after);
            Assert::AreEqual (before, after);

            Assert::IsFalse  (view.TryPoke (0xC030, 0x00));
            Assert::IsFalse  (view.TryPoke (0xC600, 0x00));

            machine.GetMemoryBus().ReadByte (kLcBank2RamWr);
            Assert::IsTrue   (view.TryPoke (0xD000, 0x5A));
            Assert::AreEqual ((Byte) 0x5A, machine.GetMemoryBus().ReadByte (0xD000));
        }



        TEST_METHOD (Io_IsUnreadable)
        {
            TestMachine      machine ("Apple2e", TestMachine::Slots::Empty);
            DebugMemoryView  view (machine);
            Byte             peeked = 0;



            for (int address = 0xC000; address <= 0xC0FF; ++address)
            {
                Assert::IsFalse (view.TryPeek ((Word) address, peeked));
            }
        }
    };
}
