#pragma once

#include "Debugger/Reply.h"

struct DebugMemorySpace;

class MachineHost;
struct DebugMemoryImage;





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

    bool          TryPeek        (Word address, Byte & value) const;
    bool          TryPeekIo      (Word address, Byte & value) const;
    bool          TryPeekRom     (Word address, Byte & value) const;
    bool          TryPoke        (Word address, Byte value);
    bool          TryPatch       (Word address, Byte value);
    MemoryRegion  GetRegion      (Word address) const;
    void          CopyAll        (DebugMemoryImage & image) const;
    void          GetSpaces      (std::vector<DebugMemorySpace> & spaces) const;
    bool          TrySelectSpace (const std::string & word);

private:
    static constexpr Word    kIoFirst      = 0xC000;
    static constexpr Word    kIoLast       = 0xC0FF;
    static constexpr Word    kSlotRomFirst = 0xC100;
    static constexpr Word    kSlotRomLast  = 0xCFFF;
    static constexpr Word    kLcFirst      = 0xD000;
    static constexpr Word    kPageMask     = 0x00FF;
    static constexpr size_t  kAuxRamSize   = 0x10000;
    static constexpr Word    kLowRamLast   = 0xBFFF;

    bool          TryPeekRomDevice (Word address, Byte & value) const;
    bool          TryPatchRom      (Word address, Byte value);
    bool          TryPokeShown     (Word address, Byte value);
    bool          IsAuxPage        (const Byte * page) const;
    bool          IsInSpace        (Word address) const;
    void          WriteSpace       (Word address, Byte value);

    MachineHost   & m_host;
    Byte          * m_spaceBase   = nullptr;   // the selected space's byte at m_spaceFirst, or null for the current view
    Word            m_spaceFirst  = 0;
    Word            m_spaceLast   = 0;
    MemoryRegion    m_spaceRegion = MemoryRegion::MainRam;
};
