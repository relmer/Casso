#pragma once

#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/Reply.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  Breakpoint
//
//  One entry in the breakpoint table. first/last is the address range for
//  Address and Io kinds; opcode is used by Opcode; condition by Register.
//
////////////////////////////////////////////////////////////////////////////////

struct Breakpoint
{
    int             id        = 0;
    BreakpointKind  kind      = BreakpointKind::Address;
    Word            first     = 0;
    Word            last      = 0;
    Byte            opcode    = 0;
    Expression      condition;
    bool            enabled   = true;
    bool            temporary = false;            // cleared once it fires
    bool            stops     = true;             // false: counts hits only
    uint32_t        hits      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable
//
//  Ids come from a counter the session shares with the watchpoint table, so
//  every breakpoint and watchpoint has a distinct id, and an id is never
//  reused after a clear.
//
//  Enabled address breakpoints are kept in a 64 KB bitmap, rebuilt on every
//  change, so the common check before each instruction is one indexed load.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointTable
{
public:
    explicit BreakpointTable (int & nextId);

    int   AddAddress       (Word first, Word last);
    int   AddOpcode        (Byte opcode);
    int   AddCondition     (const Expression & condition);
    int   AddIo            (Word first, Word last);
    int   AddBrk           ();
    int   AddInterrupt     ();

    bool  TryClear         (int id);
    void  ClearAll         ();
    bool  TrySetEnabled    (int id, bool enabled);
    bool  TrySetFlags      (int id, bool temporary, bool stops);

    //  Inserts an entry under the id it carries, for BPEDIT, which keeps a
    //  breakpoint's id across its new definition. Fails if the id is taken.
    bool  TryAdopt         (const Breakpoint & entry);
    bool  TryFind          (int id, Breakpoint & entry) const;

    const std::vector<Breakpoint> &  GetAll () const { return m_entries; }

    bool  HasEnabledStopCondition () const;
    bool  IsAddressHit     (Word pc) const { return m_addressBits[pc]; }

    //  True when an enabled entry stops before the instruction at pc, whose
    //  opcode is given. hitId receives the entry's id, and its hit count is
    //  incremented. Address entries are checked first, then opcode and BRK
    //  entries, then register conditions. An entry that does not stop only
    //  counts the hit.
    bool  TryMatchBeforeInstruction (Word                            pc,
                                     Byte                            opcode,
                                     const IDebugExpressionContext & context,
                                     int                           & hitId);

private:
    static constexpr size_t  kAddressCount = 0x10000;
    static constexpr Byte    kBrkOpcode    = 0x00;

    int   Add              (Breakpoint entry);
    void  RebuildAddressBits ();
    bool  TryMatchEntry    (Breakpoint & entry, Word pc, Byte opcode, const IDebugExpressionContext & context) const;

    int                     & m_nextId;
    std::vector<Breakpoint>   m_entries;
    std::vector<bool>         m_addressBits = std::vector<bool> (kAddressCount, false);
};
