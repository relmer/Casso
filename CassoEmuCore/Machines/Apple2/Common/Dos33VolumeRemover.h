#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33VolumeInterfaces.h"

class Dos33SectorAccess;
class FilePath;
struct DeleteOutcome;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemover
//
//  Deletes a file from a DOS 3.3 volume into a new buffer: tombstones its
//  catalog entry and frees each sector that only that entry references.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33VolumeRemover : public IDos33FileRemoval
{
public:
    explicit Dos33VolumeRemover (const Dos33SectorAccess & access);

    HRESULT  Delete (const FilePath & path, vector<Byte> & outBuffer) const;

    //  Delete, plus an account of what it freed, what it left allocated, and
    //  the conditions that bound the answer.
    HRESULT  Delete (const FilePath  & path,
                     vector<Byte>    & outBuffer,
                     DeleteOutcome   & outOutcome) const override;

private:
    //  Turns the outcome of a delete into text a user can act on.
    static void  AppendDeleteWarnings (DeleteOutcome & inOutOutcome);

    const Dos33SectorAccess  & m_access;
    Dos33Catalog               m_catalog { m_access };
    Dos33FreeMap               m_freeMap { m_access };
    Dos33IntegrityScanner      m_scanner { m_access };
};
