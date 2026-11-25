#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"

class Dos33SectorAccess;
class FilePath;
class IDos33FileRemoval;
struct FileAttributes;
struct FilePayload;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeWriter
//
//  Placement on a DOS 3.3 volume: files added and replaced, renamed, and
//  their attributes changed. Every result is computed into a new buffer and
//  checked before it is returned to the caller.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33VolumeWriter
{
public:
    Dos33VolumeWriter (const Dos33SectorAccess & access, const IDos33FileRemoval & removal);

    HRESULT  Write         (const FilePath     & path,
                            const FilePayload  & payload,
                            vector<Byte>       & outBuffer) const;

    HRESULT  Rename        (const FilePath     & from,
                            const std::string  & to,
                            vector<Byte>       & outBuffer) const;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const;

private:
    //  Lays down a file whose name is not in the catalog, over `source`.
    //  Replacement stages the removal into a working buffer and calls this
    //  over that buffer, so one code path places every file and a replacement
    //  cannot half-happen.
    HRESULT  AddFile (const Dos33SectorAccess  & source,
                      const vector<Byte>       & nameBytes,
                      Byte                       typeByte,
                      const vector<Byte>       & stored,
                      vector<Byte>             & outBuffer) const;

    const Dos33SectorAccess  & m_access;
    const IDos33FileRemoval  & m_removal;
    Dos33Catalog               m_catalog { m_access };
    Dos33IntegrityScanner      m_scanner { m_access };
};
