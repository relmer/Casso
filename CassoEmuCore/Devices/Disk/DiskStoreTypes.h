#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "MountedImageState.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreTypes
//
//  A bay of the disk store, and how a save that failed to write it is
//  reported. The store and its components use these.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskBayEntry
{
    unique_ptr<DiskImage>  image;
    string                 path;
    DiskFormat             format  = DiskFormat::Dsk;
    bool                   mounted = false;

    //  Whether salvage applies to this image, decided once at mount.
    //  Answering it needs a full decode for a damaged disk, which is far
    //  too much work to repeat every time a menu is drawn.
    bool                   salvageOffered = false;

    //  What this bay knows about the file behind it. Set at mount, cleared
    //  at eject, refreshed after every commit this store makes.
    MountedImageState      sharedState;

    //  Which bay this is.
    //
    //  CARRIED ON THE ENTRY BECAUSE FlushEntry NEEDS IT AND HAS ONLY THIS.
    //  Every message about a disk names the drive it is in, and threading
    //  a pair of ints through five call sites to reach one function is a
    //  worse answer than the entry knowing where it lives.
    int                    slot    = 0;
    int                    drive   = 0;

    //  For a kept disk, the position at which it left its bay.
    uint64_t               retiredAt = 0;
};

//  How a flush failure can be put to the user, which differs by what is
//  still standing when it happens.
//
//  THE DISCRIMINATOR IS NOT WHICH FUNCTION FLUSHED. What matters is
//  whether there is a bay still holding the disk, a pump still running,
//  and a thread that can raise a file dialog -- and the three moments
//  below have different answers to those.
enum class FlushMoment
{
    //  The machine is running, the bay stays, and a person asked for the
    //  save of one bay: an explicit flush, a write-protect change. The
    //  question goes to the shell and is answered later, and nothing is
    //  lost while it stands.
    Running,

    //  The machine is running, the bay stays, and nobody asked: every bay
    //  is saved at a spindown, a machine switch or a soft reset. The
    //  question goes the same way as at Running.
    Background,

    //  The save before a mount over the bay. Only a first recovery copy is
    //  reported; a save after which writes are in no file declines the
    //  mount, and the mount's text reports it.
    Replacing,

    //  The disk is coming out. The question goes the same way, but the
    //  eject waits for it: emptying the bay is what would destroy a disk
    //  whose copy could not be written, so it does not happen until the
    //  user has said which way they want it.
    Ejecting,

    //  The process is on its way out. There is no pump left to deliver a
    //  posted question and no CPU thread to act on the answer -- but this
    //  runs on the UI thread with its apartment still up, so a blocking
    //  file dialog still works. That is the one route left, and asking
    //  through it is synchronous.
    ShuttingDown,
};

//  What a save that failed to write the bay's file produced.
struct FlushOutcome
{
    HRESULT  saveError        = S_OK;
    bool     isUnserializable = false;
    string   recoveryPath;
    HRESULT  recoveryError    = S_OK;

    //  At Replacing, the file another program changed and where the
    //  guest's version went instead, for the mount to report.
    string   movedFrom;
    string   movedTo;
};

//  Where a save's result goes, on the thread with the store's token: its
//  HRESULT, and what a failed save produced.
using SaveDone = std::function<void (HRESULT hr, const FlushOutcome & outcome)>;





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
//      mediaId        the medium assessed; the copy is written only while the
//                     bay still holds it.
//      sourcePath     the file that medium came from.
//
struct SalvageAssessment
{
    bool              isOffered     = false;
    int               totalSectors  = 0;
    DenibblizeReport  report;
    string            suggestedPath;
    uint64_t          mediaId       = 0;
    string            sourcePath;
};
