#pragma once

#include "Pch.h"

#include "Update/InstallTypeDetector.h"
#include "Update/ReleaseInfo.h"
#include "Update/ReleaseNotesExtractor.h"
#include "Update/UpdateFailure.h"
#include "Update/UpdateSchedule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateResultKind
//
//  Which piece of update work an UpdateResult reports on.
//
//    Check          a check for the latest release finished
//    CheckSkipped   an automatic check did not run, because another Casso
//                   holds the check lock; that one records what it finds
//    Notes          the release notes for a release were fetched
//    ReadyToDeploy  an MSIX bundle is downloaded and verified; the UI thread
//                   flushes disks and settings, then starts the deploy
//    Applied        an update finished: the zip copy was replaced and the
//                   new Casso was launched, or the MSIX deploy returned;
//                   or, with isPending, it waits for Casso to close
//    Image          one release-notes image was fetched and decoded, or not
//
////////////////////////////////////////////////////////////////////////////////

enum class UpdateResultKind
{
    Check,
    CheckSkipped,
    Notes,
    ReadyToDeploy,
    Applied,
    Image,
};





////////////////////////////////////////////////////////////////////////////////
//
//  NotesImage
//
//  A decoded release-notes image, premultiplied BGRA as the text renderer's
//  bitmap draw takes it. Shared, since the session cache and the dialog both
//  hold it.
//
////////////////////////////////////////////////////////////////////////////////

struct NotesImage
{
    int                    width  = 0;
    int                    height = 0;
    std::vector<uint32_t>  bgraPremul;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateResult
//
//  What the update worker hands the UI thread. `failure` is None on success;
//  `wasCanceled` marks work the user stopped, which is not a failure to
//  report. `detail` is the network layer's own text, for a failure report.
//
////////////////////////////////////////////////////////////////////////////////

struct UpdateResult
{
    UpdateResultKind    kind         = UpdateResultKind::Check;
    UpdateCheckTrigger  trigger      = UpdateCheckTrigger::Automatic;
    UpdateFailure       failure      = UpdateFailure::None;
    bool                wasCanceled  = false;
    std::string         detail;

    ReleaseInfo         release;
    InstallType         installType  = InstallType::Unknown;
    bool                isNewer      = false;
    bool                isOffered    = false;
    std::int64_t        checkedAtUtc = 0;

    ReleaseNotes        notes;
    std::wstring        bundlePath;

    //  An update to apply when Casso closes: a zip copy's staged files, or a
    //  bundle Windows registers once Casso exits.
    bool                      isPending    = false;
    std::wstring              installDir;
    std::vector<std::string>  stagedPaths;

    std::string                         imageSrc;
    std::shared_ptr<const NotesImage>   image;
};
