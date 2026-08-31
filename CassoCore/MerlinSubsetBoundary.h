#pragma once

#include "SubsetBoundary.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSubsetBoundary
//
//  Where Merlin support ends, as ONE table.
//
//  Every refusal the assembler emits and every line of the help text that
//  describes the boundary is derived from these rows. That is the whole point:
//  help and implementation cannot disagree by construction, rather than by
//  someone noticing they have. A row added here is refused and documented in the
//  same edit, and a test sweeps the accessor so a row can never be added without
//  its refusal being exercised.
//
//  The help text is generated HERE and not where it is printed. Generating it in
//  the executable would put it beyond the reach of the unit tests, so the claim
//  above would be an assertion nothing checks -- which is the same as not making
//  it.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinSubsetBoundary
{
public:
    // Every refused construct, so a test sweeps the whole boundary rather than
    // a hand-picked sample -- matching MerlinDirectiveTable::GetAllSpellings.
    static std::span<const SubsetBoundaryRow>  GetAll();
};
