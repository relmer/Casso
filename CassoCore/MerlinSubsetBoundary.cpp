#include "Pch.h"

#include "MerlinSubsetBoundary.h"



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
        "112",
        false,
        s_kpszExportOnlyFix,
        s_kpszImportingHasNoFix,
    },

    {
        Directive::EntrySymbol,
        "ENT",
        "an entry symbol declaration",
        SubsetBoundaryTrigger::EveryOccurrence,
        "112",
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
        "112",
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
        "a second CPU-selection directive (65802/65816)",
        SubsetBoundaryTrigger::SecondOccurrence,
        nullptr,
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
