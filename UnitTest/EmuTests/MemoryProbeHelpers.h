#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryProbeHelpers
//
//  Phase 8 (T074). Test-only utilities that POKE / PEEK specific //e
//  memory regions through the MemoryBus without running the CPU. Each
//  helper sets the minimal MMU / LC banking state required for the probe
//  to land in the intended physical buffer (main RAM, aux RAM, LC main
//  bank1/bank2/high, LC aux bank1/bank2/high).
//
//  Helpers do NOT save/restore prior banking state — callers either
//  rebuild the machine per test or set up the desired baseline
//  after a probe.
//
//  See spec.md US3 / FR-005 (RAMRD/RAMWRT), FR-006 (ALTZP), FR-008..
//  FR-012 (LC), FR-010 (aux LC).
//
////////////////////////////////////////////////////////////////////////////////

class MemoryProbeHelpers
{
public:

    // Force an MMU page-table rebind so $0000-$BFFF reads/writes go
    // through the MMU's main/aux buffers (not the CPU's memory[] which
    // the build binds for the cold-boot path). Call once after
    // a TestMachine is built, power-cycled and booted to its prompt.
    static void  RebindMainBaseline (MachineHost & host);

    // Main / aux RAM probes for $0200-$BFFF. Toggle RAMRD/RAMWRT then
    // dispatch through the bus.
    static Byte  ReadMain   (MachineHost & host, Word address);
    static Byte  ReadAux    (MachineHost & host, Word address);
    static void  WriteMain  (MachineHost & host, Word address, Byte value);
    static void  WriteAux   (MachineHost & host, Word address, Byte value);

    // Zero-page / stack probes ($0000-$01FF). Toggle ALTZP then
    // dispatch through the bus.
    static Byte  ReadMainZp (MachineHost & host, Word address);
    static Byte  ReadAuxZp  (MachineHost & host, Word address);
    static void  WriteMainZp (MachineHost & host, Word address, Byte value);
    static void  WriteAuxZp  (MachineHost & host, Word address, Byte value);

    // Language Card RAM probes ($D000-$FFFF). Drive the LC soft
    // switches into {bank, readRam, writeRam} via the bus, set ALTZP
    // for aux side, then call LanguageCard::ReadRam / WriteRam directly.
    static Byte  ReadLcMainBank1 (MachineHost & host, Word address);
    static Byte  ReadLcMainBank2 (MachineHost & host, Word address);
    static Byte  ReadLcAuxBank1  (MachineHost & host, Word address);
    static Byte  ReadLcAuxBank2  (MachineHost & host, Word address);

    static void  WriteLcMainBank1 (MachineHost & host, Word address, Byte value);
    static void  WriteLcMainBank2 (MachineHost & host, Word address, Byte value);
    static void  WriteLcAuxBank1  (MachineHost & host, Word address, Byte value);
    static void  WriteLcAuxBank2  (MachineHost & host, Word address, Byte value);

private:
    // LC soft-switch addresses chosen to land in {ReadRam, WriteRam}
    // for each bank. Two odd-address reads enable WriteRam via the
    // pre-write state machine (audit M6 / Sather UTAIIe §5-23).
    static constexpr Word   kLcBank2OddRead = 0xC083;
    static constexpr Word   kLcBank1OddRead = 0xC08B;
};
