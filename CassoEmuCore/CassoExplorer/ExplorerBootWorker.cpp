#include "Pch.h"

#include "CassoExplorer/ExplorerBootWorker.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootTitle.h"
#include "Devices/Disk/DiskImageStore.h"





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
    }

    m_wake.notify_all();
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
    bool                   isOpen   = false;



    result.path = path;

    {
        std::lock_guard  lock (m_lock);

        limits = m_limits;
        fileIo = m_fileIo;
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
    record.title   = BootTitle::Find (report.rows, report.isTextMode);

    hr = m_store.Store (file, png, record);
    CHR (hr);

    if (fileIo != nullptr)
    {
        std::string  folder  = TextEncoding::WideToNarrow (file.parent_path().wstring());
        std::string  name    = TextEncoding::WideToNarrow (file.filename().wstring());
        std::string  titled  = MakeTitledName (name, record.title);
        bool         renamed = false;

        hr = FullSendManifestFile::NoteBoot (*fileIo, folder, name, record.verdict);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  A raw capture saved as a number takes its title, as its disk has
        //  no catalog to take a name from.
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
//  ExplorerBootWorker::MakeTitledName
//
//  A character a Windows file name can't have is a space, runs of spaces are
//  one, and the title ends at s_kMaxTitleChars.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerBootWorker::MakeTitledName (const std::string & name, const std::string & title)
{
    static constexpr std::string_view  s_kInvalid = "\\/:*?\"<>|";
    size_t                             dot        = name.find_last_of ('.');
    std::string                        stem       = name.substr (0, dot);
    std::string                        extension  = (dot == std::string::npos) ? std::string() : name.substr (dot);
    bool                               isNumber   = !stem.empty() && std::all_of (stem.begin(), stem.end(), [] (char each) { return each >= '0' && each <= '9'; });
    std::string                        cleaned;



    for (char each : BootTitle::ToName (title))
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
        text = std::format ("{} wasn't booted.", name);

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
        default:                       text = std::format ("{} may not boot on the {}. {}", name, machine, result.reason);   break;
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
        Item    item;
        DoneFn  done;
        BusyFn  isBusy;
        Result  result;

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
    }
}
