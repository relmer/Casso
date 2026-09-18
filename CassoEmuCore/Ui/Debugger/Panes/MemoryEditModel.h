#pragma once

#include "Debugger/Reply.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryEditModel
//
//  One memory window's bytes, as the hex view reads them, and its edits.
//
//  THE WHOLE ADDRESS SPACE, FROM ONE SNAPSHOT'S WINDOW. The view asks for the
//  rows it draws anywhere in the 64K; the model answers from the bytes the
//  CPU thread last read for this window and zero elsewhere, and the window
//  moves what the CPU thread reads as the view scrolls.
//
//  AN EDIT IS A COMMAND LINE. A write the view hands over becomes a PATCH line
//  for the host to run, and is shown at once rather than waiting for the next
//  snapshot. An I/O address, or a byte this window was never shown, is
//  refused: the one because writing it has effects only OUT should cause, the
//  other because there is no replaced value for undo to restore.
//
//  UNDO IS THIS WINDOW'S. Each edit records the bytes it replaced, and undo
//  sends them back, most recent first, whatever the machine has done to those
//  bytes since.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryEditModel : public IDxuiHexSource
{
public:
    static constexpr uint8_t  kMarkNone = 0;
    static constexpr uint8_t  kMarkIo   = 1;
    static constexpr uint8_t  kMarkRom  = 2;

    using CommandFn = std::function<void (const std::string & line)>;

    void  SetOnCommand (CommandFn fn) { m_onCommand = std::move (fn); }

    //  The bytes a snapshot read for this window, one region per byte; an
    //  unreadable (I/O) byte is empty.
    void  SetContents (Word first, std::vector<std::optional<Byte>> bytes, std::vector<MemoryRegion> regions);

    uint64_t  GetByteCount () const override { return kAddressSpace; }
    void      ReadBytes    (uint64_t offset, std::span<uint8_t> out) const override;
    void      ReadMarks    (uint64_t offset, std::span<uint8_t> out) const override;
    bool      WriteBytes   (uint64_t offset, std::span<const uint8_t> bytes) const override;

    //  The region of a byte this window was shown, for saying why an edit there
    //  was refused.
    std::optional<MemoryRegion>  TryGetRegion (Word address) const;

    bool  Undo         ();
    bool  CanUndo      () const { return !m_history.empty(); }
    void  ClearHistory ()       { m_history.clear(); }

private:
    static constexpr uint64_t  kAddressSpace = 0x10000;

    struct Edit
    {
        Word               address = 0;
        std::vector<Byte>  replaced;
    };

    bool  TryGetShown   (uint64_t address, size_t & outIndex) const;
    void  SendPatch     (Word address, std::span<const Byte> bytes) const;

    Word                                        m_first = 0;
    mutable std::vector<std::optional<Byte>>    m_bytes;
    std::vector<MemoryRegion>                   m_regions;
    mutable std::vector<Edit>                   m_history;
    CommandFn                                   m_onCommand;
};
