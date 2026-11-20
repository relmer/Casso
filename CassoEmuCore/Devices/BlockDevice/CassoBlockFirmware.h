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
//  its ProDOS and SmartPort entries use at $C0n0-$C0nE. Running a command
//  copies the block between the image and the Apple's memory through the
//  bus, so the Apple's memory banking applies.
//
//  A card uses it when its ROM is missing or the machine file sets it.
//  `getUnit` returns the drive at an index from 0, or null past the last
//  one. ProDOS's drives 1 and 2 are indexes 0 and 1, and SmartPort's units
//  1 and up are indexes 0 and up. `device` is the SmartPort device name and
//  type the status call returns.
//
////////////////////////////////////////////////////////////////////////////////

class CassoBlockFirmware
{
public:
    using UnitLookup = std::function<ICassoBlockUnit * (int drive)>;

    struct Device
    {
        std::string  name;
        Byte         type    = 0x02;
        Byte         subtype = 0x00;
    };

    static constexpr Byte  kErrNone           = 0x00;
    static constexpr Byte  kErrBadCommand     = 0x01;
    static constexpr Byte  kErrBadParamCount  = 0x04;
    static constexpr Byte  kErrBadUnit        = 0x11;
    static constexpr Byte  kErrBadControl     = 0x21;
    static constexpr Byte  kErrIo             = 0x27;
    static constexpr Byte  kErrNoDevice       = 0x28;
    static constexpr Byte  kErrWriteProtected = 0x2B;
    static constexpr Byte  kErrBadBlock       = 0x2D;
    static constexpr Byte  kErrOffline        = 0x2F;

    static constexpr Word  kCommandPort = 0;
    static constexpr Word  kUnitPort    = 1;
    static constexpr Word  kBufferLow   = 2;
    static constexpr Word  kBufferHigh  = 3;
    static constexpr Word  kBlockLow    = 4;
    static constexpr Word  kBlockHigh   = 5;
    static constexpr Word  kRunPort     = 6;
    static constexpr Word  kCountLow    = 7;
    static constexpr Word  kCountHigh   = 8;
    static constexpr Word  kReturnLow   = 9;
    static constexpr Word  kReturnHigh  = 10;
    static constexpr Word  kSmartPort   = 11;
    static constexpr Word  kResumeLow   = 12;
    static constexpr Word  kResumeHigh  = 13;
    static constexpr Word  kLastError   = 14;

    CassoBlockFirmware (int slot, MemoryBus & bus) : m_slot (slot), m_bus (bus) {}

    static HRESULT  AssemblePage (int slot, std::vector<Byte> & outPage, std::string & outErrors);

    HRESULT  Enable    (UnitLookup getUnit, Device device);
    bool     IsEnabled () const { return !m_page.empty(); }
    void     SetClock  (const uint64_t * clock) { m_clock = clock; }
    void     Reset     ();

    Byte  ReadSlotPage (Word address) const { return m_page[address & 0xFF]; }
    Byte  Read         (Word offset);
    void  Write        (Word offset, Byte value);

private:
    Byte              Run          ();
    Byte              RunSmartPort ();
    Byte              Transfer     (ICassoBlockUnit & unit, BlockDeviceOp op, uint32_t block, Word buffer);
    Byte              ReadTo       (ICassoBlockUnit & unit, uint32_t block, Word buffer);
    Byte              WriteFrom    (ICassoBlockUnit & unit, uint32_t block, Word buffer);
    Byte              Status       (int index, Word buffer, Byte code);
    ICassoBlockUnit * GetUnit      () const;
    int               GetUnitCount () const;
    void              FlushUnit    ();
    void              SetXy        (uint32_t value) { m_x = (Byte) (value & 0xFF); m_y = (Byte) ((value >> 8) & 0xFF); }
    Byte              Peek         (Word address) { return m_bus.ReadByte (address); }
    Word              PeekWord     (Word address) { return (Word) (Peek (address) | (Peek ((Word) (address + 1)) << 8)); }
    uint64_t          GetNow       () const { return m_clock != nullptr ? *m_clock : 0; }

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
    Word                m_return   = 0;
    Word                m_resume   = 0;
    Byte                m_error    = 0;
    Byte                m_x        = 0;
    Byte                m_y        = 0;
    Device              m_device;
};
