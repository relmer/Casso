#pragma once

#include "Pch.h"

class  BayFileAccess;
class  DiskBayTable;
class  IBayRepoint;
struct DiskBay;
struct DiskStoreHooks;





//  How a flush failure can be put to the user, which differs by what is
//  still standing when it happens.
//
//  THE DISCRIMINATOR IS NOT WHICH FUNCTION FLUSHED. What matters is
//  whether there is a bay still holding the disk, a pump still running,
//  and a thread that can raise a file dialog -- and the three moments
//  below have different answers to those.
enum class FlushMoment
{
    //  The machine is running and the bay stays: a spindown, an explicit
    //  flush, a write-protect change, a machine switch, a soft reset. The
    //  question goes to the shell and is answered later, and nothing is
    //  lost while it stands.
    Running,

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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFlusher
//
//  Writes dirty mounted images back to their files, and writes a WOZ's
//  write-protect flag into its file.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFlusher
{
public:
    DiskFlusher (DiskBayTable         & bays,
                 const DiskStoreHooks & hooks,
                 const BayFileAccess  & files,
                 IBayRepoint          & repoint) :
        m_bays    (bays),
        m_hooks   (hooks),
        m_files   (files),
        m_repoint (repoint)
    {
    }

    HRESULT  Flush                (int slot, int drive);
    HRESULT  FlushAll             ();

    //  The flushes nobody asked for: the motor spinning down, a reset and a
    //  power cycle. They do nothing while the flush hold is on or a replay is
    //  running; every other flush (eject, machine switch, exit, an explicit
    //  save) writes the disk as it stands now.
    HRESULT  FlushAllUnlessHeld   ();

    //  The last flush of the process, where a blocking dialog is the only
    //  way to put a question to the user.
    HRESULT  FlushAllForShutdown  ();
    HRESULT  FlushEntry           (DiskBay & entry, FlushMoment moment);

    //  Patches the flag byte and header CRC rather than re-serializing the
    //  image. Flushes pending guest writes first.
    HRESULT  SetImageWriteProtect (int slot, int drive, bool writeProtected);

private:
    HRESULT  FlushEveryBay         (FlushMoment moment);
    void     ReportPreserveFailure (DiskBay & entry, FlushMoment moment,
                                    const string & attemptedPath, HRESULT hrKeep,
                                    const string & original);
    bool     RescueOnTheWayOut     (DiskBay & entry, const string & original);

    DiskBayTable          & m_bays;
    const DiskStoreHooks  & m_hooks;
    const BayFileAccess   & m_files;
    IBayRepoint           & m_repoint;
};
