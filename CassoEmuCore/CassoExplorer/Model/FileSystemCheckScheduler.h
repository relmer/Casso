#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckKey / FileSystemCheckOutcome
//
//  An image to check, by path, length and modified time, and the result of
//  the check: the verdict and the text of its first finding.
//
////////////////////////////////////////////////////////////////////////////////

struct FileSystemCheckKey
{
    std::wstring  path;
    uint64_t      sizeBytes    = 0;
    int64_t       modifiedUnix = 0;

    bool  operator== (const FileSystemCheckKey &) const = default;
};



struct FileSystemCheckOutcome
{
    FileSystemVerdict  verdict = FileSystemVerdict::None;
    std::wstring       finding;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler
//
//  Checks the file system of the selected image in the background. The check
//  starts once the selection stays on one image for kDebounceMs, a change of
//  selection cancels the check in progress and drops its result, and results
//  are cached by path, length and modified time, so an image checked before
//  has its result at once. All calls but the check itself are made on the UI
//  thread. The executor runs a job off the UI thread, and the notify function
//  is called on that thread once a check finishes; the UI thread then calls
//  TakeFinished.
//
////////////////////////////////////////////////////////////////////////////////

class FileSystemCheckScheduler
{
public:
    using Clock    = std::function<int64_t ()>;
    using Job      = std::function<void ()>;
    using Executor = std::function<void (Job)>;
    using Notify   = std::function<void ()>;
    using Checker  = std::function<bool (const FileSystemCheckKey &, const std::atomic<bool> & isCanceled, FileSystemCheckOutcome &)>;

    static constexpr int64_t  kDebounceMs = 250;

    FileSystemCheckScheduler (Clock clock, Executor executor, Checker check, Notify notify);
    ~FileSystemCheckScheduler ();

    FileSystemCheckScheduler (const FileSystemCheckScheduler &)             = delete;
    FileSystemCheckScheduler & operator= (const FileSystemCheckScheduler &) = delete;

    void     Select        (const FileSystemCheckKey & key);
    void     Clear         ();

    //  Starts the selection's check once kDebounceMs have passed. Returns the
    //  milliseconds until Tick should be called again, or -1 when it needn't be.
    int64_t  Tick          ();

    //  Caches the finished checks. True when the selection's result is among them.
    bool     TakeFinished  ();

    bool     TryGetOutcome (FileSystemCheckOutcome & outOutcome) const;
    bool     IsChecking    () const { return m_active != nullptr; }

    //  The verdict, as "Bad: ", and the finding as a sentence; the text for no
    //  standard file system, or empty for OK.
    static std::wstring  FormatInfobar (const FileSystemCheckOutcome & outcome);

    //  An image of blocks checked as block mode reads it, through `fileIo`: the
    //  worst verdict of its volumes and that volume's first finding.
    static bool  CheckBlockImage (IDiskFileIo & fileIo, const FileSystemCheckKey & key, const std::atomic<bool> & isCanceled, FileSystemCheckOutcome & outOutcome);

private:
    struct Running;
    struct Shared;

    struct Entry
    {
        uint64_t                sizeBytes    = 0;
        int64_t                 modifiedUnix = 0;
        FileSystemCheckOutcome  outcome;
    };

    static int  GetVerdictRank (FileSystemVerdict verdict);

    void  CancelActive ();
    bool  IsCached     (const FileSystemCheckKey & key) const;

    Clock                                m_clock;
    Executor                             m_executor;
    Checker                              m_check;
    std::shared_ptr<Shared>              m_shared;
    std::shared_ptr<Running>             m_active;
    std::map<std::wstring, Entry>        m_cache;
    FileSystemCheckKey                   m_selection;
    bool                                 m_hasSelection = false;
    int64_t                              m_selectedAtMs = 0;
};
