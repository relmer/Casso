#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"
#include "MountDiagnosis.h"

class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMount
//
//  The disk store's mounts: from a file or from bytes, over an empty bay or
//  over a disk, and a machine state's disk put back. The read, the save of
//  the disk in the bay and the stat of a remounted file run on the store's
//  I/O queue, and each step continues in the one before's completion.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreMount
{
public:
    using Entry     = DiskBayEntry;
    using MountDone = std::function<void (HRESULT hr, const MountDiagnosis & diagnosis)>;

    explicit DiskStoreMount (DiskImageStore & store) : m_store (store) {}

    //  DiskImageStore's mounts.
    HRESULT       Mount             (int slot, int drive, const string & path);
    HRESULT       Mount             (int slot, int drive, const string & path,
                                     MountDiagnosis & outDiagnosis);
    void          Mount             (int slot, int drive, const string & path, MountDone done);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes,
                                     MountDiagnosis & outDiagnosis);
    HRESULT       MountRestored     (int slot, int drive, const string & path,
                                     DiskFormat fmt, const vector<Byte> & bytes);
    //  A mount's state from step to step while its file work is posted.
    struct MountJob
    {
        int             slot       = 0;
        int             drive      = 0;
        string          path;
        DiskFormat      format     = DiskFormat::Dsk;
        bool            isFromFile = false;
        bool            isSameFile = false;
        vector<Byte>    bytes;
        ImageIdentity   identity;
        MountDiagnosis  diagnosis;
        FlushOutcome    outcome;
        HRESULT         result     = S_OK;
        MountDone       done;
    };

    //  A mount over a bay, in its order: load the new image, save the disk in
    //  the bay, and swap them only when the save left no write outside a file.
    //  LoadImage builds the image without touching the bay; SaveBeforeReplace
    //  saves and DecideReplace declines with UnsavedWrites; SwapInImage
    //  retires the old disk and seats the new one; ReportReplaceOutcome
    //  reports a guest copy the save moved.
    static HRESULT  LoadImage          (DiskFormat fmt, const vector<Byte> & bytes, const string & path,
                                        unique_ptr<DiskImage> & outImage, MountDiagnosis & outDiagnosis);
    void            SaveBeforeReplace  (const shared_ptr<MountJob> & job, std::function<void (HRESULT)> done);
    HRESULT         DecideReplace      (MountJob & job);
    void            ReplaceBay         (const shared_ptr<MountJob> & job, std::function<void (HRESULT)> done);
    void            NoteKeptDisk       (MountJob & job, HRESULT hr);
    void            SwapInImage        (Entry & entry, unique_ptr<DiskImage> loaded,
                                        const string & path, DiskFormat fmt);
    void            ReportReplaceOutcome (int slot, int drive, const string & newPath, HRESULT hr,
                                          const FlushOutcome & outcome);

    //  Mount's steps after the checks: the stat of a remounted file, the read,
    //  and the end of a mount from a file or from bytes.
    void            CheckSameFile        (const shared_ptr<MountJob> & job, HRESULT hr);
    void            ReadMountFile        (const shared_ptr<MountJob> & job);
    void            FinishMountFromFile  (const shared_ptr<MountJob> & job, HRESULT hr);
    void            FinishMountFromBytes (const shared_ptr<MountJob> & job, HRESULT hr);
    void            FinishMount          (const shared_ptr<MountJob> & job, HRESULT hr);

private:
    DiskImageStore &  m_store;
};
