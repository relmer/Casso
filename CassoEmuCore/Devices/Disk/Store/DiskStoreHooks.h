#pragma once

#include "Pch.h"

#include "Devices/Disk/BayChange.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/ExternalChangePolicy.h"
#include "Devices/Disk/ImageIdentity.h"

class IDiskFileIo;
class IImageWatcher;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreHooks
//
//  The seams and sinks a host installs on a DiskImageStore. Every store
//  component reads them from here; each is empty or null until set.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskStoreHooks
{
    using FlushSink      = std::function<HRESULT (const string &, const vector<Byte> &)>;
    using ImageReader    = std::function<HRESULT (const string &, vector<Byte> &)>;
    using IdentityReader = std::function<ImageIdentity (const string &)>;
    using ReportSink     = std::function<void (int slot, int drive, const ChangePrompt &)>;
    using AskSink        = std::function<bool (int slot, int drive, const ChangePrompt &)>;
    using RescueSink     = std::function<bool (const string & imagePath, string & outPath)>;
    using BayChangeSink  = std::function<void (int slot, int drive, BayChange change)>;
    using DecisionSink   = std::function<void (const string & path, ChangeAction action,
                                               bool guestCopyPreserved, const string & preservedPath)>;

    //  Test hooks: flushSink redirects serialized bytes to an in-memory
    //  capture buffer instead of the host filesystem, and imageReader does
    //  the same for reads, so the IFixtureProvider isolation contract is
    //  preserved even for a read-modify-write.
    FlushSink                  flushSink;
    ImageReader                imageReader;

    //  Reads a file's current identity: its size and write time.
    //
    //  A THIRD SEAM BESIDE THE OTHER TWO, and for their reason: a test that has
    //  redirected reads and writes into memory has no file to stat, so without
    //  it the staleness gate would compare the mount-time identity against a
    //  missing one and fail every flush.
    IdentityReader             identityReader;

    //  Where notification comes from. The shell builds the platform watcher and
    //  passes it in; REGISTERING AND DROPPING WATCHES IS THE STORE'S JOB,
    //  because mount-registers-a-watch is orchestration and orchestration is
    //  testable. Caller-owned and may be null, which is a session with no
    //  notification -- the check before every write still holds.
    IImageWatcher            * watcher = nullptr;

    //  Where "is somebody else writing this right now" is answered. Optional:
    //  without it a pick-up cannot be deferred for a third-party writer, and
    //  the quiet period is the only debounce.
    IDiskFileIo              * fileIo  = nullptr;

    //  Restarting the machine.
    //
    //  A CALLBACK RATHER THAN A CALL. A device-layer image store reaching
    //  machine lifecycle directly is a layering inversion; the decision stays
    //  here and the action belongs to the shell.
    std::function<void ()>     restartCallback;

    //  The machine as the user knows it, for the notices that mention it.
    //  "Apple //e" rather than "the Apple", which is not what is in front of
    //  them. Empty is allowed and the notices fall back to "the machine".
    string                     machineName;

    //  Showing a report that does not block the machine. Given the bay it is
    //  about and everything to draw.
    //
    //  THE SINK REPLACES, IT DOES NOT APPEND. A report is emitted for every
    //  change acted on, and a notice already up for that bay is re-worded
    //  rather than joined by a second one. Emitting only the first would keep
    //  one notice at the cost of it going stale: measured, a `reload` followed
    //  by a `reboot` left the bar still advising a reboot that had already
    //  happened.
    ReportSink                 reportSink;

    //  Putting a question to the user.
    //
    //  IT DOES NOT RETURN THE ANSWER. Asking happens on the thread that owns
    //  disk writes and answering on the one that owns the screen, so a sink
    //  that returned the answer would have to block the machine while the user
    //  read it. The answer comes back through ResolvePendingChange instead.
    //
    //  IT RETURNS WHETHER THE QUESTION REACHED SOMEWHERE IT CAN BE ANSWERED,
    //  which is a different fact and one only the sink knows. The shell posts
    //  the question to its own window and cannot do that before the window
    //  exists or when the queue is full; a bay marked as having a question
    //  outstanding that nobody ever saw is a bay nothing acts on again until
    //  the disk is ejected.
    //
    //  Null means nothing can answer, and a change that needs an answer stays
    //  pending rather than resolving itself by default.
    AskSink                    askSink;

    //  Asking where to put a disk, and not returning until it is answered.
    //
    //  THE ONE ROUTE LEFT AT SHUTDOWN, and the reason it is separate from
    //  AskSink. That one posts a message and reads the answer later, which
    //  needs a pump and a CPU thread; by the time the last flush runs there is
    //  neither. A file dialog is a blocking modal with a pump of its own, so
    //  it still works -- but only from a thread whose apartment suits it,
    //  which is why the store never calls this anywhere but ShuttingDown.
    //
    //  Returns false when the user declined or nothing could ask.
    RescueSink                 rescueSink;

    //  A bay's disk changed, and the shell should react: re-point the
    //  controller, re-apply write protection, log the debug event, and drive
    //  the door and its sounds.
    //
    //  ONE SIGNAL FOR EVERY PATH. A mount, a user eject, a pick-up, a file that
    //  vanished -- each ends here, so the door and the speaker are lit from one
    //  place rather than from every path that can move a disk. The store knows
    //  when a bay changed; what to do about it on screen is the shell's, and a
    //  fake sink lets a test assert the store fired without a drive on screen.
    //
    //  FIRES ON THE THREAD THAT OWNS DISK WRITES, where every path that can
    //  change a bay already runs. The handler does its controller and audio
    //  work there, exactly as the mount path did before this was central.
    BayChangeSink              bayChangeSink;

    //  What a settled change to a mounted image was decided to be, reported
    //  once per decision so a tool that stated the intent can be told.
    //
    //  `guestCopyPreserved` says the guest's unsaved writes were moved to
    //  `preservedPath` before the decision was taken again as an ordinary
    //  change. A decision that puts a question to the user is not reported;
    //  nothing has been decided yet.
    //
    //  FIRES ON THE THREAD THAT OWNS DISK WRITES, where the decision is taken.
    DecisionSink               decisionSink;

    //  Where "now" comes from, in milliseconds, so the quiet period can be
    //  swept in a test without waiting for one.
    std::function<int64_t ()>  clock;

    //  Where the wall-clock time in a preserved copy's NAME comes from.
    //
    //  A SECOND SEAM RATHER THAN A CONVERSION OF THE FIRST. The quiet period
    //  needs a monotonic count of milliseconds and a filename needs a calendar
    //  date; deriving one from the other would tie a timer to the user's clock
    //  changing under it.
    std::function<time_t ()>   timestamp;
};
