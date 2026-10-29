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
//  port the shim listens on for Casso Explorer. The MAC address is a locally
//  administered one, or for an Uthernet II link, one made from the link's
//  address; a universal boot disk made for any other link has the first, and
//  an address from DHCP.
//
////////////////////////////////////////////////////////////////////////////////

struct LinkNetworkConfig
{
    std::array<Byte, 4>  address = {};
    std::array<Byte, 4>  mask    = { 255, 255, 255, 0 };
    std::array<Byte, 4>  gateway = {};
    std::array<Byte, 6>  mac     = { 0x02, 'C', 'A', 'S', 'S', 0x01 };
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
//  the core: the shim, resident at $3B00, and the program it loads at $0800.
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
    static constexpr Word    kProgramVersion    = 52;
    static constexpr Byte    kShimVersion       = 7;
    static constexpr Word    kShimAddress       = 0x3B00;
    static constexpr Word    kZeroPageSave      = 0x3A00;   // the shim's copy of the machine's zero page, below it
    static constexpr Word    kProgramAddress    = 0x0800;
    static constexpr size_t  kShimLimit         = 0x500;   // $3B00-$3FFF, below the program's tables

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

    //  The universal boot disk's payload: the first stage, which finds the
    //  machine's interfaces and waits for the PC on all of them, then each
    //  shim as built for its own address, which the stage copies there.
    static constexpr Word    kUniversalAddress  = 0x2000;
    static constexpr size_t  kUniversalLimit    = 0xC00;
    static constexpr Word    kUniversalAcia     = 0x2C00;
    static constexpr Word    kUniversalW5100    = 0x3100;

    //  The UDP port a universal disk's W5100 broadcasts its announcements to.
    static constexpr uint16_t  kAnnouncePort  = 6510;

    //  The slot finder, which runs ahead of a 6551 shim typed in or read from
    //  tape, in the page the shim's copy of the zero page later fills.
    static constexpr Word    kSlotFinderAddress = 0x3AA0;

    static HRESULT      AssembleShim     (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip = LinkChip::Acia);
    static HRESULT      AssembleProgram  (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip = LinkChip::Acia);

    //  The shim, starting at the given speed. A boot disk and an emulated
    //  link use the link's speed; the typed bootstrap uses its own.
    static HRESULT      BuildShim        (const LinkShimOptions & options, std::vector<Byte> & outShim);
    static HRESULT      BuildShim        (DWORD baud, std::vector<Byte> & outShim, const std::string & dial = std::string());

    //  A boot disk, as a .dsk's bytes, whose boot sector loads the shim at its
    //  address and runs it at the given speed. Nothing else is on
    //  the disk; the shim loads the program from the PC.
    static HRESULT      BuildBootDisk    (const LinkShimOptions & options, std::vector<Byte> & outDisk);
    static HRESULT      BuildBootDisk    (DWORD baud, std::vector<Byte> & outDisk, const std::string & dial = std::string());

    //  The universal boot disk: the stage and both shims, the 6551 shim at
    //  the options' speed and with their dial command, the W5100 shim with
    //  their network, whose address is used when no DHCP reply comes.
    //  The slot and the chip in the options are not used: the stage finds
    //  both.
    static HRESULT      AssembleUniversal      (std::vector<Byte> & outBytes, std::string & outErrors);
    static HRESULT      BuildUniversalBootDisk (const LinkShimOptions & options, std::vector<Byte> & outDisk);

    //  What a bootstrap loads, typed in or from tape, and where it starts:
    //  for a 6551, the slot finder at kSlotFinderAddress, zeros to the shim,
    //  then the shim, so the shim takes its slot from the Apple; for a
    //  W5100, the shim alone.
    static HRESULT      AssembleSlotFinder     (std::vector<Byte> & outBytes, std::string & outErrors);
    static HRESULT      BuildBootstrapImage    (const LinkShimOptions & options, std::vector<Byte> & outImage, Word & outStart);

    static std::string  GetShimSource       ();
    static std::string  GetProgramSource    ();
    static std::string  GetUniversalSource  ();
    static std::string  GetSlotFinderSource ();

    //  The 6551 control byte for a speed: 8 data bits, 1 stop bit and the
    //  rate. False for a speed the //c's port does not have.
    static bool         GetSpeedControl  (DWORD baud, Byte & outControl);

private:
    static HRESULT      Assemble (const std::string & source, Word origin, LinkChip chip, std::vector<Byte> & outBytes, std::string & outErrors);
};
