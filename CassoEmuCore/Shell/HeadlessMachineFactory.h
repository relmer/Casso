#pragma once

#include "Pch.h"

class IRomSource;
class MachineBuilder;
class MachineHost;
struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineFactory
//
//  Builds a shipped machine on a MachineHost with nothing attached to its
//  audio or printer: the same configuration loader and MachineBuilder the
//  emulator uses, with configuration and ROMs taken from an IRomSource.
//
//  The Prng is seeded explicitly, so two builds with the same seed power on
//  with the same memory.
//
////////////////////////////////////////////////////////////////////////////////

class HeadlessMachineFactory
{
public:

    static constexpr uint64_t  kDefaultSeed = 0xCA550001ULL;

    //  What goes in the machine's card slots.
    //
    //  AsShipped is the machine a user gets, Disk ][ in slot 6 and all. A
    //  machine with a drive and no disk in it does what the real one does:
    //  the autostart ROM hands over to slot 6 and the drive spins, so it
    //  never reaches BASIC. A machine with no disk card reaches the prompt,
    //  and is a machine a user can configure.
    enum class Slots
    {
        AsShipped,
        Empty,
        DiskOnly,     // slot 6 and nothing else
    };

    //  `machineId` is a shipped machine's directory name: "Apple2",
    //  "Apple2Plus", "Apple2e", "Apple2eEnhanced" or "Apple2c".
    static HRESULT  Build (MachineHost        & host,
                           MachineBuilder     & builder,
                           const IRomSource   & source,
                           const std::string  & machineId,
                           Slots                slots,
                           uint64_t             seed,
                           std::string        & error);

private:
    static HRESULT  LoadConfig (const IRomSource   & source,
                                const std::string  & machineId,
                                MachineConfig      & config,
                                std::string        & error);
    static void     ApplySlots (Slots slots, MachineConfig & config);
};
