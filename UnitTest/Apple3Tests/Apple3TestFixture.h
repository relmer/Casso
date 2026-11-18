#pragma once

#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Machines/Apple3/Apple3Mmu.h"
#include "Machines/Apple3/Cpu6502Apple3.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingDevice
//
//  A device that returns a fixed byte and stores the last access's address,
//  standing in for the I/O space or the VIAs.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingDevice : public MemoryDevice
{
public:
    static constexpr Byte  kReadValue = 0x5A;

    Byte  Read     (Word address) override              { m_lastRead = address; m_readCount++; return kReadValue; }
    void  Write    (Word address, Byte value) override  { m_lastWrite = address; m_lastValue = value; m_writeCount++; }
    Word  GetStart () const override                    { return 0; }
    Word  GetEnd   () const override                    { return 0; }
    void  Reset    () override                          {}

    Word  m_lastRead   = 0;
    Word  m_lastWrite  = 0;
    Byte  m_lastValue  = 0;
    int   m_readCount  = 0;
    int   m_writeCount = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TestFixture
//
//  A bus, the Apple /// memory map with a recognizable ROM, a recording I/O
//  space and VIAs, and the Apple /// CPU. The CPU is made before the map
//  is initialized, since the CPU's constructor maps the CPU's RAM and the map
//  replaces that mapping.
//
////////////////////////////////////////////////////////////////////////////////

struct Apple3TestFixture
{
    static constexpr Byte  kRomFill = 0xA5;

    MemoryBus        bus;
    Apple3Mmu        mmu;
    Cpu6502Apple3    cpu;
    RecordingDevice  io;
    RecordingDevice  vias;

    explicit Apple3TestFixture (size_t ramBytes = Apple3Mmu::kRam128K)
        : cpu (bus, mmu)
    {
        std::vector<Byte>  rom (Apple3Mmu::kRomBytes, kRomFill);
        HRESULT            hr = S_OK;



        rom[0]                         = 0x01;
        rom[Apple3Mmu::kRomBytes - 1]  = 0x02;

        hr = mmu.Initialize (&bus, ramBytes, rom);
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, hr);

        mmu.AttachIo   (&io);
        mmu.AttachVias (&vias);
        bus.AddDevice  (&mmu);
    }

    //  Places code in the system bank at $0200 and points the CPU at it.
    void  LoadCode (std::initializer_list<Byte> code)
    {
        Word  address = kCodeAddress;



        for (Byte b : code)
        {
            bus.WriteByte (address++, b);
        }

        cpu.SetPC (kCodeAddress);
    }

    static constexpr Word  kCodeAddress = 0x0200;
};
