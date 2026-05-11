#include "../CassoEmuCore/Pch.h"

#include "MemoryProbeHelpers.h"


namespace
{
    // LC soft-switch addresses chosen to land in {ReadRam, WriteRam}
    // for each bank. Two odd-address reads enable WriteRam via the
    // pre-write state machine (audit M6 / Sather UTAIIe §5-23).
    static constexpr Word   kLcBank2OddRead   = 0xC083;
    static constexpr Word   kLcBank1OddRead   = 0xC08B;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebindMainBaseline
//
//  HeadlessHost::BuildAppleIIe binds bus pages $00-$BF to the CPU's
//  internal memory[] buffer. The MMU's RebindPageTable is initially a
//  no-op since flags default to false. The first MMU rebind switches
//  pages to mainRam/auxRam buffers — the //e firmware never toggles a
//  banking flag during cold boot, so without an explicit nudge probes
//  would keep hitting cpu->memory[] instead of mainRam. Toggling each
//  flag on then off forces a clean rebind so subsequent probes see the
//  same buffers a real //e would.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::RebindMainBaseline (EmulatorCore & core)
{
    if (core.mmu == nullptr)
    {
        return;
    }

    core.mmu->SetRamRd  (true);
    core.mmu->SetRamRd  (false);
    core.mmu->SetRamWrt (true);
    core.mmu->SetRamWrt (false);
    core.mmu->SetAltZp  (true);
    core.mmu->SetAltZp  (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMain / ReadAux ($0200-$BFFF)
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadMain (EmulatorCore & core, Word address)
{
    core.mmu->SetRamRd (false);
    return core.bus->ReadByte (address);
}



Byte MemoryProbeHelpers::ReadAux (EmulatorCore & core, Word address)
{
    core.mmu->SetRamRd (true);
    return core.bus->ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteMain / WriteAux ($0200-$BFFF)
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteMain (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetRamWrt (false);
    core.bus->WriteByte (address, value);
}



void MemoryProbeHelpers::WriteAux (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetRamWrt (true);
    core.bus->WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMainZp / ReadAuxZp / WriteMainZp / WriteAuxZp ($0000-$01FF)
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadMainZp (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (false);
    return core.bus->ReadByte (address);
}



Byte MemoryProbeHelpers::ReadAuxZp (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (true);
    return core.bus->ReadByte (address);
}



void MemoryProbeHelpers::WriteMainZp (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (false);
    core.bus->WriteByte (address, value);
}



void MemoryProbeHelpers::WriteAuxZp (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (true);
    core.bus->WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Language Card probes
//
//  Two odd-address reads at $C083 (or $C08B) leave the LC in
//  {bank2 (or bank1), ReadRam, WriteRam} per the pre-write state
//  machine. ALTZP picks main vs aux side. The LanguageCard's ReadRam /
//  WriteRam APIs hit the buffer directly — no need to round-trip
//  through the bus's LcBank intercept.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadLcMainBank2 (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (false);
    core.bus->ReadByte (kLcBank2OddRead);
    core.bus->ReadByte (kLcBank2OddRead);
    return core.languageCard->ReadRam (address);
}



Byte MemoryProbeHelpers::ReadLcMainBank1 (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (false);
    core.bus->ReadByte (kLcBank1OddRead);
    core.bus->ReadByte (kLcBank1OddRead);
    return core.languageCard->ReadRam (address);
}



Byte MemoryProbeHelpers::ReadLcAuxBank2 (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (true);
    core.bus->ReadByte (kLcBank2OddRead);
    core.bus->ReadByte (kLcBank2OddRead);
    return core.languageCard->ReadRam (address);
}



Byte MemoryProbeHelpers::ReadLcAuxBank1 (EmulatorCore & core, Word address)
{
    core.mmu->SetAltZp (true);
    core.bus->ReadByte (kLcBank1OddRead);
    core.bus->ReadByte (kLcBank1OddRead);
    return core.languageCard->ReadRam (address);
}



void MemoryProbeHelpers::WriteLcMainBank2 (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (false);
    core.bus->ReadByte (kLcBank2OddRead);
    core.bus->ReadByte (kLcBank2OddRead);
    core.languageCard->WriteRam (address, value);
}



void MemoryProbeHelpers::WriteLcMainBank1 (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (false);
    core.bus->ReadByte (kLcBank1OddRead);
    core.bus->ReadByte (kLcBank1OddRead);
    core.languageCard->WriteRam (address, value);
}



void MemoryProbeHelpers::WriteLcAuxBank2 (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (true);
    core.bus->ReadByte (kLcBank2OddRead);
    core.bus->ReadByte (kLcBank2OddRead);
    core.languageCard->WriteRam (address, value);
}



void MemoryProbeHelpers::WriteLcAuxBank1 (EmulatorCore & core, Word address, Byte value)
{
    core.mmu->SetAltZp (true);
    core.bus->ReadByte (kLcBank1OddRead);
    core.bus->ReadByte (kLcBank1OddRead);
    core.languageCard->WriteRam (address, value);
}
