#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockDevice
//
//  Tracks reads and writes for verification in tests.
//
////////////////////////////////////////////////////////////////////////////////

class MockDevice : public MemoryDevice
{
public:
    MockDevice (Word start, Word end)
        : m_start     (start),
          m_end       (end),
          m_readCount (0),
          m_lastRead  (0),
          m_lastWrite (0),
          m_lastValue (0),
          m_returnVal (0x42)
    {
    }

    Byte Read (Word address) override
    {
        m_readCount++;
        m_lastRead = address;
        return m_returnVal;
    }

    void Write (Word address, Byte value) override
    {
        m_lastWrite = address;
        m_lastValue = value;
    }

    Word GetStart() const override { return m_start; }
    Word GetEnd  () const override { return m_end; }
    void Reset   () override       { m_readCount = 0; m_lastRead = 0; m_lastWrite = 0; }

    Word m_start;
    Word m_end;
    int  m_readCount;
    Word m_lastRead;
    Word m_lastWrite;
    Byte m_lastValue;
    Byte m_returnVal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusTests
//
//  Adversarial tests proving address routing is correct: that reads and writes
//  go to the right device, that unmapped I/O returns floating bus, that ROM
//  ignores writes, and that Validate catches overlaps.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MemoryBusTests)
{
public:

    TEST_METHOD (ReadByte_RoutesToCorrectDevice)
    {
        MemoryBus bus;
        MockDevice dev (0x1000, 0x1FFF);
        bus.AddDevice (&dev);

        Byte val = bus.ReadByte (0x1500);

        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"Read should return device value");
        Assert::AreEqual (static_cast<Word> (0x1500), dev.m_lastRead,
            L"Device should see the correct address");
    }

    TEST_METHOD (WriteByte_RoutesToCorrectDevice)
    {
        MemoryBus bus;
        MockDevice dev (0x2000, 0x2FFF);
        bus.AddDevice (&dev);

        bus.WriteByte (0x2500, 0xAB);

        Assert::AreEqual (static_cast<Word> (0x2500), dev.m_lastWrite,
            L"Write should go to correct device address");
        Assert::AreEqual (static_cast<Byte> (0xAB), dev.m_lastValue,
            L"Write should carry correct value");
    }

    TEST_METHOD (RamRom_Gap_FloatingBus)
    {
        // RAM at $0000-$BFFF, ROM at $D000-$FFFF, gap at $C000-$CFFF
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);
        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data(), romData.size());
        bus.AddDevice (rom.get());

        Byte val = bus.ReadByte (0xC100);

        Assert::AreEqual (static_cast<Byte> (0xFF), val,
            L"Unmapped I/O at $C100 should return floating bus ($FF)");
    }

    TEST_METHOD (DeviceAtC030_ReadGoesToDevice)
    {
        MemoryBus bus;
        MockDevice dev (0xC030, 0xC030);
        bus.AddDevice (&dev);

        Byte val = bus.ReadByte (0xC030);

        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"Read at $C030 should go to device");
        Assert::AreEqual (static_cast<Word> (0xC030), dev.m_lastRead,
            L"Device should see $C030");
    }

    TEST_METHOD (DeviceAtC030_ReadC031_DoesNotGoToDevice)
    {
        MemoryBus bus;
        MockDevice dev (0xC030, 0xC030);
        bus.AddDevice (&dev);

        bus.ReadByte (0xC031);

        Assert::AreEqual (0, dev.m_readCount,
            L"$C031 should NOT route to device at $C030-$C030");
    }

    TEST_METHOD (WriteToRom_SilentlyIgnored)
    {
        MemoryBus bus;

        std::vector<Byte> romData (0x3000, 0xEA);
        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data(), romData.size());
        bus.AddDevice (rom.get());

        // Writing to ROM should not crash
        bus.WriteByte (0xD000, 0x42);

        // ROM data should be unchanged
        Byte val = bus.ReadByte (0xD000);
        Assert::AreEqual (static_cast<Byte> (0xEA), val,
            L"ROM should ignore writes — data must remain $EA");
    }

    TEST_METHOD (Validate_OverlappingDevices_WarnsButSucceeds)
    {
        MemoryBus bus;
        MockDevice dev1 (0x1000, 0x1FFF);
        MockDevice dev2 (0x1800, 0x2FFF);
        bus.AddDevice (&dev1);
        bus.AddDevice (&dev2);

        HRESULT hr = bus.Validate();

        Assert::IsTrue (SUCCEEDED (hr),
            L"Overlapping devices should warn but not fail (first match wins)");
    }

    TEST_METHOD (Validate_NonOverlapping_Succeeds)
    {
        MemoryBus bus;
        MockDevice dev1 (0x1000, 0x1FFF);
        MockDevice dev2 (0x2000, 0x2FFF);
        bus.AddDevice (&dev1);
        bus.AddDevice (&dev2);

        HRESULT hr = bus.Validate();

        Assert::IsTrue (SUCCEEDED (hr),
            L"Non-overlapping devices should validate successfully");
    }

    TEST_METHOD (RemoveDevice_NoLongerRouted)
    {
        MemoryBus bus;
        MockDevice dev (0x3000, 0x3FFF);
        bus.AddDevice (&dev);
        bus.RemoveDevice (&dev);

        Byte val = bus.ReadByte (0x3500);

        Assert::AreEqual (static_cast<Byte> (0), val,
            L"Removed device should not be routed (non-IO returns 0)");
    }

    TEST_METHOD (MultipleDevices_CorrectRouting)
    {
        MemoryBus bus;
        MockDevice dev1 (0x1000, 0x1FFF);
        MockDevice dev2 (0x2000, 0x2FFF);
        dev1.m_returnVal = 0x11;
        dev2.m_returnVal = 0x22;
        bus.AddDevice (&dev1);
        bus.AddDevice (&dev2);

        Assert::AreEqual (static_cast<Byte> (0x11), bus.ReadByte (0x1500),
            L"$1500 should route to dev1");
        Assert::AreEqual (static_cast<Byte> (0x22), bus.ReadByte (0x2500),
            L"$2500 should route to dev2");
    }

    TEST_METHOD (Reset_ResetsAllDevices)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x00FF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0050, 0xAB);

        bus.Reset();

        Byte val = bus.ReadByte (0x0050);
        Assert::AreEqual (static_cast<Byte> (0x00), val,
            L"Reset should zero-fill RAM devices");
    }

    TEST_METHOD (FloatingBus_UpdatesOnLastRead)
    {
        // The floating bus value is the last value read from any device
        MemoryBus bus;
        MockDevice dev (0xC030, 0xC030);
        dev.m_returnVal = 0x77;
        bus.AddDevice (&dev);

        bus.ReadByte (0xC030);  // Returns 0x77, sets floating bus

        // Now read unmapped I/O — should get 0x77
        Byte val = bus.ReadByte (0xC040);
        Assert::AreEqual (static_cast<Byte> (0x77), val,
            L"Floating bus should reflect last device read value");
    }

    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Page table tests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (PageTable_RamRead_UsesMappedBuffer)
    {
        MemoryBus  bus;
        Byte       buffer[0x100] = {};
        buffer[0x42] = 0xAB;

        bus.SetReadPage  (0x04, buffer);
        bus.SetWritePage (0x04, buffer);

        Byte val = bus.ReadByte (0x0442);

        Assert::AreEqual (Byte (0xAB), val,
            L"Page table read should return mapped buffer value");
    }

    TEST_METHOD (PageTable_RamWrite_UpdatesMappedBuffer)
    {
        MemoryBus  bus;
        Byte       buffer[0x100] = {};

        bus.SetReadPage  (0x04, buffer);
        bus.SetWritePage (0x04, buffer);

        bus.WriteByte (0x0442, 0xCD);

        Assert::AreEqual (Byte (0xCD), buffer[0x42],
            L"Page table write should update mapped buffer");
    }

    TEST_METHOD (PageTable_NoPage_FallsBackToDevice)
    {
        MemoryBus  bus;
        MockDevice dev (0x0400, 0x04FF);
        bus.AddDevice (&dev);

        // No page mapped -- should hit device
        Byte val = bus.ReadByte (0x0442);

        Assert::AreEqual (1, dev.m_readCount,
            L"Without page table mapping, read should fall through to device");
        Assert::AreEqual (Byte (0x42), val,
            L"Should return device value");
    }

    TEST_METHOD (PageTable_RemapBetweenBuffers_RoutesCorrectly)
    {
        MemoryBus  bus;
        Byte       bufferA[0x100] = {};
        Byte       bufferB[0x100] = {};
        bufferA[0x10] = 0xAA;
        bufferB[0x10] = 0xBB;

        // Map page 4 to A
        bus.SetReadPage (0x04, bufferA);
        Byte v1 = bus.ReadByte (0x0410);

        // Remap to B
        bus.SetReadPage (0x04, bufferB);
        Byte v2 = bus.ReadByte (0x0410);

        Assert::AreEqual (Byte (0xAA), v1, L"First read should come from A");
        Assert::AreEqual (Byte (0xBB), v2, L"Second read should come from B");
    }

    TEST_METHOD (PageTable_BankingChangedCallback_Invoked)
    {
        MemoryBus bus;
        int       callCount = 0;

        bus.SetBankingChangedCallback ([&] () { callCount++; });

        bus.NotifyBankingChanged ();
        bus.NotifyBankingChanged ();

        Assert::AreEqual (2, callCount,
            L"NotifyBankingChanged should invoke registered callback");
    }

    TEST_METHOD (PageTable_NoCallback_NotifyDoesNotCrash)
    {
        MemoryBus bus;

        // No callback registered -- should be safe to call
        bus.NotifyBankingChanged ();
    }

    TEST_METHOD (PageTable_HighAddressUsesDevice_NotPageTable)
    {
        MemoryBus  bus;
        Byte       buffer[0x100] = {};
        buffer[0x00] = 0xFF;
        MockDevice dev (0xC000, 0xC0FF);
        bus.AddDevice (&dev);

        // Page table only applies to $0000-$BFFF; $C000+ goes to device
        bus.SetReadPage (0xC0, buffer);

        Byte val = bus.ReadByte (0xC042);

        Assert::AreEqual (1, dev.m_readCount,
            L"$C000+ should always use device dispatch, not page table");
        Assert::AreEqual (Byte (0x42), val,
            L"Should return device value");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusVideoDirtyTests
//
//  Proves the render-skip dirty flag: a fresh bus starts dirty, a write into
//  a watched display page (or a banking change, or a reset) raises it, a write
//  elsewhere does not, and ClearVideoDirty resets it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MemoryBusVideoDirtyTests)
{
public:

    TEST_METHOD (VideoDirty_StartsTrue_SoFirstFramePaints)
    {
        MemoryBus bus;

        Assert::IsTrue (bus.VideoDirty (),
            L"A fresh bus must start dirty so the first frame renders");
    }

    TEST_METHOD (WriteToWatchedPage_RaisesDirty)
    {
        MemoryBus  bus;
        Byte       page[0x100] = {};

        bus.SetWritePage     (0x04, page);   // $0400 text page
        bus.SetVideoWatchPage (0x04, true);
        bus.ClearVideoDirty  ();

        bus.WriteByte (0x0400, 0x41);

        Assert::IsTrue (bus.VideoDirty (),
            L"Write into a watched display page must raise video-dirty");
    }

    TEST_METHOD (WriteToUnwatchedPage_LeavesClean)
    {
        MemoryBus  bus;
        Byte       page[0x100] = {};

        bus.SetWritePage    (0x60, page);    // $6000, not a display page
        bus.ClearVideoDirty ();

        bus.WriteByte (0x6000, 0x41);

        Assert::IsFalse (bus.VideoDirty (),
            L"Write outside any watched page must not raise video-dirty");
    }

    TEST_METHOD (SameValueRewriteToWatchedPage_LeavesClean)
    {
        MemoryBus  bus;
        Byte       page[0x100] = {};

        bus.SetWritePage     (0x04, page);
        bus.SetVideoWatchPage (0x04, true);

        bus.WriteByte       (0x0400, 0x41);   // first write changes 0x00 -> 0x41
        bus.ClearVideoDirty ();

        bus.WriteByte (0x0400, 0x41);         // same value re-stored

        Assert::IsFalse (bus.VideoDirty (),
            L"Re-storing the identical byte must not raise video-dirty");
    }

    TEST_METHOD (BankingChange_RaisesDirty)
    {
        MemoryBus  bus;

        bus.ClearVideoDirty ();
        bus.NotifyBankingChanged ();

        Assert::IsTrue (bus.VideoDirty (),
            L"A banking change can swap the displayed buffer, so it must "
            L"raise video-dirty");
    }

    TEST_METHOD (Reset_RaisesDirty)
    {
        MemoryBus  bus;

        bus.ClearVideoDirty ();
        bus.Reset ();

        Assert::IsTrue (bus.VideoDirty (),
            L"Reset changes the screen, so it must raise video-dirty");
    }

    TEST_METHOD (ClearVideoDirty_Clears)
    {
        MemoryBus  bus;

        bus.MarkVideoDirty ();
        bus.ClearVideoDirty ();

        Assert::IsFalse (bus.VideoDirty (),
            L"ClearVideoDirty must reset the flag");
    }
};
