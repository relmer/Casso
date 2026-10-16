#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram
//
//  The Apple side of Casso's link, built from the 6502 sources compiled into
//  the core: the shim, resident at $0300, and the program it loads at $0800.
//  Both are assembled here with CassoCore's assembler, in the AS65 dialect,
//  for the NMOS 6502.
//
//  The versions are defined here and nowhere else. Each assembly is given
//  PROGVER and SHIMVER ahead of its source, so the Ready packet the Apple
//  sends has these values and the host compares against the same ones.
//
////////////////////////////////////////////////////////////////////////////////

class LinkProgram
{
public:
    static constexpr Word    kProgramVersion = 1;
    static constexpr Byte    kShimVersion    = 1;
    static constexpr Word    kShimAddress    = 0x0300;
    static constexpr Word    kProgramAddress = 0x0800;
    static constexpr size_t  kShimLimit      = 0xF0;    // $0300-$03EF, below the monitor's vectors

    static HRESULT      AssembleShim     (std::vector<Byte> & outBytes, std::string & outErrors);
    static HRESULT      AssembleProgram  (std::vector<Byte> & outBytes, std::string & outErrors);

    static std::string  GetShimSource    ();
    static std::string  GetProgramSource ();

    //  The 6551 control byte for a speed: 8 data bits, 1 stop bit and the
    //  rate. False for a speed the //c's port does not have.
    static bool         GetSpeedControl  (DWORD baud, Byte & outControl);

private:
    static HRESULT      Assemble (const std::string & source, Word origin, std::vector<Byte> & outBytes, std::string & outErrors);
};
