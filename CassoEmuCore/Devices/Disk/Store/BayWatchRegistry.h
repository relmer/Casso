#pragma once

#include "Pch.h"

class  DiskBayTable;
class  IImageChangeNotes;
struct DiskStoreHooks;





////////////////////////////////////////////////////////////////////////////////
//
//  BayWatchRegistry
//
//  Registers and drops the watch on each mounted bay's directory. A
//  directory is dropped only when no other bay has a disk from it.
//
////////////////////////////////////////////////////////////////////////////////

class BayWatchRegistry
{
public:
    BayWatchRegistry (DiskBayTable         & bays,
                      const DiskStoreHooks & hooks,
                      IImageChangeNotes    & notes) :
        m_bays  (bays),
        m_hooks (hooks),
        m_notes (notes)
    {
    }

    void  BeginWatching (int slot, int drive);
    void  EndWatching   (int slot, int drive);

private:
    DiskBayTable          & m_bays;
    const DiskStoreHooks  & m_hooks;
    IImageChangeNotes     & m_notes;
};