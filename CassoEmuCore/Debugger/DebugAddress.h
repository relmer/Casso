#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAddressSpace
//
//  Which memory an address is in. CurrentView is the memory the CPU
//  accesses at that address now; the others are fixed whatever the
//  registers select, so an address in them reads the same bytes in any
//  machine state.
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugAddressSpace : Byte
{
    CurrentView,
    System,
    Bank,
    Rom,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAddress
//
//  A 16-bit address with the memory it is in, the form every debugger
//  feature uses on a machine with banked memory. A Word converts to the
//  current view, so code written for a 64K address space works as written
//  on every machine without banks.
//
//  The text form is Apple's `b:nnnn` for a bank, `S:nnnn` for the system
//  bank and `R:nnnn` for the ROM; a command mode where `:` already has a
//  meaning uses `/` in its place.
//
////////////////////////////////////////////////////////////////////////////////

class DebugAddress
{
public:
    static constexpr Word  kFirstBanked = 0x2000;
    static constexpr Word  kLastBanked  = 0x9FFF;
    static constexpr Byte  kLastBank    = 0x0E;
    static constexpr char  kColon       = ':';
    static constexpr char  kSlash       = '/';

    constexpr DebugAddress () = default;
    constexpr DebugAddress (Word address) : m_address (address) {}

    static DebugAddress  MakeBank   (Byte bank, Word address);
    static DebugAddress  MakeSystem (Word address);
    static DebugAddress  MakeRom    (Word address);

    Word               GetAddress       () const { return m_address; }
    DebugAddressSpace  GetSpace         () const { return m_space;   }
    Byte               GetBank          () const { return m_bank;    }
    bool               IsCurrentView    () const { return m_space == DebugAddressSpace::CurrentView; }
    bool               IsInBankedRegion () const { return m_address >= kFirstBanked && m_address <= kLastBanked; }

    bool               operator==       (const DebugAddress & other) const = default;

    bool               Matches          (const DebugAddress & access) const;

    static HRESULT     Parse            (std::string_view text, char separator, DebugAddress & out);
    std::string        Format           (char separator) const;
    HRESULT            Validate         (size_t bankCount) const;

    static std::string FormatBankRule   (size_t bankCount);

private:
    static HRESULT     ParseSpace       (char prefix, DebugAddress & out);
    static HRESULT     ParseHex16       (std::string_view text, Word & out);

    Word               m_address = 0;
    DebugAddressSpace  m_space   = DebugAddressSpace::CurrentView;
    Byte               m_bank    = 0;
};
