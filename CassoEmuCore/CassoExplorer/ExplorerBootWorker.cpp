#include "Pch.h"

#include "CassoExplorer/ExplorerBootWorker.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootProbe.h"
#include "DiskCheck/BootTitle.h"
#include "DiskCheck/GivenNames.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "FileTypes/SaveSummary.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"





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
        m_queue.Clear();
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
    //  An image a connection just wrote goes ahead of those a list queued.
    Add ({ path, model, source, source.empty() ? BootQueue::Urgency::InView : BootQueue::Urgency::Connection });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::QueueFolderImage
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::QueueFolderImage (const std::wstring & path)
{
    Add ({ path, std::string(), std::string(), BootQueue::Urgency::Folder });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::RemoveFolderImages
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::RemoveFolderImages()
{
    CountFn  onCount;
    size_t   removed = 0;



    {
        std::lock_guard  lock (m_lock);

        removed = m_queue.RemoveFolderImages();
        onCount = m_onCount;
    }

    if (removed > 0 && onCount)
    {
        onCount();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::SetRank
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::SetRank (BootQueue::RankFn rank)
{
    std::lock_guard  lock (m_lock);



    m_rank = std::move (rank);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::Add
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::Add (BootQueue::Item item)
{
    CountFn  onCount;
    bool     isAdded = false;



    {
        std::lock_guard  lock (m_lock);

        if (m_stop)
        {
            return;
        }

        isAdded = m_queue.TryAdd (std::move (item));
        onCount = m_onCount;
    }

    if (!isAdded)
    {
        return;
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



    return m_queue.GetCount() + (m_working ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::IsPending
//
//  The image is cleared before its result is reported, so a window that
//  reads it on the result's message finds it done.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerBootWorker::IsPending (const std::wstring & path) const
{
    std::lock_guard  lock (m_lock);
    bool             isWaiting = m_queue.Contains (path);



    return isWaiting || _wcsicmp (m_current.c_str(), path.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::WaitUntilIdle
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBootWorker::WaitUntilIdle()
{
    std::unique_lock  lock (m_lock);



    m_idle.wait (lock, [this]() { return m_queue.GetCount() == 0 && !m_working && !m_reporting; });
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

    hr = DiskSourceFormat::GetSourceFormatByExtension (narrow, format);
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

    //  A CP/M data disk has no system on tracks 0 to 2, and its boot runs
    //  whatever bytes are on track 0.
    if (report.verdict == BootVerdict::DidNotBoot && IsCpmDataDisk (bytes, narrow))
    {
        report.verdict = BootVerdict::NotBootable;
        report.reason  = "The disk is a CP/M data disk, with no system on tracks 0 to 2.";
    }

    result.hasReport = true;
    result.verdict   = report.verdict;
    result.reason    = report.reason;

    //  A disk that doesn't boot and has a saved game has the save for its
    //  thumbnail, drawn as a text screen.
    if (report.verdict != BootVerdict::Booted)
    {
        DrawSave (bytes, TextEncoding::WideToNarrow (file.wstring()), m_build, report);
    }

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
        std::string  print;
        bool         isRaw    = false;

        CheckFileSystem (bytes, path, result);

        //  A raw capture is of a disk that is likely copy-protected, whose
        //  file system's bookkeeping is often odd on purpose, so it has no
        //  verdict of damage.
        hr = FullSendManifestFile::FindFingerprint (*fileIo, folder, name, print, isRaw);

        if (SUCCEEDED (hr) && isRaw && (result.fsCheck == FullSendManifest::kCheckBad || result.fsCheck == FullSendManifest::kCheckSuspect))
        {
            result.fsCheck = FullSendManifest::kCheckNone;
            result.fsFinding.clear();
        }

        hr = FullSendManifestFile::NoteBoot (*fileIo, folder, name, record.verdict, result.fsCheck, row);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  An image that didn't boot with unreadable sectors or a bad file
        //  system is a failed capture. One that boots isn't, as a crack's odd
        //  catalog reads as bad. The Apple's tone has the same rule. A raw
        //  capture has every track, so the count from the sector read before
        //  it doesn't apply.
        result.isComplete = report.verdict == BootVerdict::Booted
                            || ((row.unreadable == 0 || row.status == FullSendManifest::kStatusRawCapture) && result.fsCheck != FullSendManifest::kCheckBad);

        //  An image saved as a number is renamed with the user's earlier name
        //  for its disk, or its title once it boots. A DOS 3.3 disk that
        //  doesn't boot, as a data disk, takes its first program after HELLO;
        //  one that boots with no title found stays a number, as a game's
        //  program files are often called something meaningless.
        if (titled.empty() && report.verdict != BootVerdict::Booted && (format == DiskFormat::Dsk || format == DiskFormat::Do))
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
        BootQueue::Item  item;
        DoneFn           done;
        BusyFn           isBusy;
        CountFn          onCount;
        Result           result;
        bool             isTaken = false;

        {
            std::unique_lock  lock (m_lock);

            m_wake.wait (lock, [this]() { return m_stop || m_queue.GetCount() > 0; });

            if (m_stop)
            {
                return;
            }

            isBusy = m_isBusy;
        }

        //  The image stays in the queue until the emulator is free, so the
        //  same path queued again meanwhile is found there and isn't booted
        //  twice, and an image a connection writes meanwhile goes first.
        while (isBusy && isBusy())
        {
            std::unique_lock  lock (m_lock);

            if (m_wake.wait_for (lock, std::chrono::milliseconds (s_kBusyPollMs), [this]() { return m_stop; }))
            {
                return;
            }
        }

        {
            std::lock_guard  lock (m_lock);

            if (m_stop)
            {
                return;
            }

            isTaken = m_queue.TryTakeNext (m_rank, item);

            //  The folder images can be removed while the emulator is busy.
            if (!isTaken)
            {
                continue;
            }

            m_working = true;
            m_current = item.path;
        }

        result        = Check (item.path, item.model);
        result.source = item.source;

        {
            std::lock_guard  lock (m_lock);

            done = m_done;
            m_current.clear();
        }

        if (done)
        {
            done (result);
        }

        {
            std::lock_guard  lock (m_lock);

            m_working   = false;
            m_reporting = true;
            onCount     = m_onCount;
        }

        if (onCount)
        {
            onCount();
        }

        //  m_reporting stays set until the count is reported, so every
        //  callback has returned by the time WaitUntilIdle does.
        {
            std::lock_guard  lock (m_lock);

            m_reporting = false;
        }

        m_idle.notify_all();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::IsCpmDataDisk
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerBootWorker::IsCpmDataDisk (const std::vector<Byte> & bytes, const std::string & path)
{
    std::vector<Byte>   sectors;
    SectorDecodeReport  decoded;
    HRESULT             hr      = VolumeImage::Load (bytes, path, sectors, decoded);



    return SUCCEEDED (hr) && VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm && !CpmVolume::HasSystemTracks (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker::DrawSave
//
//  The rows are written to the text page of an enhanced //e, powered on and
//  not run, in normal characters, and its screen is drawn as a boot's is.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerBootWorker::DrawSave (const std::vector<Byte> & bytes, const std::string & path, const BootCheck::MachineFn & build, BootReport & ioReport)
{
    HRESULT                   hr      = S_OK;
    std::vector<Byte>         sectors;
    SectorDecodeReport        decoded;
    std::vector<std::string>  rows;
    BootCheck::Built          built;
    std::string               error;
    bool                      isDrawn = false;



    hr = VolumeImage::Load (bytes, path, sectors, decoded);
    BAIL_OUT_IF (FAILED (hr) || !SaveSummary::Find (sectors, rows), S_OK);

    hr = build (BootCheck::kEnhanced, built, error);
    BAIL_OUT_IF (FAILED (hr) || built.host == nullptr || built.builder == nullptr, S_OK);

    built.host->PowerCycle();

    for (size_t row = 0; row < SaveSummary::kRows; row++)
    {
        Word  base = (Word) (s_kTextPage + (row % 8) * s_kRowStride + (row / 8) * s_kThirdStride);

        for (size_t column = 0; column < SaveSummary::kColumns; column++)
        {
            char  ch = (row < rows.size() && column < rows[row].size()) ? rows[row][column] : ' ';

            built.host->GetMemoryBus().WriteByte ((Word) (base + column), (Byte) (ch | s_kNormal));
        }
    }

    BootProbe::RenderFrame (*built.host, *built.builder, ioReport.frame, ioReport.width, ioReport.height);
    isDrawn = true;

Error:
    return isDrawn;
}
