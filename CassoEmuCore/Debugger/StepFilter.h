#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilter
//
//  The routines a step into treats as a step over (FR-070): each a symbol's
//  address, an address or a range, matched against a JSR's target. Session
//  state; SKIP adds, lists, removes and clears.
//
//  A name is resolved when it is added, so a routine keeps its address when
//  the symbol tables change afterward. Adding a range that is already in the
//  list replaces it, so the list holds no duplicates.
//
////////////////////////////////////////////////////////////////////////////////

class StepFilter
{
public:
    void   Add         (const std::string & name, Word first, Word last);
    bool   TryRemove   (const std::string & name, std::optional<std::pair<Word, Word>> range);
    void   Clear       ()                   { m_entries.clear(); }
    bool   IsEmpty     () const             { return m_entries.empty(); }
    bool   Contains    (Word address) const;

    const std::vector<StepFilterEntry> &  GetEntries () const { return m_entries; }

private:
    static bool  IsSameName (const std::string & a, const std::string & b);

    std::vector<StepFilterEntry>  m_entries;
};
