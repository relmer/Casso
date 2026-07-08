#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Printer/PrinterByteRing.h"

struct DeviceConfig;

class MemoryBus;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterCard
//
//  Generic parallel interface card claiming the slot's 16-byte I/O window
//  ($C080 + slot*$10; slot 1 -> $C090-$C09F). A write to +$0 latches a data
//  byte into the SPSC ring the presenter drains; every read returns a status
//  byte with the ready convention asserted. Reads are tolerant across the
//  window (drivers poke different offsets); writes above +$0 are ignored.
//
//  The ready bit de-asserts within a high-water margin of the ring capacity
//  so a guest honoring the handshake stalls rather than overflowing when the
//  UI-thread drain is delayed (FR-002). In normal operation the drain keeps
//  the ring near-empty and ready stays asserted continuously.
//
//  When the card is disabled in machine config it is simply not attached, so
//  the slot presents today's empty-slot behavior (FR-001) -- the card never
//  needs to model the disabled case itself.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterCard : public MemoryDevice
{
public:
    static constexpr Word   kSlotIoBase   = 0xC080;
    static constexpr Word   kSlotIoStride = 0x10;
    static constexpr Word   kSlotIoSize   = 0x10;

    static constexpr Word   kDataOffset   = 0x0;   // +$0 write: data latch
    static constexpr Word   kStatusOffset = 0x1;   // +$1 read:  status

    // Ready asserts every commonly tested convention at once; busy clears
    // them (exact value locked after Print Shop capture, R-001).
    static constexpr Byte   kStatusReady  = 0xFF;
    static constexpr Byte   kStatusBusy   = 0x00;

    // Ready de-asserts once free ring space drops to this margin.
    static constexpr uint32_t   kReadyHighWater = 256;

    explicit PrinterCard (int slot);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    Byte Read (Word address) override;
    void Write (Word address, Byte value) override;
    Word GetStart() const override { return m_ioStart; }
    Word GetEnd() const override { return m_ioEnd; }
    void Reset() override;

    // Host (presenter) drains bytes the guest has written.
    PrinterByteRing & ByteRing() { return m_ring; }

    // FR-020: true once the guest first writes a data byte. The host reveals
    // the panel on the rising edge.
    bool EverTouched() const { return m_everTouched; }

    int  Slot() const { return m_slot; }

private:
    Byte ReadStatus() const;

    int               m_slot        = 0;
    Word              m_ioStart     = 0;
    Word              m_ioEnd       = 0;
    bool              m_everTouched = false;
    PrinterByteRing   m_ring;
};
