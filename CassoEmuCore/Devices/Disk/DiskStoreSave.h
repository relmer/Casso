#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"
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

    HRESULT       FlushEntry        (Entry & entry, FlushMoment moment);
    HRESULT       FlushEntry        (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome);

    //  FlushEntry's steps: moving the guest's version to a separate file when
    //  another program changed the bay's file, telling the user a save
    //  failed, and recording the file the save committed.
    HRESULT       PreserveConflictingCopy (Entry & entry, FlushMoment moment, bool & isPreserved, FlushOutcome & outOutcome);
    void          ReportSaveFailure       (Entry & entry, FlushMoment moment, const FlushOutcome & outcome);
    void          RecordCommittedFile     (Entry & entry, const vector<Byte> & savedBytes);

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
    HRESULT        TryWriteRecoveryImage  (Entry & entry, string & outPath);

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
    DiskImageStore &  m_store;
};
