#include "Pch.h"

#include "SubsetBoundary.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundary::Find
//
//  A linear walk rather than an index by token, because the table is a handful
//  of rows and indexing it would oblige every dialect to size an array by the
//  whole directive vocabulary to describe the five constructs it refuses.
//
////////////////////////////////////////////////////////////////////////////////

const SubsetBoundaryRow * SubsetBoundary::Find (std::span<const SubsetBoundaryRow> rows, Directive token)
{
    const SubsetBoundaryRow *  found = nullptr;



    for (const SubsetBoundaryRow & row : rows)
    {
        if (row.token == token)
        {
            found = &row;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundary::ComposeRefusal
//
//  The one sentence every refusal is built from.
//
//  It says four things in a fixed order, and each is there because leaving it
//  out changes what the reader concludes. The spelling, so the line can be
//  found. What the construct IS, so the refusal reads as a decision rather than
//  as a parse failure. Why, so "Casso does not do this" is distinguishable from
//  "your source is wrong". And what would widen the boundary, so the answer to
//  "is this coming?" does not require finding the issue tracker.
//
//  The workaround comes last and is conditional on the whole module, not on the
//  line. A fix offered to a module that imports symbols from elsewhere cannot
//  work, so the row's other sentence is used instead -- see ModuleLinkage.
//
////////////////////////////////////////////////////////////////////////////////

std::string SubsetBoundary::ComposeRefusal (const SubsetBoundaryRow & row,
                                            ModuleLinkage             linkage,
                                            const char              * dialectName)
{
    std::string   text;
    const char *  advice = nullptr;



    text = std::string (row.spelling) + ": " + row.construct + " is outside the " + dialectName
         + " subset Casso supports -- " + row.explanation + ". Widens with " + row.widensWith + ".";

    if (row.workaround != nullptr)
    {
        advice = (linkage == ModuleLinkage::DependsOnOther && row.workaroundRuledOut != nullptr)
                     ? row.workaroundRuledOut
                     : row.workaround;

        text += std::string (" ") + advice;
    }

    return text;
}
