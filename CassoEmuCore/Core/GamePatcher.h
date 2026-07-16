#pragma once

#include "Pch.h"

class MemoryBus;




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher
//
//  A small pattern->fixup table applied against live guest RAM through the
//  MemoryBus. Each rule is a byte signature plus a replacement written at an
//  offset inside the match. Scanning is idempotent: once a site is patched the
//  signature no longer matches, so re-scans skip it; a site re-loaded from disk
//  over the patch is simply re-patched on the next pass. Writes go through the
//  bus so they honor the live MMU page map -- the flat CPU memory[] backing
//  store is the wrong buffer once the //e/c MMU repoints a page to aux.
//
//  The built-in table targets the Apple //c: some titles (e.g. Karateka) sync
//  to vertical blank with the //e idiom `LDA $C019 / BMI *-3`, which spins
//  forever on the //c where $C019 is a sticky VBL-interrupt latch (cleared only
//  by $C070), not the //e's live RDVBLBAR. NOP-ing the branch lets the wait
//  fall through, so the //c runs the same no-sync path the title already ships
//  for pre-//e machines.
//
////////////////////////////////////////////////////////////////////////////////

class GamePatcher
{
public:
    struct Rule
    {
        std::vector<Byte>   signature;
        size_t              patchOffset = 0;
        std::vector<Byte>   replacement;
        const char        * label       = "";
    };

    void   AddRule           (const Rule & rule);
    void   AddApple2cVblSpin ();     // built-in //c VBL-spin defuser
    void   Clear             ();     // drop all rules (machine switch)

    // Scan [lo, hi] of guest address space through `bus`, applying every rule
    // whose signature matches and is not already patched. Returns the number
    // of sites newly patched this pass.
    int    Scan              (MemoryBus & bus, Word lo, Word hi);

    // Scan the general-purpose RAM window ($0400-$BFF9), skipping zero page /
    // stack / input pages. The shell calls this once per emulated frame.
    int    ScanRam           (MemoryBus & bus);

    size_t RuleCount         () const { return m_rules.size (); }
    int    TotalApplied      () const { return m_totalApplied; }

private:
    static constexpr Word   s_kRamScanLo = 0x0400;
    static constexpr Word   s_kRamScanHi = 0xBFF9;

    bool   Matches           (MemoryBus & bus, Word at, const Rule & rule) const;

    std::vector<Rule>   m_rules;
    int                 m_totalApplied = 0;
};
