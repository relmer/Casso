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
                                            ModuleLinkage             linkage)
{
    std::string   text;
    const char *  advice = nullptr;



    text = std::string (row.spelling) + ": " + row.construct + " is not supported";

    if (row.githubIssue != nullptr)
    {
        text += std::string (" (GitHub issue #") + row.githubIssue + ")";
    }

    text += ".";

    if (row.workaround != nullptr)
    {
        advice = (linkage == ModuleLinkage::DependsOnOther && row.workaroundRuledOut != nullptr)
                     ? row.workaroundRuledOut
                     : row.workaround;

        text += std::string (" ") + advice;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundary::ComposeHelpText
//
//  The rows as help output, generated from them rather than written beside them.
//
//  One line per row, carrying the same four facts a refusal carries: the
//  spelling, what the construct is, why it is refused, and what would widen the
//  boundary. A reader who has met a refusal recognizes the row, and a reader who
//  has not can see the shape of the gap before writing any source at all.
//
//  Nothing here enumerates the rows by hand. Adding a row to a dialect's table
//  documents it, and a test asserts that every row reaches this text -- so the
//  help cannot fall behind the implementation even for one commit.
//
////////////////////////////////////////////////////////////////////////////////

std::string SubsetBoundary::ComposeHelpText (std::span<const SubsetBoundaryRow> rows)
{
    std::string  text;



    for (const SubsetBoundaryRow & row : rows)
    {
        text += std::string ("  ") + row.spelling + ": " + row.construct
              + " [" + GetReasonLabel (row.reason) + "]. " + row.explanation
              + ". Widens with " + row.widensWith + ".\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundary::GetReasonLabel
//
//  The reason class as the few words a help listing shows.
//
//  A switch rather than a table indexed by the enum, so adding a reason fails
//  the build here instead of printing an empty bracket -- the reason class is
//  the part of a row a reader scans for, and a blank one reads as "no reason
//  given" rather than as a missing case.
//
////////////////////////////////////////////////////////////////////////////////

const char * SubsetBoundary::GetReasonLabel (SubsetBoundaryReason reason)
{
    const char *  label = "";



    switch (reason)
    {
    case SubsetBoundaryReason::NeedsLinker:
        label = "needs a linker";
        break;

    case SubsetBoundaryReason::NeedsUnemulatedCpu:
        label = "needs a CPU Casso does not emulate";
        break;

    case SubsetBoundaryReason::OwnedByAnotherFeature:
        label = "owned by another part of Casso";
        break;

    case SubsetBoundaryReason::NeedsItsOwnDecision:
        label = "undecided";
        break;
    }

    return label;
}
