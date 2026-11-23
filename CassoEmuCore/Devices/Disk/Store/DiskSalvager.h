#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Denibblizer.h"

class  DiskBayTable;
struct DiskBay;
struct DiskStoreHooks;





//
//  What salvaging a bay would produce, and whether it is worth offering.
//  Read-only: producing one writes nothing and touches nothing.
//
//      isOffered      the disk is damaged AND has ordinary 16-sector
//                     structure. Both halves matter. A disk that is not
//                     damaged is not write-protected, so there is nothing to
//                     escape and a lossy copy would only lose data; a
//                     copy-protected disk has no standard sectors to salvage,
//                     and rebuilding it from sectors would destroy the very
//                     tracks that make it work.
//      totalSectors   sectors on the tracks this disk actually has -- 320 on a
//                     20-track disk, not a flat 560, or the count is a lie.
//      suggestedPath  "<name>.salvaged.woz" beside the original.
//
struct SalvageAssessment
{
    bool              isOffered     = false;
    int               totalSectors  = 0;
    DenibblizeReport  report;
    string            suggestedPath;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSalvager
//
//  Recovers the readable sectors of a damaged, ordinarily formatted disk
//  into a new WOZ. The original file is never written.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSalvager
{
public:
    DiskSalvager (DiskBayTable & bays, const DiskStoreHooks & hooks) : m_bays (bays), m_hooks (hooks) {}

    //  Writes nothing, so a caller can report the counts before making a copy.
    HRESULT         AssessSalvage      (int slot, int drive, SalvageAssessment & out);
    HRESULT         SalvageToFile      (int slot, int drive, const string & path,
                                        DenibblizeReport & report);

private:
    static HRESULT  DecodeForSalvage   (DiskBay & entry, vector<Byte> & outSectors,
                                        DenibblizeReport & report);
    static HRESULT  BuildSalvagedImage (DiskBay & entry, vector<Byte> & outBytes,
                                        DenibblizeReport & report);

    DiskBayTable          & m_bays;
    const DiskStoreHooks  & m_hooks;
};
