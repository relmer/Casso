#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleTextMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MmuTests
//
//  Per-region routing tests for Apple2eMmu. Each test wires:
//    - a MemoryBus
//    - a 64 KiB main RamDevice (the page table for $0000-$BFFF will
//      point at this buffer until the MMU rebinds it)
//    - an Apple2eMmu coordinator (owns the 64 KiB aux buffer)
//    - an Apple2eSoftSwitchBank for $C000-$C00B write-switches and
//      $C054-$C057 banking notifications
//
//  Constitution §II: tests construct their own scaffolding directly;
//  no host state, no Win32, no fixture I/O.
//
//  Per audit §1.1, write-switches live at the correct //e addresses
//  ($C002/$C003 RAMRD, $C004/$C005 RAMWRT, $C006/$C007 INTCXROM,
//  $C008/$C009 ALTZP, $C00A/$C00B SLOTC3ROM). The legacy AuxRamCard
//  (deleted) wired the wrong surface; this test class proves the new
//  surface is correct.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MmuTests)
{
public:

    struct MmuFixture
    {
        MemoryBus              bus;
        RamDevice              mainRam;
        Apple2eMmu             mmu;
        Apple2eSoftSwitchBank  sw;

        MmuFixture()
            : bus     (),
              mainRam (0x0000, 0xBFFF),
              mmu     (),
              sw      (&bus)
        {
            HRESULT  hr = S_OK;



            // Bind every $0000-$BFFF page to main RAM as the baseline
            // (the same baseline EmulatorShell::WirePageTable establishes).
            Byte * mainPtr = mainRam.GetData();

            for (int page = 0x00; page < 0xC0; page++)
            {
                bus.SetReadPage  (page, mainPtr + (page * 0x100));
                bus.SetWritePage (page, mainPtr + (page * 0x100));
            }

            sw.SetMmu (&mmu);
            hr = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, &sw);
            UNREFERENCED_PARAMETER (hr);

            bus.AddDevice (&sw);
        }
    };


    // Stand-in for an active slot-I/O card (e.g. a Mockingboard) that owns
    // a $Cn00 page. Records the last access so the router-delegation tests
    // can assert whether the device was reached.
    class RecordingIoDevice : public MemoryDevice
    {
    public:
        explicit RecordingIoDevice (Word start) : m_start (start) {}

        Byte Read     (Word address) override { m_lastReadAddr = address; return 0x5A; }
        void Write    (Word address, Byte value) override { m_lastWriteAddr = address; m_lastWriteValue = value; }
        Word GetStart() const override { return m_start; }
        Word GetEnd   () const override { return static_cast<Word> (m_start + 0xFF); }
        void Reset    () override {}

        Word m_start          = 0;
        Word m_lastReadAddr   = 0;
        Word m_lastWriteAddr  = 0;
        Byte m_lastWriteValue = 0;
    };


    TEST_METHOD (Default_AllFlagsClear)
    {
        MmuFixture f;

        Assert::IsFalse (f.mmu.GetRamRd     ());
        Assert::IsFalse (f.mmu.GetRamWrt    ());
        Assert::IsFalse (f.mmu.GetAltZp     ());
        Assert::IsFalse (f.mmu.Get80Store   ());
        Assert::IsFalse (f.mmu.GetIntCxRom  ());
        Assert::IsFalse (f.mmu.GetSlotC3Rom());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  RAMRD ($C002/$C003): main02_BF reads route to aux when on
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (RamRd_RoutesReadsToAux)
    {
        MmuFixture f;

        // Place sentinels: main = 0x11, aux = 0x22 at $1234.
        f.mainRam.GetData()       [0x1234] = 0x11;
        f.mmu.GetAuxBuffer()      [0x1234] = 0x22;

        // RAMRD off => read sees main.
        Assert::AreEqual (static_cast<Byte> (0x11), f.bus.ReadByte (0x1234));

        // Write $C003 -> RAMRD on. Read now sees aux.
        f.sw.Write (0xC003, 0);
        Assert::IsTrue (f.mmu.GetRamRd());
        Assert::AreEqual (static_cast<Byte> (0x22), f.bus.ReadByte (0x1234));

        // Write $C002 -> RAMRD off. Audit C2 fix: this is the correct
        // off-switch (legacy AuxRamCard wired this wrong).
        f.sw.Write (0xC002, 0);
        Assert::IsFalse (f.mmu.GetRamRd());
        Assert::AreEqual (static_cast<Byte> (0x11), f.bus.ReadByte (0x1234));
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  RAMWRT ($C004/$C005): main02_BF writes route to aux when on
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (RamWrt_RoutesWritesToAux)
    {
        MmuFixture f;

        // Power-on: write goes to main.
        f.bus.WriteByte (0x4567, 0xAA);
        Assert::AreEqual (static_cast<Byte> (0xAA), f.mainRam.GetData()[0x4567]);
        Assert::AreEqual (static_cast<Byte> (0x00), f.mmu.GetAuxBuffer()[0x4567]);

        // RAMWRT on -> write goes to aux.
        f.sw.Write (0xC005, 0);
        Assert::IsTrue (f.mmu.GetRamWrt());
        f.bus.WriteByte (0x4568, 0xBB);
        Assert::AreEqual (static_cast<Byte> (0xBB), f.mmu.GetAuxBuffer()[0x4568]);
        Assert::AreEqual (static_cast<Byte> (0x00), f.mainRam.GetData()[0x4568]);

        // RAMWRT off -> audit C4 fix.
        f.sw.Write (0xC004, 0);
        Assert::IsFalse (f.mmu.GetRamWrt());
        f.bus.WriteByte (0x4569, 0xCC);
        Assert::AreEqual (static_cast<Byte> (0xCC), f.mainRam.GetData()[0x4569]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  ALTZP ($C008/$C009): zero page + stack route to aux
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AltZp_RoutesZeroPageAndStack)
    {
        MmuFixture f;

        // ALTZP on.
        f.sw.Write (0xC009, 0);
        Assert::IsTrue (f.mmu.GetAltZp());

        // Write to zero page and stack.
        f.bus.WriteByte (0x0042, 0x77);  // ZP
        f.bus.WriteByte (0x01FE, 0x88);  // stack

        Assert::AreEqual (static_cast<Byte> (0x77), f.mmu.GetAuxBuffer()[0x0042]);
        Assert::AreEqual (static_cast<Byte> (0x88), f.mmu.GetAuxBuffer()[0x01FE]);
        Assert::AreEqual (static_cast<Byte> (0x00), f.mainRam.GetData() [0x0042]);

        // ALTZP off restores main routing.
        f.sw.Write (0xC008, 0);
        f.bus.WriteByte (0x0042, 0x99);
        Assert::AreEqual (static_cast<Byte> (0x99), f.mainRam.GetData() [0x0042]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  80STORE + PAGE2 routes $0400-$07FF to aux for both r/w
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Store80_Page2_RoutesText04_07ToAux)
    {
        MmuFixture f;

        // Enable 80STORE then PAGE2.
        f.sw.Write (0xC001, 0);  // 80STORE on
        Assert::IsTrue (f.mmu.Get80Store());

        f.bus.ReadByte (0xC055);      // PAGE2 on (display flag -> softswitch)
        Assert::IsTrue (f.sw.IsPage2());

        // Writes to text page must land in aux.
        f.bus.WriteByte (0x0400, 0xAB);
        f.bus.WriteByte (0x07FF, 0xCD);
        Assert::AreEqual (static_cast<Byte> (0xAB), f.mmu.GetAuxBuffer()[0x0400]);
        Assert::AreEqual (static_cast<Byte> (0xCD), f.mmu.GetAuxBuffer()[0x07FF]);

        // Reads of text page also come from aux.
        f.mmu.GetAuxBuffer()[0x0500] = 0x42;
        Assert::AreEqual (static_cast<Byte> (0x42), f.bus.ReadByte (0x0500));

        // PAGE1 ($C054) flips text routing back to main.
        f.bus.ReadByte (0xC054);
        f.bus.WriteByte (0x0400, 0xEE);
        Assert::AreEqual (static_cast<Byte> (0xEE), f.mainRam.GetData()[0x0400]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  80STORE + HIRES + PAGE2 routes $2000-$3FFF to aux
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Store80_Hires_Page2_RoutesHires20_3FToAux)
    {
        MmuFixture f;

        f.sw.Write (0xC001, 0);   // 80STORE on
        f.bus.ReadByte  (0xC057);      // HIRES on
        f.bus.ReadByte  (0xC055);      // PAGE2 on

        Assert::IsTrue (f.sw.IsHiresMode());
        Assert::IsTrue (f.sw.IsPage2());

        f.bus.WriteByte (0x2000, 0x55);
        f.bus.WriteByte (0x3FFF, 0xAA);

        Assert::AreEqual (static_cast<Byte> (0x55), f.mmu.GetAuxBuffer()[0x2000]);
        Assert::AreEqual (static_cast<Byte> (0xAA), f.mmu.GetAuxBuffer()[0x3FFF]);

        // Without HIRES the hires region is not banked even with 80STORE+PAGE2.
        f.bus.ReadByte (0xC056);  // HIRES off
        f.bus.WriteByte (0x2000, 0x33);
        Assert::AreEqual (static_cast<Byte> (0x33), f.mainRam.GetData()[0x2000]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  80STORE without HIRES leaves $2000-$3FFF to RAMRD / RAMWRT, including
    //  when they change while 80STORE is already on -- the 80-column
    //  firmware's text mode, where Infocom's 128K interpreter pages story
    //  data into aux $2000-$3FFF
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Store80_NoHires_RamRdAndRamWrtSetLaterStillRouteHires20_3F)
    {
        MmuFixture f;

        f.sw.Write (0xC001, 0);   // 80STORE on, before RAMRD / RAMWRT change
        Assert::IsTrue  (f.mmu.Get80Store());
        Assert::IsFalse (f.sw.IsHiresMode());

        // RAMWRT on: writes to the hires page land in aux and leave main alone.
        f.sw.Write (0xC005, 0);
        f.bus.WriteByte (0x2000, 0x5A);
        f.bus.WriteByte (0x3FFF, 0xA5);

        Assert::AreEqual (static_cast<Byte> (0x5A), f.mmu.GetAuxBuffer()[0x2000]);
        Assert::AreEqual (static_cast<Byte> (0xA5), f.mmu.GetAuxBuffer()[0x3FFF]);
        Assert::AreEqual (static_cast<Byte> (0x00), f.mainRam.GetData()[0x2000], L"main $2000 must be untouched");
        Assert::AreEqual (static_cast<Byte> (0x00), f.mainRam.GetData()[0x3FFF], L"main $3FFF must be untouched");

        // RAMRD on: reads of the hires page come from aux.
        f.mainRam.GetData()[0x3000]  = 0x11;
        f.mmu.GetAuxBuffer()[0x3000] = 0x22;
        f.sw.Write (0xC003, 0);
        Assert::AreEqual (static_cast<Byte> (0x22), f.bus.ReadByte (0x3000));

        // Both off again: the hires page is main's, for reads and writes.
        f.sw.Write (0xC002, 0);
        f.sw.Write (0xC004, 0);
        Assert::AreEqual (static_cast<Byte> (0x11), f.bus.ReadByte (0x3000));

        f.bus.WriteByte (0x2000, 0x77);
        Assert::AreEqual (static_cast<Byte> (0x77), f.mainRam.GetData()[0x2000]);
        Assert::AreEqual (static_cast<Byte> (0x5A), f.mmu.GetAuxBuffer()[0x2000], L"aux keeps what RAMWRT put there");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  The display scans main for 40-column text, lo-res and hi-res whatever
    //  the CPU's banking. RAMRD, and 80STORE with PAGE2 (and HIRES for the
    //  hi-res page), move only the CPU's view; a renderer given main RAM must
    //  draw the same frame under all of them.
    //
    ////////////////////////////////////////////////////////////////////////////

    static constexpr int  kFbWidth  = 560;
    static constexpr int  kFbHeight = 384;

    //  Main and aux hold different bytes across the whole page, so a frame
    //  scanned from the wrong bank cannot match.
    static void FillPage (MmuFixture & f, Word first, Word last, Byte mainValue, Byte auxValue)
    {
        for (uint32_t a = first; a <= last; a++)
        {
            f.mainRam.GetData()[a]  = mainValue;
            f.mmu.GetAuxBuffer()[a] = auxValue;
        }
    }

    static vector<uint32_t> RenderFrame (VideoOutput & mode, AppleTextMode * text)
    {
        vector<uint32_t>  fb (kFbWidth * kFbHeight, 0);

        // The text renderer keeps a dirty-row cache; a fresh frame must not
        // reuse rows from the last one.
        if (text != nullptr)
        {
            text->InvalidateCache();
        }

        mode.Render (nullptr, fb.data(), kFbWidth, kFbHeight);
        return fb;
    }

    static void AssertScansMainWhateverTheBanking (MmuFixture & f, VideoOutput & mode, AppleTextMode * text, Word probe, bool hires)
    {
        vector<uint32_t>  baseline = RenderFrame (mode, text);
        vector<uint32_t>  banked;
        Byte              auxByte  = f.mmu.GetAuxBuffer()[probe];

        f.sw.Write (0xC003, 0);                // RAMRD on
        Assert::AreEqual (auxByte, f.bus.ReadByte (probe), L"fixture: RAMRD must bank the CPU's reads to aux");

        banked = RenderFrame (mode, text);
        Assert::IsTrue (baseline == banked, L"RAMRD moves the CPU's reads, not the display's");

        f.sw.Write (0xC002, 0);                // RAMRD off
        f.sw.Write (0xC001, 0);                // 80STORE on

        if (hires)
        {
            f.bus.ReadByte (0xC057);           // HIRES on
        }

        f.bus.ReadByte (0xC055);               // PAGE2 on
        Assert::AreEqual (auxByte, f.bus.ReadByte (probe), L"fixture: 80STORE+PAGE2 must bank the CPU to aux");

        banked = RenderFrame (mode, text);
        Assert::IsTrue (baseline == banked, L"80STORE+PAGE2 select the CPU's bank, not the displayed one");
    }

    TEST_METHOD (Text40_ScansMainWhateverTheCpuBanking)
    {
        MmuFixture     f;
        AppleTextMode  text (f.bus);

        FillPage (f, 0x0400, 0x07FF, 0xC1, 0xA0);   // main 'A', aux space
        text.SetMainMemory (f.mainRam.GetData());

        AssertScansMainWhateverTheBanking (f, text, &text, 0x0400, false);
    }

    TEST_METHOD (LoRes_ScansMainWhateverTheCpuBanking)
    {
        MmuFixture      f;
        AppleLoResMode  loRes (f.bus);

        FillPage (f, 0x0400, 0x07FF, 0x11, 0xEE);
        loRes.SetMainMemory (f.mainRam.GetData());

        AssertScansMainWhateverTheBanking (f, loRes, nullptr, 0x0400, false);
    }

    TEST_METHOD (HiRes_ScansMainWhateverTheCpuBanking)
    {
        MmuFixture      f;
        AppleHiResMode  hiRes (f.bus);

        FillPage (f, 0x2000, 0x3FFF, 0x00, 0x7F);
        hiRes.SetMainMemory (f.mainRam.GetData());

        AssertScansMainWhateverTheBanking (f, hiRes, nullptr, 0x2000, true);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Audit-fix anchors: each $C002-$C00B write-switch lands at the
    //  correct address (legacy AuxRamCard wired $C003-$C006 wrong).
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Audit_C2_RAMRDOFF_Works)
    {
        MmuFixture f;

        f.sw.Write (0xC003, 0);   // RAMRD on
        f.sw.Write (0xC002, 0);   // RAMRD off (this address was wrong before)

        Assert::IsFalse (f.mmu.GetRamRd());
    }

    TEST_METHOD (Audit_C4_RAMWRTOFF_Works)
    {
        MmuFixture f;

        f.sw.Write (0xC005, 0);
        f.sw.Write (0xC004, 0);

        Assert::IsFalse (f.mmu.GetRamWrt());
    }

    TEST_METHOD (Audit_C6_INTCXROMOFF_Works)
    {
        MmuFixture f;

        f.sw.Write (0xC007, 0);
        Assert::IsTrue (f.mmu.GetIntCxRom());

        f.sw.Write (0xC006, 0);
        Assert::IsFalse (f.mmu.GetIntCxRom());
    }

    TEST_METHOD (IntCxRom_TrackedFromC006_C007)
    {
        MmuFixture f;

        f.sw.Write (0xC007, 0);
        Assert::IsTrue (f.mmu.GetIntCxRom());

        f.sw.Write (0xC006, 0);
        Assert::IsFalse (f.mmu.GetIntCxRom());
    }

    TEST_METHOD (SlotC3Rom_TrackedFromC00A_C00B)
    {
        MmuFixture f;

        f.sw.Write (0xC00B, 0);
        Assert::IsTrue (f.mmu.GetSlotC3Rom());

        f.sw.Write (0xC00A, 0);
        Assert::IsFalse (f.mmu.GetSlotC3Rom());
    }

    TEST_METHOD (IntC8Rom_ResettableViaApi)
    {
        MmuFixture f;

        f.mmu.SetIntC8Rom (true);
        Assert::IsTrue (f.mmu.GetIntC8Rom());

        f.mmu.ResetIntC8Rom();
        Assert::IsFalse (f.mmu.GetIntC8Rom());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  RebindPageTable on every banking-change
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Mmu_RebindsPageTable_OnSwitchChange)
    {
        MmuFixture f;

        // Sentinel in aux at $9000.
        f.mmu.GetAuxBuffer()[0x9000] = 0x42;
        Assert::AreEqual (static_cast<Byte> (0x00), f.bus.ReadByte (0x9000));

        f.sw.Write (0xC003, 0);   // RAMRD on
        // Subsequent read must hit aux WITHOUT another switch access.
        Assert::AreEqual (static_cast<Byte> (0x42), f.bus.ReadByte (0x9000));
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Soft / power reset semantics
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (OnSoftReset_ClearsFlagsAndPreservesAuxRam)
    {
        MmuFixture f;

        f.mmu.GetAuxBuffer()[0x1111] = 0xEE;

        f.sw.Write (0xC003, 0);
        f.sw.Write (0xC005, 0);
        f.sw.Write (0xC009, 0);
        f.sw.Write (0xC001, 0);

        f.mmu.OnSoftReset();

        Assert::IsFalse (f.mmu.GetRamRd  ());
        Assert::IsFalse (f.mmu.GetRamWrt());
        Assert::IsFalse (f.mmu.GetAltZp  ());
        Assert::IsFalse (f.mmu.Get80Store());

        Assert::AreEqual (static_cast<Byte> (0xEE), f.mmu.GetAuxBuffer()[0x1111]);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  CxxxRomRouter (audit C8): INTCXROM physically remaps slot $C100-$CFFF
    //  to internal ROM. SLOTC3ROM/INTC8ROM control $C300-$C3FF and the
    //  $C800-$CFFF expansion window. Reads of $CFFF auto-clear INTC8ROM.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Slot6IsReachableWhenIntCxRomClear)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0xAA);
        vector<Byte> slot6    (0x0100, 0x00);

        slot6[0x00] = 0x66;

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (6, move (slot6));

        f.mmu.SetIntCxRom (false);

        Assert::AreEqual (static_cast<Byte> (0x66), f.bus.ReadByte (0xC600),
            L"Audit C8: INTCXROM=0 must expose slot 6 ROM at $C600");
    }

    TEST_METHOD (Slot6IsShadowedWhenIntCxRomSet)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0x00);
        vector<Byte> slot6    (0x0100, 0x66);

        // $C600 in internal-ROM space lives at offset (0xC600-0xC100)=0x500.
        internal[0x500] = 0xCC;

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (6, move (slot6));

        f.mmu.SetIntCxRom (true);

        Assert::AreEqual (static_cast<Byte> (0xCC), f.bus.ReadByte (0xC600),
            L"Audit C8: INTCXROM=1 must shadow slot 6 with internal ROM");
    }

    TEST_METHOD (SlotIoDeviceReachableWhenIntCxRomClear)
    {
        MmuFixture         f;
        RecordingIoDevice  io (0xC400);

        f.mmu.GetCxxxRouter()->SetSlotIoDevice (4, &io);
        f.mmu.SetIntCxRom (false);

        Assert::AreEqual (static_cast<Byte> (0x5A), f.bus.ReadByte (0xC404),
            L"INTCXROM=0 must route slot-4 $Cn00 reads to the active I/O device");
        Assert::AreEqual (static_cast<Word> (0xC404), io.m_lastReadAddr);

        f.bus.WriteByte (0xC405, 0x33);
        Assert::AreEqual (static_cast<Word> (0xC405), io.m_lastWriteAddr);
        Assert::AreEqual (static_cast<Byte> (0x33), io.m_lastWriteValue);
    }

    TEST_METHOD (SlotIoDeviceShadowedWhenIntCxRomSet)
    {
        MmuFixture         f;
        RecordingIoDevice  io (0xC400);

        vector<Byte>       internal (0x0F00, 0xEE);

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.GetCxxxRouter()->SetSlotIoDevice (4, &io);
        f.mmu.SetIntCxRom (true);

        Assert::AreEqual (static_cast<Byte> (0xEE), f.bus.ReadByte (0xC404),
            L"INTCXROM=1 must shadow the slot-4 I/O device with internal ROM");
        Assert::AreEqual (static_cast<Word> (0), io.m_lastReadAddr,
            L"A shadowed device must not observe the read");
    }

    TEST_METHOD (SlotC3Rom_ClearMapsInternal80ColFirmware)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0x00);
        vector<Byte> slot3    (0x0100, 0x33);

        // $C300 internal offset = 0x200.
        internal[0x200] = 0x80;

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (3, move (slot3));

        f.mmu.SetIntCxRom  (false);
        f.mmu.SetSlotC3Rom (false);

        Assert::AreEqual (static_cast<Byte> (0x80), f.bus.ReadByte (0xC300),
            L"SLOTC3ROM=0 must map internal 80-col firmware at $C300");
    }

    TEST_METHOD (SlotC3Rom_SetMapsSlot3Card)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0x00);
        vector<Byte> slot3    (0x0100, 0x00);

        slot3[0x00] = 0x33;
        internal[0x200] = 0x80;

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (3, move (slot3));

        f.mmu.SetIntCxRom  (false);
        f.mmu.SetSlotC3Rom (true);

        Assert::AreEqual (static_cast<Byte> (0x33), f.bus.ReadByte (0xC300),
            L"SLOTC3ROM=1 must map slot 3 card ROM at $C300");
    }

    TEST_METHOD (IntC8Rom_LatchedByC3xxAccess_AndClearedByCFFF)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0x00);
        vector<Byte> slot3    (0x0100, 0x00);

        // Internal $C800 lives at offset (0xC800-0xC100)=0x700.
        internal[0x700] = 0xC8;
        internal[0x200] = 0x80;

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (3, move (slot3));

        f.mmu.SetIntCxRom  (false);
        f.mmu.SetSlotC3Rom (false);

        Assert::IsFalse (f.mmu.GetIntC8Rom(),
            L"INTC8ROM should be clear at start");

        f.bus.ReadByte (0xC300);
        Assert::IsTrue (f.mmu.GetIntC8Rom(),
            L"Internal $C3xx access must latch INTC8ROM=1");

        Assert::AreEqual (static_cast<Byte> (0xC8), f.bus.ReadByte (0xC800),
            L"INTC8ROM=1: $C800-$CFFF must read internal ROM");

        // Read of $CFFF clears INTC8ROM.
        f.bus.ReadByte (0xCFFF);
        Assert::IsFalse (f.mmu.GetIntC8Rom(),
            L"$CFFF read must auto-clear INTC8ROM");
    }

    TEST_METHOD (IntC8Rom_LatchedByAnyC3xxAccessWithSlotC3RomReset)
    {
        MmuFixture    f;
        vector<Byte>  internal (0x0F00, 0x00);

        internal[0x700] = 0xC8;                 // internal $C800
        f.mmu.AttachInternalCxxxRom (move (internal));

        // A write: STA $C300 with INTCXROM and SLOTC3ROM reset.
        f.sw.Write      (0xC006, 0);            // INTCXROM off
        f.sw.Write      (0xC00A, 0);            // SLOTC3ROM off
        f.bus.WriteByte (0xC300, 0x00);

        Assert::IsTrue (f.mmu.GetIntC8Rom(),
            L"A $C3xx write with SLOTC3ROM reset must latch INTC8ROM");
        Assert::AreEqual (static_cast<Byte> (0xC8), f.bus.ReadByte (0xC800));

        f.bus.WriteByte (0xCFFF, 0x00);
        Assert::IsFalse (f.mmu.GetIntC8Rom(),
            L"A $CFFF write must clear INTC8ROM");

        // A read while INTCXROM is on.
        f.sw.Write     (0xC007, 0);             // INTCXROM on
        f.bus.ReadByte (0xC300);
        f.sw.Write     (0xC006, 0);             // INTCXROM off

        Assert::IsTrue (f.mmu.GetIntC8Rom(),
            L"INTCXROM must not gate the INTC8ROM latch");
        Assert::AreEqual (static_cast<Byte> (0xC8), f.bus.ReadByte (0xC800));

        // With SLOTC3ROM set, $C3xx is slot 3's and leaves INTC8ROM alone.
        f.bus.ReadByte  (0xCFFF);
        f.sw.Write      (0xC00B, 0);            // SLOTC3ROM on
        f.bus.ReadByte  (0xC300);
        f.bus.WriteByte (0xC300, 0x00);

        Assert::IsFalse (f.mmu.GetIntC8Rom(),
            L"A $C3xx access with SLOTC3ROM set must not latch INTC8ROM");
    }

    TEST_METHOD (CxxxExpansionWindow_FloatsWhenIntC8RomClear)
    {
        MmuFixture f;

        vector<Byte> internal (0x0F00, 0xAB);
        vector<Byte> slot6    (0x0100, 0x66);

        f.mmu.AttachInternalCxxxRom (move (internal));
        f.mmu.AttachSlotRom         (6, move (slot6));

        f.mmu.SetIntCxRom (false);

        // INTC8ROM is clear; $C800-$CFFF must NOT read internal ROM.
        Assert::AreEqual (static_cast<Byte> (0xFF), f.bus.ReadByte (0xC900),
            L"INTC8ROM=0 + INTCXROM=0: $C800-$CFFF expansion window floats");
    }
};
