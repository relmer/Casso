#pragma once

#include "Pch.h"

class Apple2cRomBank;





////////////////////////////////////////////////////////////////////////////////
//
//  IRomWindowHandler
//
//  A device in the //c's internal ROM window, ahead of the ROM. Returns true, with the byte a read returns, for an
//  address it takes.
//
////////////////////////////////////////////////////////////////////////////////

class IRomWindowHandler
{
public:
    virtual ~IRomWindowHandler() = default;

    virtual bool  OnRomWindowAccess (Word address, bool isWrite, Byte value, Byte & outValue) = 0;

    //  Whether the handler takes any address in page `page` ($C1-$CF).
    virtual bool  HandlesPage (int page) const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MigChip
//
//  The //c Plus's MIG, in $CC00-$CCFF and $CE00-$CEFF while the alternate
//  ROM bank is switched in; with the main bank in, those addresses are ROM.
//
//      $CC40          written, resets the IWM
//      $CC80-$CC9F    written, turns the internal drive on
//      $CCC0-$CCDF    written, turns the internal drive off
//      $CE00-$CE1F    32 bytes of its 2 KB of RAM, at the page pointer
//      $CE20-$CE3F    the same, the pointer advanced $20 after the access
//      $CE40-$CE5F    read, selects head 0; written, turns 3.5 select off
//      $CE60-$CE7F    read, selects head 1; written, turns 3.5 select on
//      $CEA0          resets the page pointer
//
//  Other addresses in the two pages read as the floating bus value the
//  owner supplies. Switching the main bank back in clears the pointer, the
//  internal drive and 3.5 select. A MIG with no bank coordinator acts as
//  if the alternate bank were in.
//
////////////////////////////////////////////////////////////////////////////////

class MigChip : public IRomWindowHandler
{
public:
    static constexpr size_t  kRamSize = 0x800;

    using IwmResetFn = std::function<void ()>;

    explicit MigChip (const Apple2cRomBank * bank) : m_bank (bank) {}

    // IRomWindowHandler
    bool  OnRomWindowAccess (Word address, bool isWrite, Byte value, Byte & outValue) override;
    bool  HandlesPage       (int page) const override { return page == kControlPage || page == kRamPage; }

    void  SetIwmReset (IwmResetFn fn) { m_iwmReset = std::move (fn); }
    void  Reset       ();

    bool  IsInternalOn () { Sync(); return m_internalOn; }
    bool  IsSel35      () { Sync(); return m_sel35; }
    bool  IsHeadOne    () const { return m_headOne; }

    const std::array<Byte, kRamSize> & GetRam () const { return m_ram; }

private:
    static constexpr int   kControlPage  = 0xCC;
    static constexpr int   kRamPage      = 0xCE;
    static constexpr Word  kPageStep     = 0x20;
    static constexpr Word  kPointerMask  = 0x7E0;
    static constexpr int   kAlternate    = 1;

    bool  IsActive () const;
    void  Sync     ();
    bool  Access   (Word address, bool isWrite, Byte value, Byte & outValue);

    const Apple2cRomBank          * m_bank       = nullptr;
    IwmResetFn                      m_iwmReset;
    std::array<Byte, kRamSize>      m_ram        = {};
    Word                            m_pointer    = 0;
    bool                            m_internalOn = false;
    bool                            m_sel35      = false;
    bool                            m_headOne    = false;
    bool                            m_wasActive  = false;
};
