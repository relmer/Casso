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

    // Every dialect, so tests can sweep the whole table.
    static std::span<const Entry>  GetAllDialects();
};
