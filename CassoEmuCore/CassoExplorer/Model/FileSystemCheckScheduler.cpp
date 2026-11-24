#include "Pch.h"

#include "CassoExplorer/Model/FileSystemCheckScheduler.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskInspectCommand.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::Running
//
//  One check: its image, its cancel flag and, once finished, its result.
//
////////////////////////////////////////////////////////////////////////////////

struct FileSystemCheckScheduler::Running
{
    FileSystemCheckKey      key;
    std::atomic<bool>       isCanceled = false;
    FileSystemCheckOutcome  outcome;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::Shared
//
//  The finished checks, passed from the executor's thread to the UI thread.
//  The notify function is cleared when the scheduler is destroyed, so a check
//  that finishes after that calls nothing.
//
////////////////////////////////////////////////////////////////////////////////

struct FileSystemCheckScheduler::Shared
{
    std::mutex                          lock;
    vector<std::shared_ptr<Running>>    finished;
    Notify                              notify;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::FileSystemCheckScheduler
//
////////////////////////////////////////////////////////////////////////////////

FileSystemCheckScheduler::FileSystemCheckScheduler (Clock clock, Executor executor, Checker check, Notify notify) :
    m_clock    (std::move (clock)),
    m_executor (std::move (executor)),
    m_check    (std::move (check)),
    m_shared   (std::make_shared<Shared>())
{
    m_shared->notify = std::move (notify);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::~FileSystemCheckScheduler
//
////////////////////////////////////////////////////////////////////////////////

FileSystemCheckScheduler::~FileSystemCheckScheduler()
{
    CancelActive();

    std::lock_guard<std::mutex>  lock (m_shared->lock);

    m_shared->notify = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::Select
//
//  Another image cancels the check in progress and starts the wait over.
//
////////////////////////////////////////////////////////////////////////////////

void FileSystemCheckScheduler::Select (const FileSystemCheckKey & key)
{
    if (m_hasSelection && key == m_selection)
    {
        return;
    }

    CancelActive();

    m_selection    = key;
    m_hasSelection = true;
    m_selectedAtMs = m_clock();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::Clear
//
////////////////////////////////////////////////////////////////////////////////

void FileSystemCheckScheduler::Clear()
{
    CancelActive();

    m_selection    = FileSystemCheckKey();
    m_hasSelection = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::Tick
//
//  A job for the executor checks the image and, unless it was canceled,
//  queues its result for TakeFinished.
//
////////////////////////////////////////////////////////////////////////////////

int64_t FileSystemCheckScheduler::Tick()
{
    int64_t                   elapsed = 0;
    std::shared_ptr<Running>  running;
    std::shared_ptr<Shared>   shared  = m_shared;
    Checker                   check   = m_check;



    if (!m_hasSelection || m_active != nullptr || IsCached (m_selection))
    {
        return -1;
    }

    elapsed = m_clock() - m_selectedAtMs;

    if (elapsed < kDebounceMs)
    {
        return kDebounceMs - elapsed;
    }

    running      = std::make_shared<Running>();
    running->key = m_selection;
    m_active     = running;

    m_executor ([running, shared, check] ()
    {
        bool  isChecked = false;



        if (!running->isCanceled)
        {
            isChecked = check (running->key, running->isCanceled, running->outcome);
        }

        std::lock_guard<std::mutex>  lock (shared->lock);

        if (isChecked && !running->isCanceled)
        {
            shared->finished.push_back (running);

            if (shared->notify)
            {
                shared->notify();
            }
        }
    });

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::TakeFinished
//
////////////////////////////////////////////////////////////////////////////////

bool FileSystemCheckScheduler::TakeFinished()
{
    vector<std::shared_ptr<Running>>  finished;
    bool                              isSelected = false;



    {
        std::lock_guard<std::mutex>  lock (m_shared->lock);

        finished.swap (m_shared->finished);
    }

    for (const std::shared_ptr<Running> & running : finished)
    {
        m_cache[running->key.path] = Entry { running->key.sizeBytes, running->key.modifiedUnix, running->outcome };

        if (running == m_active)
        {
            m_active = nullptr;
        }

        isSelected = isSelected || (m_hasSelection && running->key == m_selection);
    }

    return isSelected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::TryGetOutcome
//
//  The cached result for the selection, when its length and time match.
//
////////////////////////////////////////////////////////////////////////////////

bool FileSystemCheckScheduler::TryGetOutcome (FileSystemCheckOutcome & outOutcome) const
{
    bool  isCached = m_hasSelection && IsCached (m_selection);



    if (isCached)
    {
        outOutcome = m_cache.at (m_selection.path).outcome;
    }

    return isCached;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::FormatInfobar
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileSystemCheckScheduler::FormatInfobar (const FileSystemCheckOutcome & outcome)
{
    std::wstring  text;
    std::wstring  sentence = outcome.finding;



    if (!sentence.empty())
    {
        sentence[0] = static_cast<wchar_t> (towupper (sentence[0]));
    }

    if (!sentence.empty() && !sentence.ends_with (L'.'))
    {
        sentence += L".";
    }

    if (outcome.verdict == FileSystemVerdict::None)
    {
        text = L"No standard file system was found.";
    }
    else if (outcome.verdict == FileSystemVerdict::Bad || outcome.verdict == FileSystemVerdict::Suspect)
    {
        text = FileSystemCheck::FormatVerdict (outcome.verdict) + (sentence.empty() ? std::wstring (L".") : L": " + sentence);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::CheckBlockImage
//
//  Each volume's map is read when the image opens. False for an image that
//  can't be opened or a check that was canceled.
//
////////////////////////////////////////////////////////////////////////////////

bool FileSystemCheckScheduler::CheckBlockImage (IDiskFileIo & fileIo, const FileSystemCheckKey & key, const std::atomic<bool> & isCanceled, FileSystemCheckOutcome & outOutcome)
{
    HRESULT                          hr        = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    std::wstring                     error;
    BlockInspectorModel              model;
    FileSystemVerdict                verdict   = FileSystemVerdict::None;
    int                              volume    = 0;
    bool                             isStopped = false;
    bool                             isDone    = false;



    outOutcome = FileSystemCheckOutcome();

    hr = BlockImageFile::Open (fileIo, TextEncoding::WideToUtf8 (key.path), file, error);
    CHR (hr);

    isStopped = isCanceled;
    CBR (!isStopped);

    hr = model.Open (std::move (file));
    CHR (hr);

    isStopped = isCanceled;
    CBR (!isStopped);

    for (volume = 0; volume < model.GetVolumeCount(); volume++)
    {
        const BlockMap  & map = model.GetVolumeMap (volume);

        verdict = BlockRepair::GetVerdict (map);

        if (GetVerdictRank (verdict) > GetVerdictRank (outOutcome.verdict))
        {
            vector<DiskInspectCommand::BlockFinding>  findings = DiskInspectCommand::GetBlockFindings (map);

            outOutcome.verdict = verdict;
            outOutcome.finding = findings.empty() ? std::wstring() : findings.front().sentence;
        }
    }

    isDone = !isCanceled;

Error:
    return isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::GetVerdictRank
//
//  Bad ranks highest, then Suspect, then OK, then no file system.
//
////////////////////////////////////////////////////////////////////////////////

int FileSystemCheckScheduler::GetVerdictRank (FileSystemVerdict verdict)
{
    constexpr int  kOkRank      = 1;
    constexpr int  kSuspectRank = 2;
    constexpr int  kBadRank     = 3;
    int            rank         = 0;



    if (verdict == FileSystemVerdict::Bad)
    {
        rank = kBadRank;
    }
    else if (verdict == FileSystemVerdict::Suspect)
    {
        rank = kSuspectRank;
    }
    else if (verdict == FileSystemVerdict::Ok)
    {
        rank = kOkRank;
    }

    return rank;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::CancelActive
//
//  The check in progress stops at its next test of the flag, and its result
//  is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void FileSystemCheckScheduler::CancelActive()
{
    if (m_active != nullptr)
    {
        m_active->isCanceled = true;
        m_active             = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheckScheduler::IsCached
//
////////////////////////////////////////////////////////////////////////////////

bool FileSystemCheckScheduler::IsCached (const FileSystemCheckKey & key) const
{
    auto  found = m_cache.find (key.path);



    return found != m_cache.end() && found->second.sizeBytes == key.sizeBytes && found->second.modifiedUnix == key.modifiedUnix;
}




