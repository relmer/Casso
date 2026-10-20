#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram
//
//  The Apple side of Casso's link, built from the 6502 sources compiled into
//  the core: the shim, resident at $3D00, and the program it loads at $0800.
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
    static constexpr Word    kProgramVersion    = 36;
    static constexpr Byte    kShimVersion       = 2;
    static constexpr Word    kShimAddress       = 0x3D00;
    static constexpr Word    kProgramAddress    = 0x0800;
    static constexpr size_t  kShimLimit         = 0x300;   // $3D00-$3FFF, below the program's tables

    //  Where the host writes the shim's starting speed: the 6551 control
    //  byte, and its text, padded with spaces.
    static constexpr size_t  kSpeedOffset       = 2;
    static constexpr size_t  kSpeedTextOffset   = 6;
    static constexpr size_t  kSpeedTextLength   = 6;

    //  A Wi-Fi modem's dial command, ending in a zero byte; empty for a
    //  serial or TCP link.
    static constexpr size_t  kDialOffset        = 12;
    static constexpr size_t  kDialLength        = 40;

    static HRESULT      AssembleShim     (std::vector<Byte> & outBytes, std::string & outErrors);
    static HRESULT      AssembleProgram  (std::vector<Byte> & outBytes, std::string & outErrors);

    //  The shim, starting at the given speed. A boot disk and an emulated
    //  link use the link's speed; the typed bootstrap uses its own.
    static HRESULT      BuildShim        (DWORD baud, std::vector<Byte> & outShim, const std::string & dial = std::string());

    //  A boot disk, as a .dsk's bytes, whose boot sector loads the shim at its
    //  address and runs it at the given speed (FR-091). Nothing else is on
    //  the disk; the shim loads the program from the PC.
    static HRESULT      BuildBootDisk    (DWORD baud, std::vector<Byte> & outDisk, const std::string & dial = std::string());

    static std::string  GetShimSource    ();
    static std::string  GetProgramSource ();

    //  The 6551 control byte for a speed: 8 data bits, 1 stop bit and the
    //  rate. False for a speed the //c's port does not have.
    static bool         GetSpeedControl  (DWORD baud, Byte & outControl);

private:
    static HRESULT      Assemble (const std::string & source, Word origin, std::vector<Byte> & outBytes, std::string & outErrors);
};
