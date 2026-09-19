#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiagnosticsProvider::MakeFlagRow
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsRow IDiagnosticsProvider::MakeFlagRow (const std::string & label, bool on)
{
    return DiagnosticsRow { label, on ? "on" : "off", {} };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiagnosticsProvider::MakeHexRow
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsRow IDiagnosticsProvider::MakeHexRow (const std::string & label, uint32_t value, int digits)
{
    return DiagnosticsRow { label, std::format ("${:0{}X}", value, digits), {} };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiagnosticsProvider::MakeByteRow
//
//  The decode lists bit 7 first, the order a byte is written in.
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsRow IDiagnosticsProvider::MakeByteRow (const std::string & label, Byte value, const std::array<const char *, 8> & bitNames)
{
    constexpr int   kTopBit = 7;
    DiagnosticsRow  row     = MakeHexRow (label, value, kByteDigits);



    for (int bit = kTopBit; bit >= 0; bit--)
    {
        const char  * name = bitNames[(size_t) (kTopBit - bit)];

        if (name == nullptr || *name == '\0')
        {
            continue;
        }

        row.bits.push_back ({ name, ((value >> bit) & 1) != 0 });
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiagnosticsProvider::MakeTextRow
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsRow IDiagnosticsProvider::MakeTextRow (const std::string & label, const std::string & value)
{
    return DiagnosticsRow { label, value, {} };
}
