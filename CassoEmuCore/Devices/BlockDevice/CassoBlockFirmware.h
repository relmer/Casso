#pragma once

#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/ICassoBlockUnit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoBlockFirmware
//
//  Casso's firmware for a hard disk card, in place of Apple's ROM: the slot
//  page assembled from CassoBlock.s for the card's slot, and the registers
//  its ProDOS driver writes at $C0n0-$C0n8. Running a command copies the
//  block between the image and the Apple's memory through the bus, so the
//  Apple's memory banking applies.
//
//  A card uses it when its ROM is missing or the machine file sets it.
//  `getUnit` returns the drive for ProDOS's drive 1 or 2, or null.
//
////////////////////////////////////////////////////////////////////////////////

class CassoBlockFirmware
{
public:
    using UnitLookup = std::function<ICassoBlockUnit * (int drive)>;

    static constexpr Byte  kErrNone           = 0x00;
    static constexpr Byte  kErrBadCommand     = 0x01;
    static constexpr Byte  kErrIo             = 0x27;
    static constexpr Byte  kErrNoDevice       = 0x28;
    static constexpr Byte  kErrWriteProtected = 0x2B;

    static constexpr Word  kCommandPort = 0;
    static constexpr Word  kUnitPort    = 1;
    static constexpr Word  kBufferLow   = 2;
    static constexpr Word  kBufferHigh  = 3;
    static constexpr Word  kBlockLow    = 4;
    static constexpr Word  kBlockHigh   = 5;
    static constexpr Word  kRunPort     = 6;
    static constexpr Word  kCountLow    = 7;
    static constexpr Word  kCountHigh   = 8;

    CassoBlockFirmware (int slot, MemoryBus & bus) : m_slot (slot), m_bus (bus) {}

    static HRESULT  AssemblePage (int slot, std::vector<Byte> & outPage, std::string & outErrors);

    HRESULT  Enable    (UnitLookup getUnit);
    bool     IsEnabled () const { return !m_page.empty(); }
    void     SetClock  (const uint64_t * clock) { m_clock = clock; }
    void     Reset     ();

    Byte  ReadSlotPage (Word address) const { return m_page[address & 0xFF]; }
    Byte  Read         (Word offset);
    void  Write        (Word offset, Byte value);

private:
    Byte              Run       ();
    Byte              ReadTo    (ICassoBlockUnit & unit);
    Byte              WriteFrom (ICassoBlockUnit & unit);
    ICassoBlockUnit * GetUnit   () const;
    uint32_t          GetCount  () const;
    void              FlushUnit ();
    uint64_t          GetNow    () const { return m_clock != nullptr ? *m_clock : 0; }

    int                 m_slot     = 0;
    MemoryBus         & m_bus;
    UnitLookup          m_getUnit;
    std::vector<Byte>   m_page;
    const uint64_t    * m_clock    = nullptr;
    ICassoBlockUnit   * m_written  = nullptr;
    Byte                m_command  = 0;
    Byte                m_unit     = 0;
    Word                m_buffer   = 0;
    Word                m_block    = 0;
};
