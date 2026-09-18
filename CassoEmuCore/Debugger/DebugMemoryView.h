#pragma once

#include "Debugger/Reply.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryView
//
//  The machine's address space as the CPU would see it under the current
//  banking, read and written without side effects. Nothing here reads a soft
//  switch, latches the Cxxx ROM router, or touches a device.
//
////////////////////////////////////////////////////////////////////////////////

class DebugMemoryView
{
public:
    explicit DebugMemoryView (MachineHost & host);

    bool          TryPeek   (Word address, Byte & value) const;
    bool          TryPoke   (Word address, Byte value);
    bool          TryPatch  (Word address, Byte value);
    MemoryRegion  GetRegion (Word address) const;

private:
    static constexpr Word    kIoFirst      = 0xC000;
    static constexpr Word    kIoLast       = 0xC0FF;
    static constexpr Word    kSlotRomFirst = 0xC100;
    static constexpr Word    kSlotRomLast  = 0xCFFF;
    static constexpr Word    kPageMask     = 0x00FF;
    static constexpr size_t  kAuxRamSize   = 0x10000;

    bool          TryPeekRomDevice (Word address, Byte & value) const;
    bool          TryPatchRom      (Word address, Byte value);
    bool          IsAuxPage        (const Byte * page) const;

    MachineHost & m_host;
};
