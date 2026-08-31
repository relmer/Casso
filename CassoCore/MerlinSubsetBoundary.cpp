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
        "the symbol is defined in another module, which would require linker support",
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
//  The boundary as help output: this dialect's own heading over the rows,
//  worded by the shared composer.
//
//  The per-row wording is NOT here, because it is not Merlin's. Where the
//  boundary sits is this table's fact; how a row reads is mechanism every
//  dialect's boundary shares, and duplicating it here is how the tool's own help
//  ends up describing two different sets of rules.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinSubsetBoundary::GetHelpText()
{
    return "Merlin constructs Casso recognizes and refuses, and why:\n"
         + SubsetBoundary::ComposeHelpText (s_kMerlinBoundary);
}
