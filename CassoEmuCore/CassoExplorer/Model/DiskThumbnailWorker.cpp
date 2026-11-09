#include "Pch.h"

#include "CassoExplorer/Model/DiskThumbnailWorker.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Ui/DiskInspector/PlatterCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::DiskThumbnailWorker
//
////////////////////////////////////////////////////////////////////////////////

DiskThumbnailWorker::DiskThumbnailWorker (Notify notify, Builder build) :
    m_notify (std::move (notify)),
    m_build  (build ? std::move (build) : Builder (&DiskThumbnailWorker::Build))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::~DiskThumbnailWorker
//
//  A thumbnail being built finishes before the thread is joined.
//
////////////////////////////////////////////////////////////////////////////////

DiskThumbnailWorker::~DiskThumbnailWorker()
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_isStopping = true;
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::Request
//
//  The thread starts with the first request.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskThumbnailWorker::Request (const std::wstring & path)
{
    uint64_t  id = 0;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        id          = ++m_latest;
        m_path      = path;
        m_pending   = id;
        m_hasWork   = true;
        m_hasResult = false;
    }

    if (!m_thread.joinable())
    {
        m_thread = std::thread ([this] () { Run(); });
    }

    m_wake.notify_all();

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::Cancel
//
//  A thumbnail being built is dropped when it finishes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailWorker::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    ++m_latest;
    m_hasWork   = false;
    m_hasResult = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::TakeResult
//
////////////////////////////////////////////////////////////////////////////////

bool DiskThumbnailWorker::TakeResult (DiskThumbnailResult & outResult)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    bool                         isTaken = m_hasResult && m_result.request == m_latest;



    if (isTaken)
    {
        outResult   = std::move (m_result);
        m_hasResult = false;
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::Build
//
//  A file that cannot be read has no thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

DiskThumbnailResult DiskThumbnailWorker::Build (const std::wstring & path)
{
    HRESULT              hr = S_OK;
    DiskThumbnailResult  result;
    vector<Byte>         bytes;



    result.path = path;

    hr = DiskImageStore::ReadFileBytes (TextEncoding::WideToUtf8 (path), bytes);
    CHR (hr);

    result = BuildFromBytes (bytes, path);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::BuildFromBytes
//
//  The bytes loaded as a drive mounts the file and analyzed in full, then
//  each record's ring levels as the inspector builds them. A 3.5" disk is
//  not analyzed further (FR-072).
//
////////////////////////////////////////////////////////////////////////////////

DiskThumbnailResult DiskThumbnailWorker::BuildFromBytes (const vector<Byte> & bytes, const std::wstring & path)
{
    HRESULT                          hr       = S_OK;
    DiskThumbnailResult              result;
    std::shared_ptr<const DiskCopy>  copy;
    std::wstring                     reason;
    auto                             analysis = std::make_shared<DiskAnalysis>();
    vector<Byte>                     cells;
    size_t                           slot     = 0;



    result.path = path;

    hr = InspectorImageLoader::LoadBytes (bytes, TextEncoding::WideToUtf8 (path), false, copy, reason);
    CHR (hr);

    result.isOpened = true;
    DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), *analysis);
    result.is35 = analysis->image.isWoz && analysis->image.info.diskType == WozLoader::kDiskType35;

    if (!result.is35)
    {
        result.levels.resize (analysis->tracks.size());
        result.edges.resize  (analysis->tracks.size());

        for (slot = 0; slot < analysis->tracks.size(); slot++)
        {
            if (analysis->tracks[slot] != nullptr)
            {
                auto  levels = std::make_shared<PlatterRenderer::Levels>();
                auto  edges  = std::make_shared<PlatterRenderer::Levels> (1);

                PlatterCells::BuildCells  (*analysis->tracks[slot], cells);
                PlatterCells::BuildEdges  (*analysis->tracks[slot], cells, (*edges)[0]);
                PlatterCells::MapToTime   (*analysis->tracks[slot], cells);
                PlatterCells::BuildLevels (cells, *levels);
                result.levels[slot] = levels;
                result.edges[slot]  = edges;
            }
        }

        result.chips = InspectorText::BuildChips (analysis->summary);
    }

    result.analysis = analysis;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker::Run
//
//  On the worker's thread: the newest request is built, and its result is
//  stored only when no newer request has come in since.
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailWorker::Run()
{
    std::wstring         path;
    uint64_t             id       = 0;
    bool                 isNewest = false;
    DiskThumbnailResult  result;



    for (;;)
    {
        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait (lock, [this] () { return m_isStopping || m_hasWork; });

            if (m_isStopping)
            {
                break;
            }

            path      = m_path;
            id        = m_pending;
            m_hasWork = false;
        }

        result         = m_build (path);
        result.request = id;

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            isNewest = id == m_latest;

            if (isNewest)
            {
                m_result    = std::move (result);
                m_hasResult = true;
            }
        }

        if (isNewest && m_notify)
        {
            m_notify();
        }
    }
}
