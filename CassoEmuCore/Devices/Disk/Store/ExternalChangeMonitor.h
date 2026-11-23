#pragma once

#include "Pch.h"

#include "Devices/Disk/ExternalChangePolicy.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"

class  BayFileAccess;
class  DiskBayTable;
struct DiskStoreHooks;





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeMonitor
//
//  Marks changes made to mounted images' files outside Casso as pending, and
//  picks the ChangeAction for each one once it settles.
//
////////////////////////////////////////////////////////////////////////////////

class ExternalChangeMonitor : public IImageChangeNotes
{
public:
    ExternalChangeMonitor (DiskBayTable          & bays,
                           const DiskStoreHooks  & hooks,
                           const BayFileAccess   & files,
                           IChangeActions        & actions) :
        m_bays    (bays),
        m_hooks   (hooks),
        m_files   (files),
        m_actions (actions)
    {
    }

    //  Called from any thread. Marks the change as pending and returns.
    void     NoteExternalChange      (const string & path, ExternalChangeIntent intent,
                                      bool fromCassoExplorer = false) override;

    //  Called on the CPU thread at a moment with no disk operation in flight.
    void     ApplyPendingReload      ();

private:
    void     ApplyPendingReloadToBay (int slot, int drive);
    int64_t  GetNowMs                () const;

    DiskBayTable           & m_bays;
    const DiskStoreHooks   & m_hooks;
    const BayFileAccess    & m_files;
    IChangeActions         & m_actions;
};
