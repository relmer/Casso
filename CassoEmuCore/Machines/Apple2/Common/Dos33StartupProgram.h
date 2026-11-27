#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"

class Dos33SectorAccess;
class FilePath;
struct VolumeListing;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33StartupProgram
//
//  The program a booted DOS 3.3 runs: its file name, stored in the DOS image
//  on track 1, and the rule for which files can be one.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33StartupProgram
{
public:
    explicit Dos33StartupProgram (const Dos33SectorAccess & access);

    HRESULT      SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const;

    //  Whether the tracks DOS occupies have anything at all in them. A volume
    //  formatted without an operating system reserves them, and they are
    //  blank, and a greeting file name patched into blank space would report
    //  success for something no boot could ever read. Public because a
    //  listing reports the same fact about the disk.
    bool         HasDosImage          () const;

    //
    //  Whether a booting DOS 3.3 would actually RUN this file.
    //
    //  Measured against the stock master rather than reasoned about: with a
    //  binary set as the greeting the machine boots and the program never
    //  runs, because the command DOS issues at boot is RUN. A file RUN does
    //  not understand is a greeting in name only.
    //
    //  A file name that is not on the volume returns true. The error for that
    //  comes from the lookup, and one error per problem is the rule.
    //
    //  Both routes to a startup program use this: the disk command that sets
    //  one and the assembler that writes one. A second copy is how two routes
    //  to one idea come to accept different things.
    //
    static bool  IsRunnableAsGreeting (const VolumeListing & listing, const std::string & name);

private:
    //  The location of the file name of the program a booted DOS runs: inside
    //  the DOS image on track 1, NOT in the catalog. Thirty bytes of high
    //  ASCII padded with high-ASCII spaces, the same width as a catalog name
    //  field.
    //
    //  Measured rather than recalled. A scan of the stock System Master for
    //  the high-ASCII form of its greeting file name finds exactly two
    //  occurrences on the whole disk: this one and the file's catalog entry.
    //  The field's end is not visible in the data, because the bytes after it
    //  are pad bytes too, so the width comes from the catalog name field,
    //  which the DOS code that reads it shares.
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;

    const Dos33SectorAccess  & m_access;
    Dos33Catalog               m_catalog { m_access };
    Dos33IntegrityScanner      m_scanner { m_access };
};
