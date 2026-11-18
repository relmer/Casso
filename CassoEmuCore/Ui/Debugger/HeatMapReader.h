#pragma once

#include "Debugger/HeatMapOptions.h"
#include "Debugger/HeatTypes.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"

class AccessHeatMap;
class DebugSession;
class HeatMapSymbols;
class IDebugTarget;
class Microcode;
class SymbolTable;
struct HeatAccessInfo;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader
//
//  The heat map's part of a snapshot: switches the machine's map on while
//  the heat map pane or a heat column is visible, and reads its levels, its
//  marks and the last accesses of the cell under the mouse.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapReader
{
public:
    //  How often a running machine's heat map is read again: the levels of
    //  every address and the bytes under them. Each snapshot between has the
    //  last reading. A stopped machine's is read at every build.
    static constexpr uint64_t  kHeatIntervalMs = 100;

    using Clock = uint64_t (*)();

    //  The heat map pane: while it is shown the machine's accesses are
    //  counted and each snapshot has their levels; hidden, the machine
    //  records nothing.
    void  SetHeatMapShown (bool shown) { m_isHeatMapShown = shown; m_isHeatReadDue = true; }
    bool  IsHeatMapShown  () const     { return m_isHeatMapShown; }

    //  The heat columns: while any is shown the accesses are counted too, and
    //  each snapshot has the CPU's address space's levels.
    void  SetHeatColumnsShown (bool shown) { m_isHeatColumnsShown = shown; m_isHeatReadDue = true; }
    bool  IsHeatColumnsShown  () const     { return m_isHeatColumnsShown; }

    //  Whether the levels come from the fading heat or the totals, and how
    //  long the heat takes to fade.
    void                    SetHeatMapOptions (const HeatMapOptions & options) { m_heatMapOptions = options; m_isHeatReadDue = true; }
    const HeatMapOptions &  GetHeatMapOptions () const                         { return m_heatMapOptions; }

    //  The next build reads the heat map again whatever the time, as after
    //  anything the window did; and the milliseconds the reading interval is
    //  timed by.
    void  MarkHeatMapReadDue ()            { m_isHeatReadDue = true; }
    void  SetClock           (Clock clock) { m_clock = clock; }

    //  The cell the mouse is over on the heat map, whose last writer and
    //  reader each snapshot has; none when it is over none.
    void  SetHeatMapHover (std::optional<Word> address) { m_heatMapHover = address; }

    //  An opcode's mnemonic with its operand's form, each operand byte a
    //  dot: "LDA (..),Y", "JMP ....", "INX"; empty for an opcode the
    //  instruction set does not define.
    static std::string  GetOpcodeForm (const Microcode * instructionSet, Byte opcode);

    void  BuildHeatMap (DebugSession & session, DebuggerViewSnapshot & snapshot, bool isPaused) const;

    //  The heat map ranges' copy of the symbols, taken again only when they
    //  have changed since the last.
    std::shared_ptr<const HeatMapSymbols>  GetHeatMapSymbols (const SymbolTable & symbols) const;

private:
    void  ReadHeatMap (DebugSession & session, const AccessHeatMap & map, DebuggerViewSnapshot::HeatMapState & state) const;

    std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels>  ReadHeatColumns (const AccessHeatMap & map) const;
    static HeatAccessInfo                                          GetHeatAccess   (DebugSession & session, HeatSpace space, bool isWrite, Word address);
    static void                                                    ReadHeatValues  (IDebugTarget & target, const AccessHeatMap & map, HeatMapOptions::Bank bank, std::vector<int16_t> & values, std::vector<uint16_t> & regions);
    static void                                                    FindHeatMarks   (IDebugTarget & target, const AccessHeatMap & map, DebuggerViewSnapshot::HeatMapState & state);

    //  Every opcode's form for the instruction set, made again only when the
    //  machine brings another.
    std::shared_ptr<const DebuggerViewSnapshot::OpcodeForms>  GetOpcodeForms (const IDebugTarget & target) const;

    bool                                           m_isHeatMapShown         = false;
    bool                                           m_isHeatColumnsShown     = false;
    HeatMapOptions                                 m_heatMapOptions;
    mutable std::shared_ptr<const HeatMapSymbols>  m_heatMapSymbols;
    mutable uint64_t                               m_heatMapSymbolsRevision = 0;
    std::optional<Word>                            m_heatMapHover;

    //  Every opcode's form, and the instruction set they were made for. The
    //  set's owner is stored with them, so its address is never reused while
    //  the forms are.
    mutable std::shared_ptr<const DebuggerViewSnapshot::OpcodeForms>    m_opcodeForms;
    mutable const Microcode                                           * m_opcodeFormsSet = nullptr;
    mutable std::shared_ptr<const std::vector<Microcode>>               m_opcodeFormsTable;

    //  The heat map as last read, when, and whether the next build reads it
    //  again whatever the time.
    mutable std::shared_ptr<const DebuggerViewSnapshot::HeatMapState>     m_heatRead;
    mutable std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels> m_heatColumnsRead;
    mutable uint64_t                                                      m_heatReadAt     = 0;
    mutable uint64_t                                                      m_heatSerial     = 0;
    mutable bool                                                          m_isHeatReadDue  = true;
    Clock                                                                 m_clock          = [] () -> uint64_t { return GetTickCount64(); };
};
