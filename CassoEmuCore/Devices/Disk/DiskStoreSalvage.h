#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSalvage
//
//  A damaged disk's sectors, read back and written to a separate WOZ.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreSalvage
{
public:
    using Entry = DiskBayEntry;

    explicit DiskStoreSalvage (DiskImageStore & store) : m_store (store) {}

    //  DiskImageStore's AssessSalvage, IsSalvageOffered and SalvageToFile.
    HRESULT        AssessSalvage    (int slot, int drive, SalvageAssessment & out);
    bool           IsSalvageOffered (int slot, int drive) const;
    HRESULT        SalvageToFile    (int slot, int drive, uint64_t mediaId, const string & path,
                                     DenibblizeReport & report);

    // Recover what can be recovered. Decode only -- this is all the counts
    // need, and it is the half AssessSalvage can afford to run.
    HRESULT       DecodeForSalvage (Entry & entry, vector<Byte> & outSectors,
                                    DenibblizeReport & report);

    // Decode, then rebuild the result as a WOZ. Only the write path needs the
    // rebuild, so only the write path pays for it.
    HRESULT       BuildSalvagedImage (Entry & entry, vector<Byte> & outBytes,
                                      DenibblizeReport & report);

private:
    DiskImageStore &  m_store;
};
