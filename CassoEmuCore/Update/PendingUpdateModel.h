#pragma once

#include "Pch.h"

#include "Update/ReleaseVersion.h"
#include "Update/UpdateFailure.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PendingUpdate
//
//  An update applied when Casso closes, as the preferences hold it across
//  the exit: the version it installs, how ("zip" or "msix"), and the
//  failure of a zip swap at exit. An empty version means none is waiting.
//
////////////////////////////////////////////////////////////////////////////////

struct PendingUpdate
{
    static constexpr LPCSTR  kpszZip  = "zip";
    static constexpr LPCSTR  kpszMsix = "msix";

    std::string    version;
    std::string    kind;
    UpdateFailure  failure = UpdateFailure::None;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PendingLaunchAction
//
//  What a launch does about a pending update:
//
//    showUpdated     say "Casso was updated to version X."
//    removeOldFiles  delete the zip copy's .update-old, left by the swap
//    discardStaged   delete a zip copy's .update-new that was never swapped
//                    in, because Casso ended without closing normally
//    failure         report a swap that failed at exit
//    clearPending    forget the pending update
//
////////////////////////////////////////////////////////////////////////////////

struct PendingLaunchAction
{
    bool           showUpdated    = false;
    bool           removeOldFiles = false;
    bool           discardStaged  = false;
    UpdateFailure  failure        = UpdateFailure::None;
    bool           clearPending   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PendingUpdateModel
//
//  The decisions around an update applied when Casso closes, as pure
//  functions.
//
////////////////////////////////////////////////////////////////////////////////

class PendingUpdateModel
{
public:
    static PendingLaunchAction  DecideAtLaunch (const PendingUpdate & pending, const ReleaseVersion & running);
    static bool                 IsZip          (const PendingUpdate & pending) { return pending.kind == PendingUpdate::kpszZip; }
};
