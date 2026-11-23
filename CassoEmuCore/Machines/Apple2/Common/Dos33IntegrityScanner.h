#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"

class Dos33SectorAccess;
class VolumeIntegrityReport;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScanner
//
//  The integrity pass over a DOS 3.3 volume: every sector on the catalog's
//  chains, compared against the VTOC free bitmap. Also the check that every
//  computed write and delete runs over its result before the result is
//  returned.
//
//  A unit is track * 16 + sector, so a 35-track disk has 560 units.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33IntegrityScanner
{
public:
    explicit Dos33IntegrityScanner (const Dos33SectorAccess & access);

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const;

    //  The self-check every computed write and delete runs over its output,
    //  and the ONLY path from a computed buffer to a caller. A buffer that
    //  disagrees with itself in a way its input did not is an error; any other
    //  buffer is copied to `outBuffer`.
    //
    //  The check and the copy are one operation on purpose. A check BESIDE the
    //  assignment can be deleted while the assignment stays, and nothing
    //  observable changes; no input makes correct code produce a bad buffer,
    //  so no test would fail. Routing the copy through the check means
    //  removing it cannot be silent.
    //
    //  A rejection means OUR writer produced a bad image, not that the user
    //  asked for something impossible, so it returns E_UNEXPECTED and asserts:
    //  the one condition in this layer that is a defect rather than a verdict.
    //
    //  Public because a rejection is only reachable with a result that is
    //  deliberately wrong.
    static HRESULT  HandBackVerifiedResult (const VolumeIntegrityReport  & before,
                                            const vector<Byte>           & result,
                                            vector<Byte>                 & outBuffer);

private:
    //  Credits the tracks DOS occupies and the catalog track to the reserved
    //  volume owner. No catalog entry lists them, and a report that left them
    //  unowned would call every healthy volume inconsistent.
    void  ClaimVolumeStructures (VolumeIntegrityReport & outReport) const;

    const Dos33SectorAccess  & m_access;
    Dos33Catalog               m_catalog { m_access };
    Dos33FileChain             m_chain   { m_access };
    Dos33FreeMap               m_freeMap { m_access };
};
