#include "Pch.h"

#include "Update/PendingUpdateModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PendingUpdateModel::DecideAtLaunch
//
//  Running the version that was pending means it was applied: say so, and
//  for a zip copy remove the files the swap set aside. A recorded failure
//  is reported once. Otherwise a zip update never reached its swap --
//  Casso ended without closing normally -- so its staged files go. An MSIX
//  update stays pending while the running version is older, since Windows
//  registers it only once no Casso is running; a running version past it
//  means something newer came in, and the record goes.
//
////////////////////////////////////////////////////////////////////////////////

PendingLaunchAction PendingUpdateModel::DecideAtLaunch (const PendingUpdate & pending, const ReleaseVersion & running)
{
    PendingLaunchAction  action;
    ReleaseVersion       target;
    bool                 isKnown = !pending.version.empty() && ReleaseVersion::TryParse (pending.version, target);
    bool                 isZip   = IsZip (pending);



    if (!isKnown)
    {
        action.clearPending = !pending.version.empty();
    }
    else if (pending.failure != UpdateFailure::None)
    {
        action.failure       = pending.failure;
        action.discardStaged = isZip;
        action.clearPending  = true;
    }
    else if (running == target)
    {
        action.showUpdated    = true;
        action.removeOldFiles = isZip;
        action.clearPending   = true;
    }
    else if (isZip)
    {
        action.discardStaged = true;
        action.clearPending  = true;
    }
    else
    {
        action.clearPending = target < running;
    }

    return action;
}
