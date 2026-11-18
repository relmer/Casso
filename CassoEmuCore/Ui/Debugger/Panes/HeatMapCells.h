#pragma once

#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"
#include "Ui/Debugger/MemoryRegionKeys.h"

struct BreakpointInfo;
class  HeatMapSymbols;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells
//
//  The heat map's data for each address of the bank on the map: how hot
//  each kind of access made it, the color its cell is drawn in, the bytes,
//  opcodes and symbols there, and the outlines over it. The layout of the
//  cells on the pane is in HeatMapGrid, and a cell's tip in HeatMapTips.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapCells
{
public:
    using Mode = HeatMapOptions::View;
    using Bank = HeatMapOptions::Bank;

    //  The heat's colors, then the overlays': the PC's outline and the stack
    //  pointer's, a breakpoint's and a read or write watchpoint's, the tint
    //  of an address with a symbol, and Blend's color for an address both run
    //  as code and written.
    struct Palette
    {
        uint32_t  background    = 0;
        uint32_t  cold          = 0;
        uint32_t  execute       = 0;
        uint32_t  read          = 0;
        uint32_t  write         = 0;
        uint32_t  pc            = 0;
        uint32_t  stack         = 0;
        uint32_t  breakpoint    = 0;
        uint32_t  readWatch     = 0;
        uint32_t  writeWatch    = 0;
        uint32_t  symbol        = 0;
        uint32_t  selfModifying = 0;
        uint32_t  unwritten     = 0;
        uint32_t  changed       = 0;
        uint32_t  found         = 0;

        bool operator== (const Palette & other) const = default;
    };

    //  A breakpoint the map outlines: on running an address, or a watchpoint
    //  on reading or writing a span, or both.
    enum class BreakKind
    {
        Execute,
        Read,
        Write,
        ReadWrite,
    };

    struct Breakpoint
    {
        int        id    = 0;
        Word       first = 0;
        Word       last  = 0;
        BreakKind  kind  = BreakKind::Execute;

        bool operator== (const Breakpoint & other) const = default;
    };

    //  What drew a cell: nothing touched it; or run as code, as an operand
    //  only, read, written, run as code and written, read before written, a
    //  write that changed its byte, or a Blend mix of kinds.
    enum class CellKind
    {
        Untouched,
        Code,
        Operand,
        Read,
        Written,
        SelfModifying,
        Unwritten,
        Changed,
        Blended,
    };

    static constexpr int    kAddressCount      = 0x10000;
    static constexpr float  kSymbolTint        = 0.25f;
    static constexpr float  kFaintest          = 0.3f;
    static constexpr float  kUnwrittenFaintest = 0.8f;

    static uint32_t                  GetColor        (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette);
    static uint32_t                  GetColor        (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette, bool isBlend);
    static uint32_t                  GetChannelColor (Mode mode, Byte unwritten, Byte changed, uint32_t color, const Palette & palette);
    static std::wstring              GetModeLabel    (Mode mode);
    static CellKind                  GetCellKind     (Mode mode, Byte execute, Byte read, Byte write, Byte unwritten, Byte changed, bool isOperand, bool isBlend);
    static std::optional<BreakKind>  GetBreakKind    (const BreakpointInfo & info);

    //  The levels, and the cells built from them; each setter that changes
    //  what a cell looks like builds the cells again. SetPalette and
    //  SetSymbols return whether they changed anything.
    void   SetLevels        (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write);
    void   SetChannelLevels (const std::vector<Byte> & unwritten, const std::vector<Byte> & changed, const std::vector<Byte> & edited);
    bool   SetPalette       (const Palette & palette);
    void   SetMode          (Mode mode);
    void   SetOptions       (const HeatMapOptions & options);
    void   SetOpcodes       (const std::vector<Byte> & opcodes);
    void   SetBreakpoints   (std::vector<Breakpoint> breakpoints);
    bool   SetSymbols       (std::shared_ptr<const HeatMapSymbols> symbols);

    void   SetValues        (const std::vector<int16_t> & values)                    { m_values = values; }
    void   SetOpcodeForms   (std::shared_ptr<const std::vector<std::string>> forms)  { m_opcodeForms = std::move (forms); }
    void   SetCpuMarks      (std::optional<Word> pc, std::optional<Word> stack)      { m_pc = pc; m_stack = stack; }
    void   SetFound         (Word address)                                           { m_found = address; }
    void   SetRegions       (const std::vector<uint16_t> & regions)                  { m_regions = regions; }
    void   SetSlotCards     (const MemoryRegionKeys::SlotCards & cards)              { m_slotCards = cards; }
    void   SetMachineId     (const std::string & machineId)                          { m_machineId = machineId; }
    void   SetRegionColors  (const MemoryRegionKeys::Colors & colors)                { m_regionColors = colors; }
    void   SetShownBank     (Bank bank)                                              { m_shownBank = bank; }

    const HeatMapOptions                                   & GetOptions         () const { return m_options; }
    const Palette                                          & GetPalette         () const { return m_palette; }
    const std::vector<int16_t>                             & GetValues          () const { return m_values; }
    const std::vector<Byte>                                & GetOpcodes         () const { return m_opcodes; }
    const std::shared_ptr<const std::vector<std::string>>  & GetOpcodeForms     () const { return m_opcodeForms; }
    std::optional<Word>                                      GetPc              () const { return m_pc; }
    std::optional<Word>                                      GetStack           () const { return m_stack; }
    std::optional<Word>                                      GetFound           () const { return m_found; }
    const std::vector<Breakpoint>                          & GetBreakpoints     () const { return m_breakpoints; }
    const std::vector<uint16_t>                            & GetRegions         () const { return m_regions; }
    const MemoryRegionKeys::SlotCards                      & GetSlotCards       () const { return m_slotCards; }
    const std::string                                      & GetMachineId       () const { return m_machineId; }
    const MemoryRegionKeys::Colors                         & GetRegionColors    () const { return m_regionColors; }
    Bank                                                     GetShownBank       () const { return m_shownBank; }
    const std::shared_ptr<const HeatMapSymbols>            & GetSymbols         () const { return m_symbols; }
    const std::vector<Byte>                                & GetExecuteLevels   () const { return m_execute; }
    const std::vector<Byte>                                & GetReadLevels      () const { return m_read; }
    const std::vector<Byte>                                & GetWriteLevels     () const { return m_write; }
    const std::vector<Byte>                                & GetUnwrittenLevels () const { return m_unwritten; }
    const std::vector<Byte>                                & GetChangedLevels   () const { return m_changed; }
    const std::vector<Byte>                                & GetEditedLevels    () const { return m_edited; }
    bool                                                     HasCellColors      () const { return m_cells.size() == (size_t) kAddressCount; }
    uint16_t                                                 GetRegionAt        (Word address) const { return (m_regions.size() == (size_t) kAddressCount) ? m_regions[address] : (uint16_t) 0; }

    Byte                   GetShownLevel       (Word address) const;
    bool                   IsOperand           (Word address) const;
    bool                   IsOpcode            (Word address) const { return m_opcodes.size() == (size_t) kAddressCount && m_opcodes[address] != 0; }
    bool                   IsMarked            (Word address) const;
    uint32_t               GetCellColor        (Word address) const;
    std::vector<uint32_t>  GetOutlineColors    (Word address) const;
    CellKind               GetCellKind         (Word address) const;
    uint32_t               GetValueInk         (Word address, uint32_t cell, uint32_t plain) const;
    std::string            GetSymbolAt         (Word address) const;

    //  Whether instructions can be drawn as one cell across their bytes, and
    //  the length of the one at an address, zero when it cannot be.
    bool                   CanMerge            () const;
    int                    GetMergeLength      (Word address) const;

    bool                   TryGetCellScale     (Word address, std::vector<uint32_t> & gradient, float & mark) const;
    std::vector<uint32_t>  GetScaleStops       (CellKind kind) const;

private:
    void                   BuildCells          ();
    void                   MarkSymbols         ();

    std::vector<Byte>                                m_execute;
    std::vector<Byte>                                m_read;
    std::vector<Byte>                                m_write;
    std::vector<Byte>                                m_unwritten;
    std::vector<Byte>                                m_edited;
    std::vector<Byte>                                m_changed;
    std::vector<uint32_t>                            m_cells;
    Palette                                          m_palette;
    HeatMapOptions                                   m_options;
    std::vector<Byte>                                m_opcodes;
    std::vector<int16_t>                             m_values;
    std::shared_ptr<const std::vector<std::string>>  m_opcodeForms;
    std::optional<Word>                              m_pc;
    std::optional<Word>                              m_stack;
    std::optional<Word>                              m_found;
    std::vector<Breakpoint>                          m_breakpoints;
    std::shared_ptr<const HeatMapSymbols>            m_symbols;
    Bank                                             m_symbolsBank   = Bank::Cpu;
    std::vector<Byte>                                m_symbolMarks;
    std::vector<uint16_t>                            m_regions;
    MemoryRegionKeys::SlotCards                      m_slotCards;
    std::string                                      m_machineId;
    MemoryRegionKeys::Colors                         m_regionColors;
    Bank                                             m_shownBank     = Bank::Cpu;
};
