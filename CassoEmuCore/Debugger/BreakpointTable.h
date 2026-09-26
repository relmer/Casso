#pragma once

#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/Reply.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  Breakpoint
//
//  One entry in the breakpoint table. first/last is the address range for
//  Address and Io kinds, and the address for MemoryValue; opcode is used by
//  Opcode; value by MemoryValue. condition is the whole predicate for
//  Register, and an optional IF expression for Address and MemoryValue,
//  evaluated only when the address or the write hits.
//
////////////////////////////////////////////////////////////////////////////////

struct Breakpoint
{
    int                  id        = 0;
    BreakpointKind       kind      = BreakpointKind::Address;
    Word                 first     = 0;
    Word                 last      = 0;
    Byte                 opcode    = 0;
    std::optional<Byte>  value;
    Expression           condition;
    bool                 enabled   = true;
    bool                 temporary = false;       // cleared once it fires
    bool                 stops     = true;        // false: counts hits only
    uint32_t             hits      = 0;
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

    int   AddAddress       (Word first, Word last, const Expression & condition = {});
    int   AddMemoryValue   (Word address, Byte value, const Expression & condition = {});
    int   AddOpcode        (Byte opcode);
    int   AddCondition     (const Expression & condition);
    int   AddIo            (Word first, Word last);
    int   AddBrk           ();
    int   AddInterrupt     ();

    bool  TryClear         (int id);
    void  ClearAll         ();
    void  ClearKind        (BreakpointKind kind);
    bool  TrySetEnabled    (int id, bool enabled);
    bool  TrySetFlags      (int id, bool temporary, bool stops);
    bool  HasBrk           () const;
    bool  HasOpcode        (Byte opcode) const;

    //  Inserts an entry under the id it carries, for BPEDIT, which keeps a
    //  breakpoint's id across its new definition. Fails if the id is taken.
    bool  TryAdopt         (const Breakpoint & entry);
    bool  TryFind          (int id, Breakpoint & entry) const;

    const std::vector<Breakpoint> &  GetAll () const { return m_entries; }

    bool  HasEnabledStopCondition () const;
    bool  IsAddressHit     (Word pc) const { return m_addressBits[pc]; }

    //  True when an enabled entry stops before the instruction at pc, whose
    //  opcode is given, or absent when it cannot be read (no opcode or BRK
    //  entry then matches). hitId receives the id of the first entry that
    //  stops. Address entries are checked first, then opcode and BRK
    //  entries, then register conditions. Every matching entry counts the
    //  hit, whether or not it stops and whether or not an earlier one did.
    bool  TryMatchBeforeInstruction (Word                            pc,
                                     std::optional<Byte>             opcode,
                                     const IDebugExpressionContext & context,
                                     int                           & hitId);

    //  True when an enabled MemoryValue entry at address stops on a write
    //  of value, its IF expression, if any, being true. Counts hits as
    //  TryMatchBeforeInstruction does; conditionValue receives the first
    //  stopping entry's expression value.
    bool  TryMatchWrite    (Word                            address,
                            Byte                            value,
                            const IDebugExpressionContext & context,
                            int                           & hitId,
                            std::optional<int32_t>        & conditionValue);

    //  The IF expression's value from the last stop TryMatchBeforeInstruction
    //  reported, when the entry had one.
    const std::optional<int32_t> &  GetLastConditionValue () const { return m_lastConditionValue; }

private:
    static constexpr size_t  kAddressCount = 0x10000;
    static constexpr Byte    kBrkOpcode    = 0x00;

    int   Add              (Breakpoint entry);
    void  RebuildAddressBits ();
    bool  TryMatchEntry    (const Breakpoint & entry, Word pc, std::optional<Byte> opcode, const IDebugExpressionContext & context, std::optional<int32_t> & conditionValue) const;

    int                     & m_nextId;
    std::vector<Breakpoint>   m_entries;
    std::vector<bool>         m_addressBits = std::vector<bool> (kAddressCount, false);
    std::optional<int32_t>    m_lastConditionValue;
};
