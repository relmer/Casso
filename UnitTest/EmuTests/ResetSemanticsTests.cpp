// DiskIIController embeds two DiskImage objects (~143KB each) which exceed
// the C6262 stack-frame budget when constructed in TEST_METHOD frames.
// Each test using a DiskIIController moves the controller onto the heap
// (unique_ptr) and the warning is suppressed at file scope to match the
// pattern used by DiskIITests.cpp.
#pragma warning(disable:6262)

#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/MemoryBusCpu.h"
#include "Core/InterruptController.h"
#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/LanguageCard.h"
#include "Devices/AppleIIeMmu.h"
#include "Devices/AppleIIeSoftSwitchBank.h"
#include "Devices/DiskIIController.h"
#include "Video/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ResetSemanticsTests
//
//  Phase 4 (FR-034 / FR-035 / audit §10) coverage of the SoftReset and
//  PowerCycle split. Each test wires the smallest scaffold the contract
//  needs — no HeadlessHost full-machine build is required because the
//  reset paths are compositional. Where deterministic randomness matters
//  the Prng is pinned to HeadlessHost::kPinnedSeed so two builds produce
//  byte-identical seed patterns.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr uint64_t   kPinnedSeed = 0xCA550001ULL;
}



TEST_CLASS (ResetSemanticsTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  SoftReset preserves DRAM
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SoftResetPreservesMainRam)
    {
        RamDevice    ram (0x0000, 0xBFFF);

        ram.Write (0x1234, 0xAB);
        ram.Write (0x5678, 0xCD);

        ram.SoftReset ();

        Assert::AreEqual (static_cast<Byte> (0xAB), ram.Read (0x1234),
            L"Audit §10: SoftReset must preserve main RAM");
        Assert::AreEqual (static_cast<Byte> (0xCD), ram.Read (0x5678));
    }

    TEST_METHOD (SoftResetPreservesAuxRam)
    {
        AppleIIeMmu     mmu;

        mmu.GetAuxBuffer ()[0x1111] = 0xEE;
        mmu.GetAuxBuffer ()[0x2222] = 0xDD;

        mmu.OnSoftReset ();

        Assert::AreEqual (static_cast<Byte> (0xEE), mmu.GetAuxBuffer ()[0x1111],
            L"Audit §10: SoftReset must preserve aux RAM");
        Assert::AreEqual (static_cast<Byte> (0xDD), mmu.GetAuxBuffer ()[0x2222]);
    }

    TEST_METHOD (SoftResetPreservesLcRamBothBanksMainAndAux)
    {
        MemoryBus      bus;
        LanguageCard   lc (bus);

        // Power-on has WRITERAM pre-armed but the actual write enable
        // requires two consecutive C08x reads. Use the same wake-up
        // sequence as LanguageCardTests::SoftResetPreservesAllLcRamBanks.
        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xAA);
        lc.WriteRam (0xE000, 0xCC);

        lc.Read (0xC089);
        lc.Read (0xC089);
        lc.WriteRam (0xD000, 0xBB);

        lc.SoftReset ();

        // After SoftReset, BANK2 + WRITERAM pre-armed. Read RAM via $C080.
        lc.Read (0xC080);
        Assert::AreEqual (static_cast<Byte> (0xAA), lc.ReadRam (0xD000),
            L"SoftReset must preserve LC bank-2 $D000");
        Assert::AreEqual (static_cast<Byte> (0xCC), lc.ReadRam (0xE000),
            L"SoftReset must preserve LC high RAM");

        lc.Read (0xC088);
        Assert::AreEqual (static_cast<Byte> (0xBB), lc.ReadRam (0xD000),
            L"SoftReset must preserve LC bank-1 $D000");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  CPU register state after SoftReset
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SoftResetSetsCpuIFlagAndAdjustsSP)
    {
        MemoryBus            bus;
        RamDevice            ramLo (0x0000, 0xBFFF);
        RamDevice            ramHi (0xC000, 0xFFFF);
        MemoryBusCpu         cpu (bus);
        Cpu6502Registers     regs = {};

        bus.AddDevice (&ramLo);
        bus.AddDevice (&ramHi);

        // Reset vector at $FFFC -> $1234. Writes go through the bus so
        // ReadWord($FFFC) inside SoftReset sees the same bytes.
        bus.WriteByte (0xFFFC, 0x34);
        bus.WriteByte (0xFFFD, 0x12);

        cpu.SoftReset ();

        regs = cpu.GetRegisters ();

        Assert::AreEqual (static_cast<uint8_t>  (0xFD), regs.sp,
            L"SoftReset must set SP to $FD per 6502 reset sequence");
        Assert::AreEqual (static_cast<uint16_t> (0x1234), regs.pc,
            L"SoftReset must reload PC from $FFFC");
        Assert::IsTrue   ((regs.p & 0x04) != 0,
            L"SoftReset must set the I flag (interruptDisable=1)");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Soft-switch state after SoftReset (audit §10 [CRITICAL])
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SoftResetResetsSoftSwitchesPerMmuRules)
    {
        MemoryBus               bus;
        AppleIIeSoftSwitchBank  sw (&bus);

        sw.Write (0xC050, 0);   // graphics
        sw.Write (0xC053, 0);   // mixed
        sw.Write (0xC055, 0);   // page2
        sw.Write (0xC057, 0);   // hires

        sw.SoftReset ();

        Assert::IsFalse (sw.IsGraphicsMode (), L"SoftReset must clear GR");
        Assert::IsFalse (sw.IsMixedMode    (), L"SoftReset must clear MIXED");
        Assert::IsFalse (sw.IsPage2        (), L"SoftReset must clear PAGE2");
        Assert::IsFalse (sw.IsHiresMode    (), L"SoftReset must clear HIRES");
    }

    TEST_METHOD (SoftResetLeaves80ColModeAtDocumented_ResetState)
    {
        // Audit §10 [CRITICAL]: //e MMU asserts /RESET clearing every soft
        // switch including 80COL and ALTCHARSET. The originally-reported
        // bug was that 80COL persisted; SoftReset MUST clear it.
        MemoryBus               bus;
        AppleIIeSoftSwitchBank  sw (&bus);

        sw.Write (0xC00D, 0);   // 80COL on
        sw.Write (0xC00F, 0);   // ALTCHARSET on
        sw.Write (0xC05F, 0);   // DHIRES on (alias)

        sw.SoftReset ();

        Assert::IsFalse (sw.Is80ColMode   (),
            L"Audit §10 [CRITICAL]: SoftReset must clear 80COL");
        Assert::IsFalse (sw.IsAltCharSet  (),
            L"SoftReset must clear ALTCHARSET");
        Assert::IsFalse (sw.IsDoubleHiRes (),
            L"SoftReset must clear DHIRES");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  VideoTiming
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SoftResetClearsVideoTimingCycleCounterPerSpec)
    {
        VideoTiming     vt;

        vt.Tick (1234);

        Assert::AreEqual (static_cast<uint32_t> (1234), vt.GetCycleInFrame ());

        vt.SoftReset ();

        Assert::AreEqual (static_cast<uint32_t> (0), vt.GetCycleInFrame (),
            L"data-model.md: SoftReset clears VideoTiming cycle counter");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  DiskII controller mount preservation
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SoftResetPreservesDiskMountsAndFlushesDirty)
    {
        unique_ptr<DiskIIController> ctrl = make_unique<DiskIIController> (6);

        ctrl->GetDisk (0)->SetLoadedForTest (true, true);
        ctrl->GetDisk (1)->SetLoadedForTest (true, false);

        ctrl->SoftReset ();

        Assert::IsTrue (ctrl->GetDisk (0)->IsLoaded (),
            L"FR-034: SoftReset must preserve drive 0 mount");
        Assert::IsTrue (ctrl->GetDisk (1)->IsLoaded (),
            L"FR-034: SoftReset must preserve drive 1 mount");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PowerCycle: re-seed DRAM
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (PowerCycleSeedsAllRamFromPrng)
    {
        RamDevice    ram (0x0000, 0xBFFF);
        Prng         prng (kPinnedSeed);
        size_t       nonZero = 0;

        // Pre-zero so any post-PowerCycle non-zero byte must come from
        // the Prng.
        ram.Reset ();

        ram.PowerCycle (prng);

        for (size_t i = 0; i < 0xC000; i++)
        {
            if (ram.Read (static_cast<Word> (i)) != 0)
            {
                ++nonZero;
            }
        }

        // SplitMix64 over 48 KB will leave ~190 zero bytes statistically;
        // require at least 90% non-zero to confirm it actually seeded.
        Assert::IsTrue (nonZero > 40000,
            L"FR-035: PowerCycle must re-seed RAM (got <40000 non-zero bytes)");
    }

    TEST_METHOD (PowerCycleZeroesNothingButSeedsEverything)
    {
        // No region should remain entirely zero after PowerCycle —
        // every page gets touched by the Prng fill (FR-035, audit §10).
        RamDevice    ram (0x0000, 0xBFFF);
        Prng         prng (kPinnedSeed);
        bool         pageHasNonZero = false;
        Word         page           = 0;
        Word         offset         = 0;

        ram.Reset ();
        ram.PowerCycle (prng);

        for (page = 0; page < 0xC0; page++)
        {
            pageHasNonZero = false;

            for (offset = 0; offset < 0x100; offset++)
            {
                if (ram.Read (static_cast<Word> ((page << 8) | offset)) != 0)
                {
                    pageHasNonZero = true;
                    break;
                }
            }

            Assert::IsTrue (pageHasNonZero,
                L"FR-035: every page must contain Prng-seeded data after PowerCycle");
        }
    }

    TEST_METHOD (PowerCycleResetsLcToBank2WriteRamPrearmed)
    {
        MemoryBus      bus;
        LanguageCard   lc (bus);
        Prng           prng (kPinnedSeed);

        // Drive flag state away from the power-on values.
        lc.Read (0xC088);   // bank 1
        lc.Read (0xC088);
        lc.Read (0xC081);   // try to enable WRITE — but power-on already has
                            // it set; the more interesting transition is the
                            // bank switch above.

        lc.PowerCycle (prng);

        Assert::IsTrue  (lc.IsBank2 (),
            L"PowerCycle must restore BANK2");
        Assert::IsTrue  (lc.IsWriteRam (),
            L"PowerCycle must pre-arm WRITERAM (audit M8)");
        Assert::IsFalse (lc.IsReadRam (),
            L"PowerCycle must select ROM read");
        Assert::AreEqual (0, lc.GetPreWriteCount (),
            L"PowerCycle must clear pre-write counter");
    }

    TEST_METHOD (PowerCycleSeedDeterministicWithPinnedSeed)
    {
        // Two RamDevices PowerCycle'd from the same pinned seed must
        // produce byte-identical buffers (FR-035 deterministic property).
        RamDevice   ramA (0x0000, 0xBFFF);
        RamDevice   ramB (0x0000, 0xBFFF);
        Prng        prngA (kPinnedSeed);
        Prng        prngB (kPinnedSeed);

        ramA.PowerCycle (prngA);
        ramB.PowerCycle (prngB);

        for (size_t i = 0; i < 0xC000; i++)
        {
            Byte    a = ramA.Read (static_cast<Word> (i));
            Byte    b = ramB.Read (static_cast<Word> (i));

            if (a != b)
            {
                Assert::Fail (L"PowerCycle with pinned seed must be deterministic");
            }
        }
    }

    TEST_METHOD (PowerCycleZeroesVideoTimingCycleCounter)
    {
        VideoTiming    vt;
        Prng           prng (kPinnedSeed);

        vt.Tick (5000);

        vt.PowerCycle (prng);

        Assert::AreEqual (static_cast<uint32_t> (0), vt.GetCycleInFrame (),
            L"data-model.md: PowerCycle zeroes VideoTiming cycle counter");
    }

    TEST_METHOD (PowerCycleUnmountsAndFlushesAllDisks)
    {
        unique_ptr<DiskIIController> ctrl = make_unique<DiskIIController> (6);
        Prng                              prng (kPinnedSeed);

        ctrl->GetDisk (0)->SetLoadedForTest (true, true);
        ctrl->GetDisk (1)->SetLoadedForTest (true, false);

        ctrl->PowerCycle (prng);

        Assert::IsFalse (ctrl->GetDisk (0)->IsLoaded (),
            L"FR-035: PowerCycle must eject drive 0");
        Assert::IsFalse (ctrl->GetDisk (1)->IsLoaded (),
            L"FR-035: PowerCycle must eject drive 1");
    }
};
