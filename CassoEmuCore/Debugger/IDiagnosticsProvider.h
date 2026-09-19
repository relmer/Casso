#pragma once

#include "Pch.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiagnosticsProvider
//
//  A device that publishes rows for a debugger panel. The id is stable across
//  builds, since a saved window layout stores it; the title is what the panel
//  shows. GetDiagnostics runs on the CPU thread, only for a panel that is open,
//  and reads state without changing it.
//
////////////////////////////////////////////////////////////////////////////////

class IDiagnosticsProvider
{
public:
    static constexpr int  kByteDigits = 2;
    static constexpr int  kWordDigits = 4;

    virtual ~IDiagnosticsProvider() = default;

    virtual std::string  GetDiagnosticsId    () const                          = 0;
    virtual std::string  GetDiagnosticsTitle () const                          = 0;
    virtual void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const = 0;

    //  Rows in the forms every provider uses: on or off, a number in hex, and
    //  a byte in hex with a name for each bit, bit 7 first. An empty name
    //  leaves that bit out of the decode.
    static DiagnosticsRow  MakeFlagRow (const std::string & label, bool on);
    static DiagnosticsRow  MakeHexRow  (const std::string & label, uint32_t value, int digits);
    static DiagnosticsRow  MakeByteRow (const std::string & label, Byte value, const std::array<const char *, 8> & bitNames);
    static DiagnosticsRow  MakeTextRow (const std::string & label, const std::string & value);
};
