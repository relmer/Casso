#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "MemoryProbeHelpers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RebindMainBaseline
//
//  MachineBuilder binds bus pages $00-$BF to the CPU's
//  internal memory[] buffer. The MMU's RebindPageTable is initially a
//  no-op since flags default to false. The first MMU rebind switches
//  pages to mainRam/auxRam buffers — the //e firmware never toggles a
//  banking flag during cold boot, so without an explicit nudge probes
//  would keep hitting cpu->memory[] instead of mainRam. Toggling each
//  flag on then off forces a clean rebind so subsequent probes see the
//  same buffers a real //e would.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::RebindMainBaseline (MachineHost & host)
{
    if (host.GetMmu() == nullptr)
    {
        return;
    }

    host.GetMmu()->SetRamRd  (true);
    host.GetMmu()->SetRamRd  (false);
    host.GetMmu()->SetRamWrt (true);
    host.GetMmu()->SetRamWrt (false);
    host.GetMmu()->SetAltZp  (true);
    host.GetMmu()->SetAltZp  (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMain / ReadAux ($0200-$BFFF)
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadMain (MachineHost & host, Word address)
{
    host.GetMmu()->SetRamRd (false);
    return host.GetMemoryBus().ReadByte (address);
}



Byte MemoryProbeHelpers::ReadAux (MachineHost & host, Word address)
{
    host.GetMmu()->SetRamRd (true);
    return host.GetMemoryBus().ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteMain / WriteAux ($0200-$BFFF)
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteMain (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetRamWrt (false);
    host.GetMemoryBus().WriteByte (address, value);
}



void MemoryProbeHelpers::WriteAux (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetRamWrt (true);
    host.GetMemoryBus().WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMainZp / ReadAuxZp / WriteMainZp / WriteAuxZp ($0000-$01FF)
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadMainZp (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (false);
    return host.GetMemoryBus().ReadByte (address);
}



Byte MemoryProbeHelpers::ReadAuxZp (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (true);
    return host.GetMemoryBus().ReadByte (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteMainZp
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteMainZp (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (false);
    host.GetMemoryBus().WriteByte (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteAuxZp
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteAuxZp (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (true);
    host.GetMemoryBus().WriteByte (address, value);
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

Byte MemoryProbeHelpers::ReadLcMainBank2 (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (false);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    return host.GetRefs().languageCard->ReadRam (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLcMainBank1
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadLcMainBank1 (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (false);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    return host.GetRefs().languageCard->ReadRam (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLcAuxBank2
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadLcAuxBank2 (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (true);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    return host.GetRefs().languageCard->ReadRam (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLcAuxBank1
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryProbeHelpers::ReadLcAuxBank1 (MachineHost & host, Word address)
{
    host.GetMmu()->SetAltZp (true);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    return host.GetRefs().languageCard->ReadRam (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLcMainBank2
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteLcMainBank2 (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (false);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetRefs().languageCard->WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLcMainBank1
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteLcMainBank1 (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (false);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetRefs().languageCard->WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLcAuxBank2
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteLcAuxBank2 (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (true);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetMemoryBus().ReadByte (kLcBank2OddRead);
    host.GetRefs().languageCard->WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLcAuxBank1
//
////////////////////////////////////////////////////////////////////////////////

void MemoryProbeHelpers::WriteLcAuxBank1 (MachineHost & host, Word address, Byte value)
{
    host.GetMmu()->SetAltZp (true);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetMemoryBus().ReadByte (kLcBank1OddRead);
    host.GetRefs().languageCard->WriteRam (address, value);
}
