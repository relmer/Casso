#include "Pch.h"

#include "Ui/DiskInspector/ComparisonSession.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::ComparisonSession
//
//  B's results are taken on the window's frame, so its scheduler needs no
//  wake-up of its own.
//
////////////////////////////////////////////////////////////////////////////////

ComparisonSession::ComparisonSession() :
    m_bScheduler (std::make_unique<AnalysisScheduler> (nullptr))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::~ComparisonSession
//
////////////////////////////////////////////////////////////////////////////////

ComparisonSession::~ComparisonSession()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::Begin
//
//  Anything still arriving from an earlier comparison is dropped by its
//  generation.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::Begin (const ComparisonSource & a, const ComparisonSource & b, bool isLoadingA, const DecodeSettings & settings, IDiskInspectorHost & host)
{
    End();

    m_isComparing    = true;
    m_sources        = { a, b };
    m_settings       = settings;
    m_hasOwnSettings = false;

    StartRead (kSideB, host);

    if (isLoadingA)
    {
        StartRead (kSideA, host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::End
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::End()
{
    m_generation++;
    m_isComparing = false;
    m_reads       = {};
    m_errors      = {};
    m_b           = DiskAnalysis();
    m_result      = DiskComparison();
    m_hasResult   = false;
    m_isRunning   = false;
    m_listed.clear();
    m_version++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::OfferReply
//
//  A drive's disk as it is now is the side's disk; for "Its file" the copy
//  gives the file's path, and the file is read next.
//
////////////////////////////////////////////////////////////////////////////////

bool ComparisonSession::OfferReply (const InspectorReply & reply)
{
    bool        isTaken = false;
    LoadedDisk  loaded;



    for (int side = kSideA; side <= kSideB && !isTaken; side++)
    {
        Read &  read = m_reads[side];

        if (!read.isActive || read.hostRequest == 0 || reply.requestId != read.hostRequest)
        {
            continue;
        }

        isTaken          = true;
        read.hostRequest = 0;
        loaded.side      = side;

        if (reply.disk == nullptr)
        {
            loaded.reason = std::format (L"There is no disk in drive {}.", read.source.drive + 1);
        }
        else if (read.source.kind == ComparisonSourceKind::ItsFile && reply.disk->fileName.empty())
        {
            loaded.reason = std::format (L"The disk in drive {} has no file.", read.source.drive + 1);
        }
        else if (read.source.kind == ComparisonSourceKind::ItsFile)
        {
            ReadFile (side, reply.disk->fileName);
            continue;
        }
        else
        {
            loaded.copy = reply.disk;
        }

        std::scoped_lock  lock (m_lock);

        m_fileLoads.push_back ({ m_generation, std::move (loaded) });
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::Tick
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::Tick (const DiskAnalysis & a, bool isAReady, vector<LoadedDisk> & outLoadsForA)
{
    vector<std::pair<uint64_t, LoadedDisk>>  loads;



    outLoadsForA.clear();

    {
        std::scoped_lock  lock (m_lock);

        loads.swap (m_fileLoads);
    }

    for (auto & [generation, loaded] : loads)
    {
        if (generation == m_generation && m_isComparing)
        {
            Deliver (std::move (loaded), outLoadsForA);
        }
    }

    TakeBResults();
    TakeCompared();

    if (m_isComparing && !m_isRunning && m_posted != m_changes && isAReady && IsBReady() && !m_reads[kSideA].isActive)
    {
        StartCompare (a);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::MarkAChanged
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::MarkAChanged()
{
    m_changes++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::ApplySettings
//
//  The window's settings apply to B too, unless B has settings of its own
//  (FR-117).
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::ApplySettings (const DecodeSettings & settings)
{
    m_settings = settings;

    if (!m_hasOwnSettings && m_b.copy != nullptr)
    {
        m_b.settings = settings;
        m_bScheduler->Restart (m_b.copy, settings);
        m_changes++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::ApplyOwnSettings
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::ApplyOwnSettings (const DecodeSettings & settings)
{
    m_hasOwnSettings = true;

    if (m_b.copy != nullptr)
    {
        m_b.settings = settings;
        m_bScheduler->Restart (m_b.copy, settings);
        m_changes++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::SetOptions
//
//  The options change only which differences are listed (FR-120).
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::SetOptions (const ComparisonOptions & options)
{
    m_options = options;
    m_listed  = m_result.GetListed (m_options);
    m_version++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::Swap
//
//  A comparison under way was of the sides as they were, so it is dropped
//  and the two are compared again.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::Swap (DiskAnalysis & inOutA, std::unique_ptr<AnalysisScheduler> & inOutScheduler, ComparisonSource & inOutSource)
{
    std::swap (inOutA,         m_b);
    std::swap (inOutScheduler, m_bScheduler);
    std::swap (inOutSource,    m_sources[kSideB]);

    m_sources[kSideA] = inOutSource;
    m_generation++;
    m_changes++;
    m_isRunning = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::IsBusy
//
//  True while a side is being read or analyzed or the two are being
//  compared, which the window shows as "Comparing".
//
////////////////////////////////////////////////////////////////////////////////

bool ComparisonSession::IsBusy() const
{
    return m_isComparing && m_errors[kSideA].empty() && m_errors[kSideB].empty() &&
           (m_reads[kSideA].isActive || m_reads[kSideB].isActive || m_isRunning || m_posted != m_changes || !m_hasResult);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::StartRead
//
//  A drive's disk now and its file both start with a copy from the host;
//  an image file is read on the background thread. The disk as inserted is
//  not kept yet, so that source says so.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::StartRead (int side, IDiskInspectorHost & host)
{
    Read &            read    = m_reads[side];
    InspectorRequest  request;



    read          = Read();
    read.source   = m_sources[side];
    read.isActive = true;
    m_errors[side].clear();

    switch (read.source.kind)
    {
        case ComparisonSourceKind::DriveNow:
        case ComparisonSourceKind::ItsFile:
            request.kind     = InspectorRequestKind::CopyDisk;
            request.drive    = read.source.drive;
            read.hostRequest = host.PostInspectorRequest (request);
            break;

        case ComparisonSourceKind::ImageFile:
            ReadFile (side, TextEncoding::WideToUtf8 (read.source.path));
            break;

        case ComparisonSourceKind::AsInserted:
        {
            std::scoped_lock  lock (m_lock);

            m_fileLoads.push_back ({ m_generation, { side, nullptr, std::format (L"Drive {}'s disk as inserted is not kept yet.", read.source.drive + 1) } });
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::ReadFile (int side, const std::string & utf8Path)
{
    uint64_t  generation = m_generation;



    m_queue.Post ([this, side, utf8Path, generation] ()
    {
        LoadedDisk  loaded;
        HRESULT     hr     = S_OK;



        loaded.side = side;
        hr          = InspectorImageLoader::LoadFile (utf8Path, loaded.copy, loaded.reason);
        IGNORE_RETURN_VALUE (hr, S_OK);

        std::scoped_lock  lock (m_lock);

        m_fileLoads.push_back ({ generation, std::move (loaded) });
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::Deliver
//
//  B's disk starts its analysis here; A's goes to the window.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::Deliver (LoadedDisk loaded, vector<LoadedDisk> & outLoadsForA)
{
    m_reads[loaded.side].isActive = false;
    m_errors[loaded.side]         = loaded.reason;

    if (loaded.side == kSideB && loaded.copy != nullptr)
    {
        StartB (loaded.copy);
    }
    else if (loaded.side == kSideA)
    {
        outLoadsForA.push_back (std::move (loaded));
    }

    m_changes++;
    m_version++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::StartB
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::StartB (std::shared_ptr<const DiskCopy> copy)
{
    m_b           = DiskAnalysis();
    m_b.mediaId   = copy->mediaId;
    m_b.copy      = copy;
    m_b.settings  = m_settings;
    m_b.headLimit = Disk2Controller::kMaxQuarterTrack;
    m_b.tracks.resize (copy->tracks.size());
    DiskAnalyzer::Assemble (m_b);
    m_bScheduler->Restart (copy, m_settings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::TakeBResults
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::TakeBResults()
{
    vector<RecordResult>  results;



    m_bScheduler->TakeResults (results);

    for (RecordResult & result : results)
    {
        if (result.mediaId == m_b.mediaId)
        {
            DiskAnalyzer::Accept (result.copy, result.slot, std::move (result.analysis), m_b);
        }
    }

    if (!results.empty())
    {
        DiskAnalyzer::Assemble (m_b);
        m_changes++;
        m_version++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::StartCompare
//
//  Each side is copied as it is now; the copies share every record's
//  analysis with the originals, so this costs little.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::StartCompare (const DiskAnalysis & a)
{
    auto      copyA      = std::make_shared<const DiskAnalysis> (a);
    auto      copyB      = std::make_shared<const DiskAnalysis> (m_b);
    uint64_t  generation = m_generation;
    uint64_t  change     = m_changes;



    m_posted    = m_changes;
    m_isRunning = true;

    m_queue.Post ([this, copyA, copyB, generation, change] ()
    {
        Compared  compared { generation, change, DiskComparer::Compare (*copyA, *copyB) };

        std::scoped_lock  lock (m_lock);

        m_compared.push_back (std::move (compared));
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::TakeCompared
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonSession::TakeCompared()
{
    vector<Compared>  compared;



    {
        std::scoped_lock  lock (m_lock);

        compared.swap (m_compared);
    }

    for (Compared & c : compared)
    {
        if (c.generation == m_generation && c.change == m_posted)
        {
            m_result    = std::move (c.result);
            m_listed    = m_result.GetListed (m_options);
            m_hasResult = true;
            m_isRunning = false;
            m_version++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSession::IsBReady
//
////////////////////////////////////////////////////////////////////////////////

bool ComparisonSession::IsBReady() const
{
    return m_b.copy != nullptr && !m_reads[kSideB].isActive && !m_bScheduler->HasPending();
}
