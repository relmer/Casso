#pragma once

#include "Directive.h"






////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundaryTrigger
//
//  Which occurrence of a construct is refused.
//
//  Two values because a construct can be inside the subset once and outside it
//  twice. A CPU-selection directive is the case that forces this: the first one
//  selects a processor Casso emulates and must be accepted, and the second
//  selects one it does not. Encoding that here keeps it a property of the ROW,
//  so the engine never learns which directive is the special one.
//
////////////////////////////////////////////////////////////////////////////////

enum class SubsetBoundaryTrigger
{
    EveryOccurrence,
    SecondOccurrence,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleLinkage
//
//  Whether the source being assembled depends on anything outside itself.
//
//  It decides which of two refusals a construct gets, and the two are not
//  interchangeable. A module that only publishes symbols can be made to
//  assemble by deleting the constructs that publish them and naming an origin;
//  a module that also consumes symbols defined elsewhere cannot, because
//  resolving those is the linker's whole job. Offering the first fix in the
//  second case sends a developer down a path that cannot work, which is worse
//  than offering no fix at all.
//
////////////////////////////////////////////////////////////////////////////////

enum class ModuleLinkage
{
    SelfContained,    // nothing here depends on another module
    DependsOnOther,   // at least one construct references something defined elsewhere
};





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundaryRow
//
//  One refused construct: how the source spells it, what it is, where the gap
//  is tracked, and what -- if anything -- to do instead.
//
//  Everything a refusal can say is here, so a message composed anywhere else
//  would be a second source of truth the moment a row changed.
//
////////////////////////////////////////////////////////////////////////////////

struct SubsetBoundaryRow
{
    // What the source writes, and what to call the thing it writes.
    Directive               token;
    const char            * spelling;
    const char            * construct;

    SubsetBoundaryTrigger   trigger;

    //  Where the gap is tracked, without the leading '#'. nullptr when no
    //  issue covers it, and the refusal then points nowhere.
    const char            * githubIssue;

    // Whether THIS construct is what makes the module depend on another one.
    // A module holding any such construct cannot be made to assemble by
    // deleting the constructs around it, so one of them rules out every
    // workaround in the table at once -- which is why the linkage is a property
    // of the whole module and not of the line being refused.
    bool                    makesModuleDependOnAnother;

    // What to do instead, for a module that depends on nothing else. Null for a
    // construct no module shape admits a fix for.
    const char            * workaround;

    // What to say in place of that fix once the module does depend on another.
    // Null where the module's shape changes nothing.
    const char            * workaroundRuledOut;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundary
//
//  The mechanism half of the boundary: looking a construct up, and turning a
//  row into the sentence a developer reads.
//
//  Separate from any one dialect's table on purpose. The rows are a dialect's
//  own -- Merlin refuses relocatable mode, another dialect will refuse its own
//  import and export declarations -- but the lookup and the wording are shared,
//  so the assembler can refuse a construct without naming the dialect that
//  defined it. A dialect-specific call in the two-pass driver is exactly the
//  per-dialect special case the profile seam exists to prevent.
//
////////////////////////////////////////////////////////////////////////////////

class SubsetBoundary
{
public:
    // The row for a token, or null when the token is inside the subset.
    //
    // A row coming back does NOT by itself mean the line is refused: a row
    // whose trigger is SecondOccurrence answers for a construct whose first
    // occurrence is accepted. The caller owns the count.
    static const SubsetBoundaryRow *  Find (std::span<const SubsetBoundaryRow> rows, Directive token);

    // One refusal, in the active dialect's vocabulary.
    static std::string                ComposeRefusal (const SubsetBoundaryRow & row,
                                                      ModuleLinkage             linkage);

private:
};
