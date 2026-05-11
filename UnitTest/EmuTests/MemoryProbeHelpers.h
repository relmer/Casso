#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





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
//  recreate the EmulatorCore per test or set up the desired baseline
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
    // HeadlessHost binds for the cold-boot path). Call once after
    // HeadlessHost::BuildAppleIIe + PowerCycle + BootToPrompt.
    static void  RebindMainBaseline (EmulatorCore & core);

    // Main / aux RAM probes for $0200-$BFFF. Toggle RAMRD/RAMWRT then
    // dispatch through the bus.
    static Byte  ReadMain   (EmulatorCore & core, Word address);
    static Byte  ReadAux    (EmulatorCore & core, Word address);
    static void  WriteMain  (EmulatorCore & core, Word address, Byte value);
    static void  WriteAux   (EmulatorCore & core, Word address, Byte value);

    // Zero-page / stack probes ($0000-$01FF). Toggle ALTZP then
    // dispatch through the bus.
    static Byte  ReadMainZp (EmulatorCore & core, Word address);
    static Byte  ReadAuxZp  (EmulatorCore & core, Word address);
    static void  WriteMainZp (EmulatorCore & core, Word address, Byte value);
    static void  WriteAuxZp  (EmulatorCore & core, Word address, Byte value);

    // Language Card RAM probes ($D000-$FFFF). Drive the LC soft
    // switches into {bank, readRam, writeRam} via the bus, set ALTZP
    // for aux side, then call LanguageCard::ReadRam / WriteRam directly.
    static Byte  ReadLcMainBank1 (EmulatorCore & core, Word address);
    static Byte  ReadLcMainBank2 (EmulatorCore & core, Word address);
    static Byte  ReadLcAuxBank1  (EmulatorCore & core, Word address);
    static Byte  ReadLcAuxBank2  (EmulatorCore & core, Word address);

    static void  WriteLcMainBank1 (EmulatorCore & core, Word address, Byte value);
    static void  WriteLcMainBank2 (EmulatorCore & core, Word address, Byte value);
    static void  WriteLcAuxBank1  (EmulatorCore & core, Word address, Byte value);
    static void  WriteLcAuxBank2  (EmulatorCore & core, Word address, Byte value);
};
