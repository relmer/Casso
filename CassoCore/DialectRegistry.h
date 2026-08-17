#pragma once

#include "Dialect.h"





class DialectProfile;





////////////////////////////////////////////////////////////////////////////////
//
//  DialectRegistry
//
//  Every dialect the assembler knows, as data. Adding one is a row here plus
//  its profile -- not a switch somewhere that a second dialect has to be
//  threaded through.
//
//  Profiles are stateless and shared. A profile holds only its grammar, never
//  anything about the assembly in progress, so one instance serves every
//  session and there is nothing to reset between runs.
//
//  GetAllDialects exists so tests sweep the whole table rather than a
//  hand-picked sample, the same reason DirectiveTable::GetAllSpellings and
//  CommandLineParser::GetAllSubcommands do -- a dialect added to the table is
//  covered without anyone editing a test.
//
////////////////////////////////////////////////////////////////////////////////

class DialectRegistry
{
public:
    // One row of the table. Nested rather than declared in the .cpp: a bare
    // struct there has external linkage and no keyword can change that.
    struct Entry
    {
        const char            *  name;
        DialectId                id;
        const DialectProfile  *  profile;
    };

    // The profile for a dialect. Never null for a real enumerator.
    static const DialectProfile &  Get (DialectId id);

    // The dialect a command-line word names, or false when the word names none.
    static bool  TryLookUpByName (const std::string & name, DialectId & outId);

    // A construct some OTHER dialect defines. `dialect` is null when no other
    // dialect claims the spelling, which is the ordinary case for a plain
    // misspelling.
    //
    // `category` is what to CALL the thing, because the two ways a dialect can
    // claim a word are not interchangeable to whoever reads the message: a
    // directive is a construct the source has to stop using, and an alternate
    // instruction spelling names an instruction the machine has under another
    // name. Composing it here rather than at the call site keeps a rejection
    // from having to know how a dialect's vocabulary is organized.
    //
    // It carries its own article, so a message can place it without knowing
    // which word follows. The alternative is a caller choosing between `a` and
    // `an` for a fragment it did not write.
    struct ForeignConstruct
    {
        const DialectProfile  *  dialect  = nullptr;
        const char            *  category = "";
    };

    // Which dialect other than `active` defines a spelling. `upperSpelling` is
    // expected upper-cased.
    //
    // The active dialect is EXCLUDED rather than merely searched last. A word
    // the active dialect claims never reaches here -- it parsed -- so finding it
    // here would mean attributing a source's own vocabulary to a foreign
    // dialect, which is worse than saying nothing.
    static ForeignConstruct  FindForeignConstruct (const std::string & upperSpelling, DialectId active);

    // Every dialect, so tests can sweep the whole table.
    static std::span<const Entry>  GetAllDialects();
};
