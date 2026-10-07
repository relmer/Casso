#pragma once

#include "Debugger/Reply.h"
#include "Ui/Debugger/ByteChanges.h"





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
//  bytes since. Redo writes an undone edit's bytes again; a new edit clears
//  what could be redone.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryEditModel : public IDxuiHexSource
{
public:
    static constexpr uint8_t  kMarkNone    = 0;
    static constexpr uint8_t  kMarkIo      = 1;
    static constexpr uint8_t  kMarkRom     = 2;
    static constexpr uint8_t  kMarkChanged = 3;

    //  A byte no read has reached yet, whose value and region are unknown.
    static constexpr uint8_t  kMarkUnread  = 4;

    //  The regions a window outlines: one for each run with a label of its
    //  own, every slot's ROM apart from the next. Main RAM is in none.
    static constexpr uint16_t  kRegionRom          = 1;
    static constexpr uint16_t  kRegionIo           = 2;
    static constexpr uint16_t  kRegionLcBank1      = 3;
    static constexpr uint16_t  kRegionLcBank2      = 4;
    static constexpr uint16_t  kRegionAux          = 5;
    static constexpr uint16_t  kRegionExpansionRom = 6;
    static constexpr uint16_t  kRegionSlotRom      = 8;     // plus the slot, 1 to 7

    //  The outline colors: ROM's and I/O's are their bytes' own, and the
    //  language card's and aux RAM's the memory map's. Until the window has
    //  the theme's, the dark themes' defaults.
    struct RegionColors
    {
        uint32_t  rom     = 0xFF7FB2E5;
        uint32_t  io      = 0xFF909090;
        uint32_t  lcBank1 = 0xFF5BB36A;
        uint32_t  lcBank2 = 0xFF2E8B57;
        uint32_t  aux     = 0xFFD98A4A;
    };

    static uint16_t      GetRegionKey   (MemoryRegion region, Word address);
    static std::wstring  GetRegionLabel (uint16_t key);

    void  SetRegionColors (const RegionColors & colors) { m_regionColors = colors; }
    //  An edit's bytes, which the window writes at the address directly.
    using PatchFn = std::function<void (Word address, std::span<const Byte> bytes)>;

    void  SetOnPatch (PatchFn fn) { m_onPatch = std::move (fn); }

    //  The bytes a snapshot read for this window, one region per byte; an
    //  unreadable (I/O) byte is empty. A byte whose value differs from the
    //  one an earlier snapshot read at its address is marked changed, as
    //  ByteChanges says, whether the machine or an edit changed it.
    void  SetContents (Word first, std::vector<std::optional<Byte>> bytes, std::vector<MemoryRegion> regions);

    //  Whether the machine is stopped, which holds the changed marks.
    void  SetPaused   (bool isPaused) { m_isPaused = isPaused; }

    uint64_t  GetByteCount  () const override { return kAddressSpace; }
    void      ReadBytes     (uint64_t offset, std::span<uint8_t> out) const override;
    void      ReadMarks     (uint64_t offset, std::span<uint8_t> out) const override;
    bool      WriteBytes    (uint64_t offset, std::span<const uint8_t> bytes) const override;
    bool      TryGetByteTip (uint64_t offset, std::wstring & tip) const override;
    void      ReadRegions   (uint64_t offset, std::span<uint16_t> out) const override;
    bool      TryGetRegionStyle (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const override;

    //  Where the view's rows start relative to a 16-byte boundary. Offset 0 is
    //  address `phase`, so Go to can put any address at a row's start.
    void  SetPhase     (Word phase) { m_phase = (Word) (phase & 0x000F); }
    Word  GetPhase     () const     { return m_phase; }
    Word  GetAddressOf (uint64_t offset) const { return (Word) ((offset + m_phase) & 0xFFFF); }

    //  The region of a byte this window was shown, for saying why an edit there
    //  was refused.
    std::optional<MemoryRegion>  TryGetRegion (Word address) const;

    bool  Undo         ();
    bool  Redo         ();
    bool  CanUndo      () const { return !m_history.empty(); }
    bool  CanRedo      () const { return !m_redo.empty(); }
    void  ClearHistory ()       { m_history.clear(); m_redo.clear(); }

    //  The newest edit Undo or Redo would act on, as "changed 2 bytes at $0300".
    std::wstring  GetUndoText () const;
    std::wstring  GetRedoText () const;

private:
    static constexpr uint64_t  kAddressSpace = 0x10000;

    struct Edit
    {
        Word               address = 0;
        std::vector<Byte>  replaced;
        std::vector<Byte>  written;
    };

    bool  TryGetShown   (uint64_t address, size_t & outIndex) const;
    void  SendPatch     (Word address, std::span<const Byte> bytes) const;

    //  The byte and region at an address from the current read, or else from
    //  the last read that reached it. False for an address none has.
    bool  TryGetKnown   (Word address, std::optional<Byte> & outByte, MemoryRegion & outRegion) const;

    Word                                      m_first    = 0;
    ByteChanges                               m_changes;
    Word                                      m_phase    = 0;
    bool                                      m_isPaused = true;
    mutable std::vector<std::optional<Byte>>  m_bytes;
    std::vector<MemoryRegion>                 m_regions;

    //  Every read's bytes and regions, kept by address, so a window scrolled
    //  past its read draws what it was last shown there rather than unmarked
    //  zeros until the next read arrives. A region is kept one above its
    //  value; zero is an address no read has reached.
    std::vector<std::optional<Byte>>          m_keptBytes;
    std::vector<uint8_t>                      m_keptRegions;
    mutable std::vector<Edit>                 m_history;
    mutable std::vector<Edit>                 m_redo;
    PatchFn                                   m_onPatch;
    RegionColors                              m_regionColors;
};
