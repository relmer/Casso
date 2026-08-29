#include "Pch.h"

#include "DialectRegistry.h"

#include "As65Dialect.h"
#include "MerlinDialect.h"





// The profiles themselves. Stateless, so one shared instance each -- a profile
// holds grammar and nothing about the assembly in progress.
static const As65Dialect    s_kAs65Dialect;
static const MerlinDialect  s_kMerlinDialect;



// Every dialect, in enumerator order so a row can be found by index as well as
// by name.
static constexpr DialectRegistry::Entry  s_kDialects[] =
{
    { "as65",   DialectId::As65,   &s_kAs65Dialect   },
    { "merlin", DialectId::Merlin, &s_kMerlinDialect },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectRegistry::Get
//
//  The profile for a dialect.
//
//  Asserts rather than returning null for an unknown enumerator: every value of
//  DialectId must have a row, and a missing one is a Casso bug rather than
//  anything a caller can recover from. AS65 is returned as the fallback so a
//  release build degrades to the default dialect instead of dereferencing null.
//
////////////////////////////////////////////////////////////////////////////////

const DialectProfile & DialectRegistry::Get (DialectId id)
{
    const DialectProfile  * found = nullptr;



    for (const Entry & entry : s_kDialects)
    {
        if (entry.id == id)
        {
            found = entry.profile;
            break;
        }
    }

    ASSERT (found != nullptr);

    if (found == nullptr)
    {
        found = &s_kAs65Dialect;
    }

    return *found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectRegistry::TryLookUpByName
//
//  The dialect a command-line word names.
//
//  Returns false rather than a sentinel dialect for an unrecognized word,
//  because the caller has to tell "no dialect named that" from "the default
//  applies" and a sentinel cannot express both.
//
////////////////////////////////////////////////////////////////////////////////

bool DialectRegistry::TryLookUpByName (const std::string & name, DialectId & outId)
{
    bool found = false;



    for (const Entry & entry : s_kDialects)
    {
        if (name == entry.name)
        {
            outId = entry.id;
            found = true;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectRegistry::FindForeignConstruct
//
//  Which dialect defines a word the active one rejected.
//
//  Both ways a dialect can claim a word are checked, because both produce the
//  same failure and neither is a misspelling. A directive is source written in
//  the wrong assembler's language. An instruction ALIAS is subtler: the machine
//  does have that instruction, under a name this dialect happens not to accept,
//  so "invalid mnemonic" is true of the spelling and false of the operation.
//
//  Directives first, since a dialect that spells a directive the way another
//  spells an instruction has a genuine collision and the directive is the
//  stronger claim -- it is the word's whole meaning there, where an alias is one
//  of two names for something the other dialect also has.
//
////////////////////////////////////////////////////////////////////////////////

DialectRegistry::ForeignConstruct DialectRegistry::FindForeignConstruct (const std::string & upperSpelling,
                                                                        DialectId           active)
{
    ForeignConstruct  found = {};



    for (const Entry & entry : s_kDialects)
    {
        if (entry.id == active)
        {
            continue;
        }

        if (entry.profile->GetDirectiveForSpelling (upperSpelling) != Directive::None)
        {
            found.dialect  = entry.profile;
            found.category = "a directive";
            break;
        }

        for (const MnemonicAlias & alias : entry.profile->GetMnemonicAliases())
        {
            if (upperSpelling == alias.spelling)
            {
                found.dialect  = entry.profile;
                found.category = "an alternate instruction name";
                break;
            }
        }

        if (found.dialect != nullptr)
        {
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectRegistry::GetAllDialects
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DialectRegistry::Entry> DialectRegistry::GetAllDialects()
{
    return std::span<const Entry> (s_kDialects);
}
