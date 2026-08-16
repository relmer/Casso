#include "Pch.h"

#include "DialectRegistry.h"

#include "As65Dialect.h"





// The profiles themselves. Stateless, so one shared instance each -- a profile
// holds grammar and nothing about the assembly in progress.
static const As65Dialect  s_kAs65Dialect;



// Every dialect, in enumerator order so a row can be found by index as well as
// by name. Merlin's row lands here when its profile exists; until then the
// table has one entry and the mechanism is still the mechanism.
static constexpr DialectRegistry::Entry  s_kDialects[] =
{
    { "as65", DialectId::As65, &s_kAs65Dialect },
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
//  DialectRegistry::GetAllDialects
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DialectRegistry::Entry> DialectRegistry::GetAllDialects()
{
    return std::span<const Entry> (s_kDialects);
}
