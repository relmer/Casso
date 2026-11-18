#include "Pch.h"

#include "CassoExplorer/ExplorerBootWorker.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootTitle.h"
#include "DiskCheck/GivenNames.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::ExplorerBootWorker
//
////////////////////////////////////////////////////////////////////////////////

ExplorerBootWorker::ExplorerBootWorker (ThumbnailStore & store, BootCheck::MachineFn build) :
    m_store (store),
    m_build (std::move (build))
{
    m_thread = std::thread ([this]() { Run(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::~ExplorerBootWorker
//
//  The image being booted is finished first; the ones waiting aren't booted.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerBootWorker::~ExplorerBootWorker()
{
    {
        std::lock_guard  lock (m_lock);

        m_stop = true;
        m_queue.clear();
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetDone
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetDone (DoneFn done)
{
    std::lock_guard  lock (m_lock);



    m_done = std::move (done);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetBusy
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetBusy (BusyFn isBusy)
{
    std::lock_guard  lock (m_lock);



    m_isBusy = std::move (isBusy);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetManifestIo
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetManifestIo (IDiskFileIo * fileIo)
{
    std::lock_guard  lock (m_lock);



    m_fileIo = fileIo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetGivenNames
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetGivenNames (GivenNames * given)
{
    std::lock_guard  lock (m_lock);



    m_given = given;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetLimits
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetLimits (const BootProbe::Limits & limits)
{
    std::lock_guard  lock (m_lock);



    m_limits = limits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::Queue
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::Queue (const std::wstring & path, const std::string & model, const std::string & source)
{
    CountFn  onCount;



    {
        std::lock_guard  lock (m_lock);
        bool             isWaiting = std::any_of (m_queue.begin(), m_queue.end(), [&path] (const Item & item) { return _wcsicmp (item.path.c_str(), path.c_str()) == 0; });

        if (m_stop || isWaiting)
        {
            return;
        }

        //  An image a connection just wrote goes ahead of those a list queued.
        if (source.empty())
        {
            m_queue.push_back ({ path, model, source });
        }
        else
        {
            m_queue.push_front ({ path, model, source });
        }

        onCount = m_onCount;
    }

    m_wake.notify_all();

    if (onCount)
    {
        onCount();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetOnCount
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetOnCount (CountFn onCount)
{
    std::lock_guard  lock (m_lock);



    m_onCount = std::move (onCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::GetPending
//
////////////////////////////////////////////////////////////////////////////////

size_t ExplorerBootWorker::GetPending() const
{
    std::lock_guard  lock (m_lock);



    return m_queue.size() + (m_working ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::WaitUntilIdle
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::WaitUntilIdle()
{
    std::unique_lock  lock (m_lock);



    m_idle.wait (lock, [this]() { return m_queue.empty() && !m_working; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::Check
//
//  A file that can't be read, or isn't a disk image by its extension, has
//  no report.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerBootWorker::Result ExplorerBootWorker::Check (const std::wstring & path, const std::string & model)
{
    HRESULT                hr       = S_OK;
    Result                 result;
    std::filesystem::path  file     = path;
    std::string            narrow   = TextEncoding::WideToNarrow (path);
    DiskFormat             format   = DiskFormat::Dsk;
    std::vector<Byte>      bytes;
    std::vector<Byte>      png;
    BootReport             report;
    ThumbnailRecord        record;
    BootProbe::Limits      limits;
    IDiskFileIo          * fileIo   = nullptr;
    GivenNames           * given    = nullptr;
    bool                   isOpen   = false;



    result.path = path;

    {
        std::lock_guard  lock (m_lock);

        limits = m_limits;
        fileIo = m_fileIo;
        given  = m_given;
    }

    hr = DiskImageStore::GetSourceFormatByExtension (narrow, format);
    CHR (hr);

    {
        std::ifstream  in (file, std::ios::binary);

        isOpen = in.is_open();

        if (isOpen)
        {
            bytes.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }
    }

    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = BootCheck::Run (bytes, format, model, limits, m_build, report, result.machine, result.missing);
    CHR (hr);

    result.hasReport = true;
    result.verdict   = report.verdict;
    result.reason    = report.reason;

    hr = PicturePreview::EncodePng (report.frame, report.width, report.height, png);
    CHR (hr);

    record.verdict = BootProbe::GetVerdictName (report.verdict);
    record.machine = result.machine;
    record.reason  = report.reason;
    record.title   = (report.verdict == BootVerdict::Booted) ? BootTitle::Find (report.rows, report.isTextMode) : std::string();

    hr = m_store.Store (file, png, record);
    CHR (hr);

    if (fileIo != nullptr)
    {
        std::string  folder   = TextEncoding::WideToNarrow (file.parent_path().wstring());
        std::string  name     = TextEncoding::WideToNarrow (file.filename().wstring());
        std::string  titled   = (report.verdict == BootVerdict::Booted) ? MakeTitledName (name, record.title) : std::string();
        std::string  fromUser = FindGivenName (*fileIo, given, folder, name);
        FullSendRow  row;
        bool         renamed  = false;

        CheckFileSystem (bytes, path, result);

        hr = FullSendManifestFile::NoteBoot (*fileIo, folder, name, record.verdict, result.fsCheck, row);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  An image with unreadable sectors or a bad file system is a failed
        //  capture, even when it boots. The Apple's tone has the same rule. A
        //  raw capture has every track, so the count from the sector read
        //  before it doesn't apply.
        result.isComplete = (row.unreadable == 0 || row.status == FullSendManifest::kStatusRawCapture) && result.fsCheck != FullSendManifest::kCheckBad;

        //  An image saved as a number is renamed with the user's earlier name
        //  for its disk, or its title once it boots, or with neither, a DOS
        //  3.3 disk's first program after HELLO.
        if (titled.empty() && (format == DiskFormat::Dsk || format == DiskFormat::Do))
        {
            titled = MakeNumberedName (name, FullSendDisk::GetProgramTitle (bytes));
        }

        titled = fromUser.empty() ? titled : fromUser;

        if (!titled.empty())
        {
            hr = FullSendManifestFile::RenameCapture (*fileIo, folder, name, titled, renamed);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (renamed)
        {
            result.renamedFrom = name;
            result.path        = (file.parent_path() / TextEncoding::NarrowToWide (titled)).wstring();
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::CheckFileSystem
//
//  The disk inspector's file-system check of the bytes that were booted: a
//  sector image's sectors, or the sectors a raw capture decodes to. An image
//  the inspector can't load has no verdict.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::CheckFileSystem (const std::vector<Byte> & bytes, const std::wstring & path, Result & ioResult)
{
    HRESULT          hr      = S_OK;
    DiskAnalysis     analysis;
    std::wstring     reason;
    const Finding  * finding = nullptr;



    hr = FileSystemCheck::Analyze (bytes, TextEncoding::WideToUtf8 (path), analysis, reason);
    CHR (hr);

    finding            = FileSystemCheck::GetVerdictFinding (analysis);
    ioResult.fsCheck   = GetCheckName (analysis.verdict);
    ioResult.fsFinding = (finding != nullptr) ? TextEncoding::WideToNarrow (TextEncoding::Utf8ToWide (finding->detail)) : std::string();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::GetCheckName
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::GetCheckName (FileSystemVerdict verdict)
{
    const char  * name = FullSendManifest::kCheckNone;



    switch (verdict)
    {
        case FileSystemVerdict::Ok:      name = FullSendManifest::kCheckOk;      break;
        case FileSystemVerdict::Suspect: name = FullSendManifest::kCheckSuspect; break;
        case FileSystemVerdict::Bad:     name = FullSendManifest::kCheckBad;     break;
        case FileSystemVerdict::None:    name = FullSendManifest::kCheckNone;    break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::FindGivenName
//
//  The image's name with the user's name for its disk, for a raw capture
//  whose disk has one; empty otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::FindGivenName (IDiskFileIo & fileIo, GivenNames * given, const std::string & folder, const std::string & name)
{
    HRESULT      hr          = S_OK;
    std::string  fingerprint;
    std::string  saved;
    std::string  numbered;
    bool         isRaw       = false;
    bool         hasName     = false;



    BAIL_OUT_IF (given == nullptr, S_OK);

    hr = FullSendManifestFile::FindFingerprint (fileIo, folder, name, fingerprint, isRaw);
    CHR (hr);
    BAIL_OUT_IF (!isRaw, S_OK);

    hasName = given->TryGet (fingerprint, saved);
    BAIL_OUT_IF (!hasName, S_OK);

    numbered = MakeNumberedName (name, saved);

Error:
    return numbered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::MakeTitledName
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::MakeTitledName (const std::string & name, const std::string & title)
{
    return MakeNumberedName (name, BootTitle::ToName (title));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::MakeNumberedName
//
//  A character a Windows file name can't have is a space, runs of spaces are
//  one, and the name ends at s_kMaxTitleChars.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::MakeNumberedName (const std::string & name, const std::string & given)
{
    static constexpr std::string_view  s_kInvalid = "\\/:*?\"<>|";
    size_t                             dot        = name.find_last_of ('.');
    std::string                        stem       = name.substr (0, dot);
    std::string                        extension  = (dot == std::string::npos) ? std::string() : name.substr (dot);
    bool                               isNumber   = !stem.empty() && std::all_of (stem.begin(), stem.end(), [] (char each) { return each >= '0' && each <= '9'; });
    std::string                        cleaned;



    for (char each : given)
    {
        bool  isSpace = each == ' ' || (unsigned char) each < ' ' || s_kInvalid.find (each) != std::string_view::npos;

        if (!isSpace)
        {
            cleaned += each;
        }
        else if (!cleaned.empty() && cleaned.back() != ' ')
        {
            cleaned += ' ';
        }
    }

    cleaned = cleaned.substr (0, s_kMaxTitleChars);

    while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '.'))
    {
        cleaned.pop_back();
    }

    return (isNumber && !cleaned.empty()) ? stem + " " + cleaned + extension : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::Describe (const Result & result)
{
    std::string  name    = TextEncoding::WideToNarrow (std::filesystem::path (result.path).filename().wstring());
    std::string  machine = BootCheck::GetMachineName (result.machine);
    std::string  text;



    if (!result.hasReport)
    {
        text = result.missing.empty() ? std::format ("{} couldn't be read as a disk image, so it wasn't booted.", name)
                                      : std::format ("{} wasn't booted: the emulated machine failed to start.", name);

        for (const std::string & missing : result.missing)
        {
            text += " " + missing;
        }

        return text;
    }

    switch (result.verdict)
    {
        case BootVerdict::Booted:      text = std::format ("{} booted on the {}.", name, machine);                           break;
        case BootVerdict::DidNotBoot:  text = std::format ("{} did not boot on the {}. {}", name, machine, result.reason);   break;
        case BootVerdict::NotBootable: text = std::format ("{} is not a bootable disk. {}", name, result.reason);           break;
        default:                       text = std::format ("It is unclear whether {} boots on the {}. {}", name, machine, result.reason); break;
    }

    return text + DescribeCheck (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::DescribeCheck
//
//  The file-system verdict after the boot result, as the inspector writes
//  it, with its first finding: " File system: Bad, ..." Empty for an image
//  with no verdict.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::DescribeCheck (const Result & result)
{
    std::string  text;
    std::string  end;



    if (result.fsCheck == FullSendManifest::kCheckNone)
    {
        text = " No standard file system.";
    }
    else if (!result.fsCheck.empty())
    {
        text = result.fsCheck == FullSendManifest::kCheckOk      ? " File system: OK"
             : result.fsCheck == FullSendManifest::kCheckSuspect ? " File system: Possible inconsistencies"
                                                                 : " File system: Bad";

        text += result.fsFinding.empty() ? std::string() : ", " + result.fsFinding;
        end   = text.ends_with ('.') ? "" : ".";
        text += end;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::Run
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::Run()
{
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    for (;;)
    {
        Item     item;
        DoneFn   done;
        BusyFn   isBusy;
        CountFn  onCount;
        Result   result;

        {
            std::unique_lock  lock (m_lock);

            m_wake.wait (lock, [this]() { return m_stop || !m_queue.empty(); });

            if (m_stop)
            {
                return;
            }

            item      = m_queue.front();
            isBusy    = m_isBusy;
            m_working = true;

            m_queue.pop_front();
        }

        while (isBusy && isBusy())
        {
            std::unique_lock  lock (m_lock);

            if (m_wake.wait_for (lock, std::chrono::milliseconds (s_kBusyPollMs), [this]() { return m_stop; }))
            {
                return;
            }
        }

        result        = Check (item.path, item.model);
        result.source = item.source;

        {
            std::lock_guard  lock (m_lock);

            done = m_done;
        }

        if (done)
        {
            done (result);
        }

        {
            std::lock_guard  lock (m_lock);

            m_working = false;
        }

        m_idle.notify_all();

        {
            std::lock_guard  lock (m_lock);

            onCount = m_onCount;
        }

        if (onCount)
        {
            onCount();
        }
    }
}
