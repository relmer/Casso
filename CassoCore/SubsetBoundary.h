#pragma once

#include "Directive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SubsetBoundaryReason
//
//  WHY a construct is refused, as a class rather than as prose.
//
//  The boundary is defined by the reason rather than by a fixed list, so that
//  one capability arriving widens every construct that was waiting on it at
//  once. A refusal that only carried its own sentence could not be grouped, and
//  the question a developer actually asks -- "is any of this coming?" -- would
//  have to be answered by reading every row.
//
////////////////////////////////////////////////////////////////////////////////

enum class SubsetBoundaryReason
{
    NeedsLinker,             // the construct only means something once modules are linked
    NeedsUnemulatedCpu,      // it selects a processor Casso does not emulate
    OwnedByAnotherFeature,   // the capability exists elsewhere in Casso's plans
    NeedsItsOwnDecision,     // nothing else arriving will settle what it should do
};





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
//  One refused construct: how the source spells it, what it is, why it is
//  refused, what would widen the boundary, and what -- if anything -- to do
//  instead.
//
//  Everything a refusal or a help listing can say is here, so the two cannot
//  disagree by construction rather than by anybody noticing. A message composed
//  anywhere else would be a second source of truth the moment a row changed.
//
////////////////////////////////////////////////////////////////////////////////

struct SubsetBoundaryRow
{
    // What the source writes, and what to call the thing it writes.
    Directive               token;
    const char            * spelling;
    const char            * construct;

    SubsetBoundaryTrigger   trigger;
    SubsetBoundaryReason    reason;

    // Why it is refused, and what arriving would let it in. Both are prose
    // fragments the composer places in a fixed sentence, so a row states facts
    // rather than punctuation.
    const char            * explanation;
    const char            * widensWith;

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
                                                      ModuleLinkage             linkage,
                                                      const char              * dialectName);

    // Any dialect's rows as help output, one line each. Shared for the reason
    // the refusal wording is: where a boundary sits is a dialect's own fact, and
    // how it is worded is not.
    static std::string                ComposeHelpText (std::span<const SubsetBoundaryRow> rows);

private:
    static const char *               ReasonLabel (SubsetBoundaryReason reason);
};
