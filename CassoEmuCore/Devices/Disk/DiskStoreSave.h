#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"
#include "DiskFileOps.h"
#include "DurableCommit.h"
#include "PreservedCopy.h"

class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave
//
//  The disk store's saves: a bay's image to its file, the copy beside a
//  file another program changed, the recovery copy of an image that does
//  not serialize, and the reports and questions after a failed save.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreSave
{
public:
    using Entry = DiskBayEntry;

    explicit DiskStoreSave (DiskImageStore & store) : m_store (store) {}

    //  Where a file operation's result goes: its HRESULT and the path written
    //  to or attempted.
    using PathDone = std::function<void (HRESULT hr, const string & path)>;

    //  Saves a bay's disk through the store's I/O queue, and calls done on
    //  the store's thread when the save is over, or at once when there is
    //  nothing to save. A save requested while one is in flight for the same
    //  disk starts when that one is over.
    void          Save              (Entry & entry, FlushMoment moment, SaveDone done);

    //  The same save, for a caller that uses the result before it returns:
    //  with a queue that runs work on another thread, these wait for the
    //  save's completion.
    HRESULT       FlushEntry        (Entry & entry, FlushMoment moment);
    HRESULT       FlushEntry        (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome);

    //  Reports a failed save to the user.
    void          ReportSaveFailure (Entry & entry, FlushMoment moment, const FlushOutcome & outcome);

    //  Tells the user the guest's version could not be written beside the
    //  file that displaced it -- as a question where one can still be
    //  answered, and as a notice where it cannot.
    void          ReportPreserveFailure (Entry & entry, FlushMoment moment,
                                         const string & attemptedPath, HRESULT hrKeep,
                                         const string & original);

    //  Asks the shell where to put a disk on the way out and writes it there,
    //  without returning until it is done. Only ever called at ShuttingDown.
    //  A disk whose format cannot store its writes is offered a .woz name, and
    //  any disk that does not serialize natively is written as WOZ.
    bool          RescueOnTheWayOut (Entry & entry, const string & original, bool isUnserializable);

    //  The eject's question for a save to the disk's file that failed with no
    //  recovery copy. Returns whether the question was delivered, in which
    //  case the eject waits for its answer.
    bool          AskBeforeEject (Entry & entry, const FlushOutcome & outcome);

    //  Writes the bay's image as WOZ at `path` with a .woz extension, and
    //  makes it the bay's recovery copy, marked as reported. The bay stays on
    //  its file, whose format cannot change.
    HRESULT       WriteWozCopy (Entry & entry, const string & path, string & outPath);

    //  A Save as answer: a native copy at `savePath`, with the bay moved onto
    //  it, or, finishing an eject, a WOZ copy when the image does not
    //  serialize natively. outPath is the file written or attempted.
    HRESULT       SaveAnswerCopy (Entry & entry, const string & savePath, bool isEjecting, string & outPath);

    // Preserves an image that could not be serialized to its own format,
    // beside the original and losslessly. Leaves the original untouched.
    void           TryWriteRecoveryImage  (Entry & entry, PathDone done);

    // TryWriteRecoveryImage's steps: whether the copy this mount wrote is as
    // it was written, and a new copy at the first free recovery name.
    bool           IsRecoveryCopyIntact   (const Entry & entry);
    HRESULT        WriteNewRecoveryImage  (const Entry & entry, const vector<Byte> & wozBytes, string & outPath);

    //  Writes what the bay currently holds to a preserved copy beside the
    //  original, and reports where it went.
    //
    //  SERIALIZED FROM THE MOUNTED IMAGE rather than copied from the file: the
    //  point of preserving it is that the file no longer holds this version.
    HRESULT        SaveLoadedImage (Entry & entry, string & outPath);
    void           SaveLoadedImage (Entry & entry, const string & path, PathDone done);

    //  A preserved-copy path nothing is sitting at yet.
    //
    //  THE COLLISION LOOP IS NOT OPTIONAL. Two conflicts on one image inside a
    //  second is exactly what a build loop produces, and a one-second stamp
    //  cannot keep the accumulate-rather-than-overwrite promise on its own.
    HRESULT        FindFreePreservedPath (const string & imagePath, string & outPath) const;

    //  Whether something is already at `path`, through the file seam when one
    //  is installed and through the filesystem when none is.
    bool           DoesPathExist (const string & path) const;

    //  Puts bytes at a path: through the flush sink when one is installed, and
    //  otherwise through DurableCommit over the file seam, or over a local
    //  Win32DiskFileIo when no seam is installed. Every write of an image, a
    //  salvaged copy or a preserved copy goes through here.
    HRESULT        CommitFile (const string & path, const vector<Byte> & bytes, CommitMode mode);

    //  Whether a bay's guest writes are in a file, and whether its recovery
    //  copy is current and as this mount wrote it.
    bool            AreWritesSaved     (Entry & entry);
    bool            IsRecoveryCurrent  (Entry & entry);

private:
    //  How long a caller that uses a result waits for it.
    static constexpr DWORD  s_kSyncIoTimeoutMs = 30000;

    //  A result a waiting caller reads once the completion has run.
    struct IoWait
    {
        HRESULT       hr     = S_OK;
        FlushOutcome  outcome;
        string        path;
        bool          isDone = false;
    };

    //  One save's state from the post to the completion. The I/O thread
    //  reads the request half and writes the result half; done is the store
    //  thread's alone.
    struct SaveJob
    {
        int                slot        = 0;
        int                drive       = 0;
        uint64_t           imageId     = 0;
        FlushMoment        moment      = FlushMoment::Background;
        SaveDone           done;
        string             path;
        ImageIdentity      recorded;
        bool               hasTarget   = false;
        HRESULT            hrSerialize = S_OK;
        vector<Byte>       bytes;
        vector<uint64_t>   generations;

        bool               isChanged   = false;
        HRESULT            hrCommit    = S_OK;
        ImageIdentity      after;
    };

    //  A recovery copy's state, in the same halves.
    struct RecoveryJob
    {
        int                slot         = 0;
        int                drive        = 0;
        uint64_t           imageId      = 0;
        PathDone           done;
        string             imagePath;
        string             copyPath;
        ImageIdentity      copyIdentity;
        bool               isElsewhere  = false;
        bool               isCurrent    = false;
        bool               isNameFixed  = false;
        HRESULT            hrWoz        = S_OK;
        vector<Byte>       wozBytes;
        vector<uint64_t>   generations;

        HRESULT            hr           = S_OK;
        bool               isWritten    = false;
        string             outPath;
        ImageIdentity      after;
    };

    //  A save requested while one was in flight for the same disk.
    struct SaveRequest
    {
        FlushMoment  moment = FlushMoment::Background;
        SaveDone     done;
    };

    //  Save's steps: whether the bay has anything to save, posting the work,
    //  the work, and the completion, which goes on to a preserved copy when
    //  the file had changed, to a recovery copy when the image would not
    //  serialize, and otherwise applies the save to the bay.
    bool           IsSaveNeeded        (Entry & entry);
    void           StartSave           (Entry & entry, FlushMoment moment, SaveDone done);
    static void    RunSaveWork         (SaveJob & job, const DiskFileOps & ops);
    void           CompleteSave        (const shared_ptr<SaveJob> & job);
    void           RecordSave          (Entry & entry, const SaveJob & job);
    void           PreserveAfterChange (Entry & entry, const shared_ptr<SaveJob> & job);
    void           FinishPreserve      (const shared_ptr<SaveJob> & job, const string & original, bool isAsked,
                                        HRESULT hrKeep, const string & triedPath);
    void           SaveUnserializable  (Entry & entry, const shared_ptr<SaveJob> & job);
    static void    RunRecoveryWork     (RecoveryJob & job, const DiskFileOps & ops);
    void           CompleteRecovery    (const shared_ptr<RecoveryJob> & job);

    //  Calls the save's done function, then starts the saves requested meanwhile.
    void           FinishSave          (uint64_t imageId, HRESULT hr, const FlushOutcome & outcome, SaveDone & done);

    //  The bay with this disk in it, or null when the disk has left it.
    Entry *        FindBay             (int slot, int drive, uint64_t imageId);
    Entry *        FindImage           (uint64_t imageId);

    //  Copies the fields a save set into an outcome a caller passes in.
    static void    MergeOutcome        (const FlushOutcome & from, FlushOutcome & into);

    //  Waits, for a caller that uses the result, until wait is done.
    HRESULT        WaitForIo           (const IoWait & wait);

    DiskImageStore &                                m_store;

    //  For each disk with a save in flight, the saves requested meanwhile.
    std::map<uint64_t, std::vector<SaveRequest>>    m_waiting;
};
