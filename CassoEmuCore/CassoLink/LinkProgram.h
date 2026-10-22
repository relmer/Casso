#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkChip
//
//  What the Apple side talks to the PC through: a 6551, the //c's own or a
//  Super Serial Card's, or an Uthernet II's W5100. Each has its own build of
//  the shim and of the program.
//
////////////////////////////////////////////////////////////////////////////////

enum class LinkChip
{
    Acia,
    W5100,
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkNetworkConfig
//
//  The W5100's own address, subnet mask, gateway and MAC address, and the TCP
//  port the shim listens on for Casso Explorer.
//
////////////////////////////////////////////////////////////////////////////////

struct LinkNetworkConfig
{
    std::array<Byte, 4>  address = {};
    std::array<Byte, 4>  mask    = { 255, 255, 255, 0 };
    std::array<Byte, 4>  gateway = {};
    std::array<Byte, 6>  mac     = {};
    uint16_t             port    = 6502;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkShimOptions
//
//  What the host writes into the shim. The speed and the dial command apply
//  to a 6551 and the network to a W5100; the slot to both.
//
////////////////////////////////////////////////////////////////////////////////

struct LinkShimOptions
{
    LinkChip           chip    = LinkChip::Acia;
    DWORD              baud    = 115200;
    int                slot    = 2;
    std::string        dial;
    LinkNetworkConfig  network;
};





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
    static constexpr Word    kProgramVersion    = 48;
    static constexpr Byte    kShimVersion       = 4;
    static constexpr Word    kShimAddress       = 0x3D00;
    static constexpr Word    kZeroPageSave      = 0x3C00;   // the shim's copy of the machine's zero page, below it
    static constexpr Word    kProgramAddress    = 0x0800;
    static constexpr size_t  kShimLimit         = 0x300;   // $3D00-$3FFF, below the program's tables

    //  A W5100 shim's driver does not fit in a 6551 shim's space, so it loads
    //  lower, with its copy of the zero page below it.
    static constexpr Word    kW5100ShimAddress  = 0x3B00;
    static constexpr Word    kW5100ZeroPageSave = 0x3A00;
    static constexpr size_t  kW5100ShimLimit    = 0x500;   // $3B00-$3FFF

    //  Where the host writes the shim's starting speed: the 6551 control
    //  byte, and its text, padded with spaces.
    static constexpr size_t  kSpeedOffset       = 2;
    static constexpr size_t  kSpeedTextOffset   = 6;
    static constexpr size_t  kSpeedTextLength   = 6;

    //  A Wi-Fi modem's dial command, ending in a zero byte; empty for a
    //  serial or TCP link.
    static constexpr size_t  kDialOffset        = 12;
    static constexpr size_t  kDialLength        = 40;

    //  A W5100 shim's network, in the dial command's place: the address,
    //  mask and gateway, the MAC address, and the port, high byte first.
    static constexpr size_t  kNetAddressOffset  = 12;
    static constexpr size_t  kNetMaskOffset     = 16;
    static constexpr size_t  kNetGatewayOffset  = 20;
    static constexpr size_t  kNetMacOffset      = 24;
    static constexpr size_t  kNetPortOffset     = 30;

    //  The slot of the 6551 or the W5100: for a 6551 on a //c, the port.
    static constexpr size_t  kSlotOffset        = 0x38;

    //  Set by the W5100 builds in Ready's shim version byte.
    static constexpr Byte    kW5100ReadyFlag    = 0x80;

    static Word         GetShimAddress   (LinkChip chip) { return chip == LinkChip::W5100 ? kW5100ShimAddress  : kShimAddress; }
    static Word         GetZeroPageSave  (LinkChip chip) { return chip == LinkChip::W5100 ? kW5100ZeroPageSave : kZeroPageSave; }
    static size_t       GetShimLimit     (LinkChip chip) { return chip == LinkChip::W5100 ? kW5100ShimLimit    : kShimLimit; }

    static HRESULT      AssembleShim     (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip = LinkChip::Acia);
    static HRESULT      AssembleProgram  (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip = LinkChip::Acia);

    //  The shim, starting at the given speed. A boot disk and an emulated
    //  link use the link's speed; the typed bootstrap uses its own.
    static HRESULT      BuildShim        (const LinkShimOptions & options, std::vector<Byte> & outShim);
    static HRESULT      BuildShim        (DWORD baud, std::vector<Byte> & outShim, const std::string & dial = std::string());

    //  A boot disk, as a .dsk's bytes, whose boot sector loads the shim at its
    //  address and runs it at the given speed (FR-091). Nothing else is on
    //  the disk; the shim loads the program from the PC.
    static HRESULT      BuildBootDisk    (const LinkShimOptions & options, std::vector<Byte> & outDisk);
    static HRESULT      BuildBootDisk    (DWORD baud, std::vector<Byte> & outDisk, const std::string & dial = std::string());

    static std::string  GetShimSource    ();
    static std::string  GetProgramSource ();

    //  The 6551 control byte for a speed: 8 data bits, 1 stop bit and the
    //  rate. False for a speed the //c's port does not have.
    static bool         GetSpeedControl  (DWORD baud, Byte & outControl);

private:
    static HRESULT      Assemble (const std::string & source, Word origin, LinkChip chip, std::vector<Byte> & outBytes, std::string & outErrors);
};
