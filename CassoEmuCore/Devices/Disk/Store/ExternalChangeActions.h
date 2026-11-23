#pragma once

#include "Pch.h"

#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/ExternalChangePolicy.h"

class  BayFileAccess;
class  DiskBayTable;
class  IBayLifecycle;
class  IBayRepoint;
struct DiskStoreHooks;





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeActions
//
//  Applies a ChangeAction to a bay whose file changed outside Casso. The
//  action comes from ExternalChangePolicy or from the user's answer to a
//  prompt.
//
////////////////////////////////////////////////////////////////////////////////

class ExternalChangeActions
{
public:
    ExternalChangeActions (DiskBayTable         & bays,
                           const DiskStoreHooks & hooks,
                           const BayFileAccess  & files,
                           IBayRepoint          & repoint,
                           IBayLifecycle        & lifecycle) :
        m_bays      (bays),
        m_hooks     (hooks),
        m_files     (files),
        m_repoint   (repoint),
        m_lifecycle (lifecycle)
    {
    }

    //  Applies the user's answer to a prompt. Runs on the thread that owns
    //  disk writes. `savePath` applies only to an answer of PreserveCopy.
    void  ResolvePendingChange (int slot, int drive, ChangeAction chosen,
                                const string & savePath = string());

    //  `author` is passed in, because by this point the bay's pending record
    //  can be for a later write.
    void  CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                const vector<Byte> & bytes, ChangeAuthor author);

private:
    DiskBayTable          & m_bays;
    const DiskStoreHooks  & m_hooks;
    const BayFileAccess   & m_files;
    IBayRepoint           & m_repoint;
    IBayLifecycle         & m_lifecycle;
};
