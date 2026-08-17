#include "Pch.h"

#include "MerlinSubsetBoundary.h"



//  What arriving would let the linker-dependent constructs in. One string for
//  all three, because they are one gap: a module that publishes symbols and a
//  module that consumes them are both waiting on the same missing tool.
static constexpr const char *  s_kpszLinkerWidening = "a relocating linker (GitHub issue #112)";



//  The fix for a module that publishes symbols and consumes none. It is stated
//  in full rather than as "remove the relocatable directive", because following
//  that alone leaves every entry declaration behind and each one is refused in
//  its own right -- a fix that half works reads as a wrong diagnostic.
static constexpr const char *  s_kpszExportOnlyFix =
    "Nothing in this module declares an external symbol, so it imports nothing: remove REL, "
    "drop the ENT declarations, and give the source an origin with ORG, and it assembles on its own.";



//  And what to say instead once the module does consume a symbol. Offering the
//  fix above here would send a developer down a path that cannot work, since no
//  amount of editing this file supplies a definition that lives in another one.
static constexpr const char *  s_kpszImportingHasNoFix =
    "This module declares external symbols, so it references definitions that live in other modules; "
    "there is no workaround, because resolving them needs the linker Casso does not have.";



//  Where Merlin support ends. Each row is one refused construct, and every
//  refusal and every line of help text is composed from these fields -- see the
//  header for why nothing may be worded anywhere else.
//
//  The three linker rows are one boundary in three spellings, and they carry
//  different advice on purpose: the relocatable directive and the entry
//  declaration have a fix for a module that imports nothing, and the external
//  declaration is what removes that fix for the whole module.
static constexpr SubsetBoundaryRow  s_kMerlinBoundary[] =
{
    {
        Directive::Relocatable,
        "REL",
        "relocatable-mode assembly",
        SubsetBoundaryTrigger::EveryOccurrence,
        SubsetBoundaryReason::NeedsLinker,
        "it produces a relocatable module for a linker to place, and Casso emits one absolutely located image",
        s_kpszLinkerWidening,
        false,
        s_kpszExportOnlyFix,
        s_kpszImportingHasNoFix,
    },

    {
        Directive::EntrySymbol,
        "ENT",
        "an entry symbol declaration",
        SubsetBoundaryTrigger::EveryOccurrence,
        SubsetBoundaryReason::NeedsLinker,
        "it publishes a symbol for a linker to resolve from another module",
        s_kpszLinkerWidening,
        false,
        s_kpszExportOnlyFix,
        s_kpszImportingHasNoFix,
    },

    //  The row that decides the advice on the two above. A module declaring one
    //  of these depends on a definition it does not contain, so nothing that can
    //  be done to this file makes it assemble alone.
    {
        Directive::ExternalSymbol,
        "EXT",
        "an external symbol declaration",
        SubsetBoundaryTrigger::EveryOccurrence,
        SubsetBoundaryReason::NeedsLinker,
        "it names a symbol defined in another module, and resolving that is what a linker is for",
        s_kpszLinkerWidening,
        true,
        nullptr,
        nullptr,
    },

    //  Accepted once and refused thereafter. Merlin's CPU selector is
    //  cumulative: the first occurrence reaches the 65C02, which Casso emulates,
    //  and the second reaches the 65802/65816, which it does not.
    {
        Directive::CpuSelect,
        "XC",
        "a second CPU-selection directive",
        SubsetBoundaryTrigger::SecondOccurrence,
        SubsetBoundaryReason::NeedsUnemulatedCpu,
        "one selects the 65C02 and a second selects the 65802/65816, which Casso does not emulate",
        "a 65802/65816 core",
        false,
        nullptr,
        nullptr,
    },

    {
        Directive::FileType,
        "TYP",
        "the output file-type directive",
        SubsetBoundaryTrigger::EveryOccurrence,
        SubsetBoundaryReason::OwnedByAnotherFeature,
        "it sets the filesystem file type of the output, which means nothing without a filesystem that has types",
        "Casso's disk file-access support, which is where filesystem file types belong",
        false,
        nullptr,
        nullptr,
    },

    //  NOT waiting on disk file access, and the widening text says so out loud.
    //  It saves the object accumulated so far and carries on, so one assembly
    //  can produce several outputs -- a question about what an assembly IS, which
    //  no amount of file-writing support answers.
    {
        Directive::SaveObject,
        "SAV",
        "the save-object directive",
        SubsetBoundaryTrigger::EveryOccurrence,
        SubsetBoundaryReason::NeedsItsOwnDecision,
        "it writes the object accumulated so far and carries on, so one assembly produces several outputs",
        "a decision about multi-output assembly, which disk file access will not settle",
        false,
        nullptr,
        nullptr,
    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSubsetBoundary::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::span<const SubsetBoundaryRow> MerlinSubsetBoundary::GetAll()
{
    return s_kMerlinBoundary;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSubsetBoundary::GetHelpText
//
//  The boundary as help output, generated from the rows above rather than
//  written beside them.
//
//  One line per row, carrying the same four facts a refusal carries: the
//  spelling, what the construct is, why it is refused, and what would widen the
//  boundary. A reader who has met a refusal recognizes the row, and a reader who
//  has not can see the shape of the gap before writing any source at all.
//
//  Nothing here enumerates the rows by hand. Adding a row to the table
//  documents it, and a test asserts that every row reaches this text -- so the
//  help cannot fall behind the implementation even for one commit.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinSubsetBoundary::GetHelpText()
{
    std::string  text = "Merlin constructs recognized and refused by name, and why:\n";



    for (const SubsetBoundaryRow & row : s_kMerlinBoundary)
    {
        text += std::string ("  ") + row.spelling + " -- " + row.construct
              + " [" + ReasonLabel (row.reason) + "]. " + row.explanation
              + ". Widens with " + row.widensWith + ".\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSubsetBoundary::ReasonLabel
//
//  The reason class as the few words a help listing shows.
//
//  A switch rather than a table indexed by the enum, so adding a reason fails
//  the build here instead of printing an empty bracket -- the reason class is
//  the part of a row a reader scans for, and a blank one reads as "no reason
//  given" rather than as a missing case.
//
////////////////////////////////////////////////////////////////////////////////

const char * MerlinSubsetBoundary::ReasonLabel (SubsetBoundaryReason reason)
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
