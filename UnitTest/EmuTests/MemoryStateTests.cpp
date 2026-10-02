#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/LanguageCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryStateTests
//
//  Save and load of RAM, the //e MMU, the language card, the soft-switch banks
//  and the //c ROM bank. A rig wires the parts the way the //e builder does.
//  The round trips give the source a non-default value in every saved field
//  and the target a different one, load, and compare every field through the
//  parts' own accessors. The run tests save, run a fixed access script, load,
//  and check the same script reads the same bytes, which fails if a load
//  leaves the page tables pointing at the wrong bank.
//
////////////////////////////////////////////////////////////////////////////////

namespace MemoryState
{
    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);

    // Size of a section header: tag, version, payload size.
    static constexpr size_t   kHeaderSize  = sizeof (uint32_t) + sizeof (uint16_t) + sizeof (uint32_t);

    static constexpr Word     kMainRamEnd  = 0xBFFF;
    static constexpr uint64_t kSourceSeed  = 0x0123456789ABCDEFull;
    static constexpr uint64_t kTargetSeed  = 0xFEDCBA9876543210ull;
    static constexpr uint64_t kTriggerAt   = 0x0000000012345678ull;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryRig
    //
    //  Main RAM, the MMU with its aux RAM and Cxxx router, the language card
    //  and the //e soft-switch bank on one bus, wired to each other, with RAM
    //  seeded from a seed so two rigs differ in every byte.
    //
    ////////////////////////////////////////////////////////////////////////////

    struct MemoryRig
    {
        MemoryBus              bus;
        RamDevice              mainRam  { 0x0000, kMainRamEnd };
        Apple2eMmu             mmu;
        LanguageCard           lc       { bus };
        Apple2eSoftSwitchBank  ssw      { &bus };
        uint64_t               cycle    = 0;

        explicit MemoryRig (uint64_t seed)
        {
            Prng     prng (seed);
            HRESULT  hr   = S_OK;



            mainRam.PowerCycle (prng);
            lc.PowerCycle      (prng);
            mmu.OnPowerCycle   (prng);

            hr = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, &lc, &ssw);
            Assert::AreEqual (S_OK, hr);

            mmu.SetLanguageCard   (&lc);
            lc.SetMmu             (&mmu);
            ssw.SetMmu            (&mmu);
            ssw.SetLanguageCard   (&lc);
            ssw.SetCpuCycleSource (&cycle);
        }

        // Every saved field away from its power-on value.
        void SetEveryField()
        {
            static constexpr Word  kSetSwitches[] =
            {
                0xC050,     // graphics
                0xC053,     // mixed
                0xC055,     // page 2
                0xC057,     // hires
                0xC059,     // AN0 on
                0xC05D,     // AN2 on
                0xC00D,     // 80 columns
                0xC00F,     // alternate character set
                0xC05E,     // double hires
            };

            int  axis = 0;



            mmu.SetRamRd     (true);
            mmu.SetRamWrt    (true);
            mmu.SetAltZp     (true);
            mmu.Set80Store   (true);
            mmu.SetIntCxRom  (true);
            mmu.SetSlotC3Rom (true);
            mmu.SetIntC8Rom  (true);

            // Bank 1, read RAM, two odd reads arm and enable writes.
            lc.Read (0xC08B);
            lc.Read (0xC08B);

            for (Word address : kSetSwitches)
            {
                ssw.Read (address);
            }

            cycle = kTriggerAt;
            ssw.Read (0xC070);

            for (axis = 0; axis < 4; axis++)
            {
                ssw.SetPaddle (axis, static_cast<Byte> (0x10 + axis));
            }
        }

        // Every language-card RAM byte, main and aux, bank 1 and bank 2,
        // read through ReadRam. Leaves the switches changed.
        std::vector<Byte> ReadAllLcRam()
        {
            static constexpr Word  kReadBank[] = { 0xC08B, 0xC083 };
            static constexpr bool  kAltZp[]    = { false, true };

            std::vector<Byte>  bytes;
            uint32_t           address = 0;



            for (bool altZp : kAltZp)
            {
                mmu.SetAltZp (altZp);

                for (Word readBank : kReadBank)
                {
                    lc.Read (readBank);

                    for (address = 0xD000; address <= 0xFFFF; address++)
                    {
                        bytes.push_back (lc.ReadRam (static_cast<Word> (address)));
                    }
                }
            }

            return bytes;
        }

        // The parts in save and load order: RAM, MMU, language card, soft
        // switches, or that list backward. Each part re-derives what it
        // shares with the others, so either order must give the same machine.
        std::vector<IMachineState *> GetParts (bool backward)
        {
            std::vector<IMachineState *>  parts = { &mainRam, &mmu, &lc, &ssw };



            if (backward)
            {
                std::reverse (parts.begin(), parts.end());
            }

            return parts;
        }

        std::vector<Byte> Save (bool backward = false)
        {
            StateWriter  writer;
            HRESULT      hr = S_OK;



            for (IMachineState * part : GetParts (backward))
            {
                hr = part->SaveState (writer);
                Assert::AreEqual (S_OK, hr);
            }

            return writer.GetBytes();
        }

        void Load (const std::vector<Byte> & bytes, bool backward = false)
        {
            StateReader  reader (bytes);
            HRESULT      hr = S_OK;



            for (IMachineState * part : GetParts (backward))
            {
                hr = part->LoadState (reader);
                Assert::AreEqual (S_OK, hr);
            }

            Assert::IsTrue (reader.IsAtEnd());
        }

        // A fixed access script through the bus page tables and the switches.
        // Returns every byte it read, then the state it ends in.
        std::vector<Byte> Run()
        {
            static constexpr Word  kProbes[] = { 0x0010, 0x0150, 0x0400, 0x0800, 0x2000, 0x4000, 0xBF00 };

            std::vector<Byte>  trace;
            std::vector<Byte>  after;
            Word               address = 0;



            for (Word probe : kProbes)
            {
                trace.push_back (bus.ReadByte (probe));
                bus.WriteByte   (probe, static_cast<Byte> (probe ^ 0x5A));
            }

            trace.push_back (lc.ReadRam (0xD000));
            trace.push_back (lc.ReadRam (0xE123));
            lc.WriteRam     (0xD000, 0x77);
            trace.push_back (bus.ReadByte (0xD000));

            mmu.SetAltZp  (false);
            mmu.SetRamRd  (false);
            ssw.Read      (0xC054);
            lc.Read       (0xC083);

            for (Word probe : kProbes)
            {
                trace.push_back (bus.ReadByte (probe));
            }

            trace.push_back (lc.ReadRam (0xD000));
            trace.push_back (bus.ReadByte (0xD000));

            cycle += 0x200;

            for (address = 0xC064; address <= 0xC067; address++)
            {
                trace.push_back (ssw.Read (address));
            }

            after = Save();
            trace.insert (trace.end(), after.begin(), after.end());

            return trace;
        }
    };


    static void AssertSameState (MemoryRig & expected, MemoryRig & actual)
    {
        int  mainDiff = memcmp (expected.mainRam.GetData(), actual.mainRam.GetData(), kMainRamEnd + 1);
        int  auxDiff  = memcmp (expected.mmu.GetAuxBuffer(), actual.mmu.GetAuxBuffer(), 0x10000);
        int  axis     = 0;
        int  index    = 0;



        Assert::AreEqual (0, mainDiff, L"main RAM");

        Assert::AreEqual (expected.mmu.GetRamRd(),     actual.mmu.GetRamRd(),     L"RAMRD");
        Assert::AreEqual (expected.mmu.GetRamWrt(),    actual.mmu.GetRamWrt(),    L"RAMWRT");
        Assert::AreEqual (expected.mmu.GetAltZp(),     actual.mmu.GetAltZp(),     L"ALTZP");
        Assert::AreEqual (expected.mmu.Get80Store(),   actual.mmu.Get80Store(),   L"80STORE");
        Assert::AreEqual (expected.mmu.GetIntCxRom(),  actual.mmu.GetIntCxRom(),  L"INTCXROM");
        Assert::AreEqual (expected.mmu.GetSlotC3Rom(), actual.mmu.GetSlotC3Rom(), L"SLOTC3ROM");
        Assert::AreEqual (expected.mmu.GetIntC8Rom(),  actual.mmu.GetIntC8Rom(),  L"INTC8ROM");
        Assert::AreEqual (0,                           auxDiff,                   L"aux RAM");

        Assert::AreEqual (expected.lc.IsReadRam(),        actual.lc.IsReadRam(),        L"LC read RAM");
        Assert::AreEqual (expected.lc.IsWriteRam(),       actual.lc.IsWriteRam(),       L"LC write RAM");
        Assert::AreEqual (expected.lc.IsBank2(),          actual.lc.IsBank2(),          L"LC bank 2");
        Assert::AreEqual (expected.lc.GetPreWriteCount(), actual.lc.GetPreWriteCount(), L"LC pre-write count");

        Assert::AreEqual (expected.ssw.IsGraphicsMode(),        actual.ssw.IsGraphicsMode(),        L"graphics");
        Assert::AreEqual (expected.ssw.IsMixedMode(),           actual.ssw.IsMixedMode(),           L"mixed");
        Assert::AreEqual (expected.ssw.IsPage2(),               actual.ssw.IsPage2(),               L"page 2");
        Assert::AreEqual (expected.ssw.IsHiresMode(),           actual.ssw.IsHiresMode(),           L"hires");
        Assert::AreEqual (expected.ssw.Is80ColMode(),           actual.ssw.Is80ColMode(),           L"80 columns");
        Assert::AreEqual (expected.ssw.IsDoubleHiRes(),         actual.ssw.IsDoubleHiRes(),         L"double hires");
        Assert::AreEqual (expected.ssw.IsAltCharSet(),          actual.ssw.IsAltCharSet(),          L"alternate character set");
        Assert::AreEqual (expected.ssw.GetPaddleTriggerCycle(), actual.ssw.GetPaddleTriggerCycle(), L"paddle trigger cycle");

        for (index = 0; index < AppleSoftSwitchBank::kAnnunciatorCount; index++)
        {
            Assert::AreEqual (expected.ssw.IsAnnunciatorOn (index), actual.ssw.IsAnnunciatorOn (index), L"annunciator");
        }

        for (axis = 0; axis < 4; axis++)
        {
            Assert::AreEqual (expected.ssw.GetPaddle (axis), actual.ssw.GetPaddle (axis), L"paddle position");
        }

        // Last, since reading every bank flips the switches.
        Assert::IsTrue (expected.ReadAllLcRam() == actual.ReadAllLcRam(), L"LC RAM");
    }


    static std::vector<Byte> SaveOne (const IMachineState & part)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = part.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    static HRESULT LoadOne (IMachineState & part, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);



        return part.LoadState (reader);
    }


    // A 16K $C000-$FFFF image whose $C100 and $D000 bytes are both marker.
    static std::vector<Byte> MakeRomBank (Byte marker)
    {
        std::vector<Byte>  image (0x4000, 0x00);



        image[0x0100] = marker;
        image[0x1000] = marker;

        return image;
    }


    TEST_CLASS (MemoryStateTests)
    {
    public:
        TEST_METHOD (RoundTripsEveryField)
        {
            MemoryRig          source (kSourceSeed);
            MemoryRig          target (kTargetSeed);
            std::vector<Byte>  bytes;



            source.SetEveryField();
            bytes = source.Save();

            target.Load (bytes);

            AssertSameState (source, target);
        }


        TEST_METHOD (RunAfterLoadMatchesRunAfterSave)
        {
            MemoryRig          source (kSourceSeed);
            MemoryRig          fresh  (kTargetSeed);
            std::vector<Byte>  bytes;
            std::vector<Byte>  first;
            std::vector<Byte>  again;
            std::vector<Byte>  elsewhere;



            source.SetEveryField();
            bytes = source.Save();
            first = source.Run();

            source.Load (bytes);
            again = source.Run();

            fresh.Load (bytes);
            elsewhere = fresh.Run();

            Assert::IsTrue (first == again,     L"same rig after load");
            Assert::IsTrue (first == elsewhere, L"fresh rig after load");
        }


        TEST_METHOD (RunAfterBackwardLoadMatchesRunAfterSave)
        {
            MemoryRig          source (kSourceSeed);
            MemoryRig          fresh  (kTargetSeed);
            std::vector<Byte>  bytes;
            std::vector<Byte>  first;
            std::vector<Byte>  elsewhere;



            source.SetEveryField();
            bytes = source.Save (true);
            first = source.Run();

            fresh.Load (bytes, true);
            elsewhere = fresh.Run();

            Assert::IsTrue (first == elsewhere, L"fresh rig after backward load");
        }


        TEST_METHOD (RamOtherRangeFails)
        {
            RamDevice          saved  (0x0000, 0xBFFF);
            RamDevice          loaded (0x0000, 0x7FFF);
            std::vector<Byte>  bytes  = SaveOne (saved);
            HRESULT            hr     = S_OK;



            hr = LoadOne (loaded, bytes);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (SoftSwitchBadAnnunciatorFails)
        {
            AppleSoftSwitchBank  ssw;
            std::vector<Byte>    bytes = SaveOne (ssw);
            HRESULT              hr    = S_OK;



            bytes.back() = 1 << AppleSoftSwitchBank::kAnnunciatorCount;

            hr = LoadOne (ssw, bytes);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (LanguageCardBadFlagsFail)
        {
            MemoryBus          bus;
            LanguageCard       lc     (bus);
            std::vector<Byte>  bytes  = SaveOne (lc);
            HRESULT            hr     = S_OK;



            bytes[kHeaderSize] = 0x08;

            hr = LoadOne (lc, bytes);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (LanguageCardBadPreWriteCountFails)
        {
            MemoryBus          bus;
            LanguageCard       lc     (bus);
            std::vector<Byte>  bytes  = SaveOne (lc);
            HRESULT            hr     = S_OK;



            bytes[kHeaderSize + sizeof (Word)] = 3;

            hr = LoadOne (lc, bytes);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (RomBankRoundTripsAndReapplies)
        {
            MemoryBus          sourceBus;
            Apple2eMmu         sourceMmu;
            LanguageCard       sourceLc   (sourceBus);
            Apple2cRomBank     source     (sourceLc, sourceMmu);
            MemoryBus          targetBus;
            Apple2eMmu         targetMmu;
            LanguageCard       targetLc   (targetBus);
            Apple2cRomBank     target     (targetLc, targetMmu);
            Byte               cxxx       = 0;
            HRESULT            hr         = S_OK;



            source.SetBankImages (MakeRomBank (0xA0), MakeRomBank (0xB1));
            target.SetBankImages (MakeRomBank (0xA0), MakeRomBank (0xB1));
            source.ToggleRomBank();

            hr = LoadOne (target, SaveOne (source));
            Assert::AreEqual (S_OK, hr);

            targetMmu.SetIntCxRom (true);
            Assert::IsTrue (targetMmu.GetCxxxRouter()->TryPeek (0xC100, cxxx));

            Assert::AreEqual<int>  (1,    target.GetCurrentBank());
            Assert::AreEqual<Byte> (0xB1, targetLc.ReadRom (0xD000));
            Assert::AreEqual<Byte> (0xB1, cxxx);
        }


        TEST_METHOD (RomBankPastSecondFails)
        {
            MemoryBus          bus;
            Apple2eMmu         mmu;
            LanguageCard       lc      (bus);
            Apple2cRomBank     romBank (lc, mmu);
            std::vector<Byte>  bytes   = SaveOne (romBank);
            HRESULT            hr      = S_OK;



            bytes.back() = 2;

            hr = LoadOne (romBank, bytes);
            Assert::AreEqual (kInvalidData, hr);
        }
    };
}
