#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"
#include "Shell/DiskMru.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore
//
//  The folders the Casso root lists, shared between the emulator and the
//  browser through one small file beside the user preferences.
//
//  A FILE OF ITS OWN, BECAUSE THE PREFERENCES FILE CANNOT BE SHARED FOR
//  WRITES. A running emulator rewrites that file whole from memory on its
//  next save, so anything a second process wrote there is lost. This file is
//  read on demand and replaced whole by whichever process appends, under a
//  named mutex so two writers cannot interleave.
//
//  NOTHING IS PRUNED. A folder that no longer exists stays listed and dimmed
//  until the user removes it; a listing that quietly dropped a folder would
//  look identical to one that never had it.
//
////////////////////////////////////////////////////////////////////////////////

class KnownFolderStore
{
public:
    struct Entry
    {
        std::wstring  path;
        int64_t       lastUsedUnix = 0;
    };

    //  `crossProcessLock` names the mutex the read-merge-replace runs under;
    //  a test on an in-memory file system turns it off, since there is no
    //  second process to hold off and the lock is the one system object the
    //  store would otherwise touch.
    KnownFolderStore (IFileSystem & fs, const std::wstring & baseDir, bool crossProcessLock = true);

    //  The list as it stands. An absent file is an empty list, not an error.
    HRESULT  Load (std::vector<Entry> & outEntries) const;

    //  Adds or re-stamps one folder; paths compare case-insensitively.
    HRESULT  Append (const std::wstring & folder, int64_t nowUnix);
    HRESULT  Remove (const std::wstring & folder);

    //  First-run seeding: the distinct folders of the recent-disks list, only
    //  when no file exists yet. Afterwards the two lists diverge.
    HRESULT  SeedFromMru (const std::vector<DiskMru::Entry> & mru, int64_t nowUnix);

    bool  Exists () const;

    //  The folders a disk picker scans: every known folder, then each of the
    //  recent-disks list's folders not already among them, so the picker
    //  never loses a folder it showed before the list existed.
    static std::vector<std::filesystem::path>  MergePickerFolders (const std::vector<Entry>          & known,
                                                                   const std::vector<DiskMru::Entry> & mru);

    //  Loads the list for a picker, seeding it from the recent disks on first
    //  use. A file that cannot be read yields the recent disks' folders alone.
    static std::vector<std::filesystem::path>  LoadPickerFolders (IFileSystem                       & fs,
                                                                  const std::wstring                & baseDir,
                                                                  const std::vector<DiskMru::Entry> & mru,
                                                                  int64_t                             nowUnix);

    //  The folders the browser's Casso root lists: the ones recorded, or the
    //  emulator's own disk folder when nothing has been recorded yet. The
    //  emulator records a folder the first time it opens a disk from one, so
    //  a machine where that has not happened would otherwise show a root with
    //  nothing under it and no way in.
    static std::vector<std::wstring>  ListRootFolders (IFileSystem        & fs,
                                                       const std::wstring & baseDir,
                                                       const std::vector<Entry> & entries);

    static std::wstring  GetFilePath (const std::wstring & baseDir);
    static bool          ArePathsEqual (const std::wstring & a, const std::wstring & b);

    static constexpr const wchar_t *  kFileName    = L"KnownFolders.json";
    static constexpr const wchar_t *  kDisksFolder = L"Disks";
    static constexpr const wchar_t *  kMutexName = L"Local\\CassoKnownFolders";

private:
    static std::wstring  JoinBase (const std::wstring & baseDir, const std::wstring & name);

    HRESULT  ReadEntries  (std::vector<Entry> & outEntries) const;
    HRESULT  WriteEntries (const std::vector<Entry> & entries) const;

    HANDLE   AcquireLock () const;
    void     ReleaseLock (HANDLE lock) const;

    static constexpr DWORD  kLockTimeoutMs = 2000;

    IFileSystem   & m_fs;
    std::wstring    m_filePath;
    bool            m_crossProcessLock;
};
