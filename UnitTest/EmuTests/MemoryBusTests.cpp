#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

// Include emulator sources directly for testing
// These use the Casso65Core Pch.h (no windows.h) so EHM takes portable path
#include "../Casso65Emu/Core/MemoryBus.h"
#include "../Casso65Emu/Core/MemoryDevice.h"
#include "../Casso65Emu/Devices/RamDevice.h"
#include "../Casso65Emu/Devices/RomDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Simple test device for MemoryBus tests
//
////////////////////////////////////////////////////////////////////////////////

class TestMemDevice : public MemoryDevice
{
public:
    TestMemDevice (Word start, Word end)
        : m_start (start), m_end (end), m_lastRead (0), m_lastWrite (0) {}

    Byte Read  (Word address) override { m_lastRead = address; return 0x42; }
    void Write (Word address, Byte value) override { m_lastWrite = address; m_lastValue = value; }
    Word GetStart () const override { return m_start; }
    Word GetEnd   () const override { return m_end; }
    void Reset    () override { m_lastRead = 0; m_lastWrite = 0; }

    Word m_start;
    Word m_end;
    Word m_lastRead;
    Word m_lastWrite;
    Byte m_lastValue;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MemoryBusTests)
{
public:

    TEST_METHOD (ReadByte_DispatchesToCorrectDevice)
    {
        MemoryBus bus;
        TestMemDevice dev (0x1000, 0x1FFF);
        bus.AddDevice (&dev);

        Byte val = bus.ReadByte (0x1500);

        Assert::AreEqual (static_cast<Byte> (0x42), val);
        Assert::AreEqual (static_cast<Word> (0x1500), dev.m_lastRead);
    }

    TEST_METHOD (WriteByte_DispatchesToCorrectDevice)
    {
        MemoryBus bus;
        TestMemDevice dev (0x2000, 0x2FFF);
        bus.AddDevice (&dev);

        bus.WriteByte (0x2500, 0xAB);

        Assert::AreEqual (static_cast<Word> (0x2500), dev.m_lastWrite);
        Assert::AreEqual (static_cast<Byte> (0xAB), dev.m_lastValue);
    }

    TEST_METHOD (ReadByte_UnmappedIO_ReturnsFloatingBus)
    {
        MemoryBus bus;

        Byte val = bus.ReadByte (0xC100);

        Assert::AreEqual (static_cast<Byte> (0xFF), val);
    }

    TEST_METHOD (Validate_OverlappingDevices_Fails)
    {
        MemoryBus bus;
        TestMemDevice dev1 (0x1000, 0x1FFF);
        TestMemDevice dev2 (0x1800, 0x2FFF);
        bus.AddDevice (&dev1);
        bus.AddDevice (&dev2);

        HRESULT hr = bus.Validate ();

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (Validate_NonOverlappingDevices_Succeeds)
    {
        MemoryBus bus;
        TestMemDevice dev1 (0x1000, 0x1FFF);
        TestMemDevice dev2 (0x2000, 0x2FFF);
        bus.AddDevice (&dev1);
        bus.AddDevice (&dev2);

        HRESULT hr = bus.Validate ();

        Assert::IsTrue (SUCCEEDED (hr));
    }

    TEST_METHOD (RemoveDevice_NoLongerRouted)
    {
        MemoryBus bus;
        TestMemDevice dev (0x3000, 0x3FFF);
        bus.AddDevice (&dev);
        bus.RemoveDevice (&dev);

        Byte val = bus.ReadByte (0x3500);

        // Unmapped outside I/O range returns 0
        Assert::AreEqual (static_cast<Byte> (0), val);
    }
};
