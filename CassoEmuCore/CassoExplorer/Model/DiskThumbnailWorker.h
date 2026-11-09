#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailResult
//
//  What the preview's small platter and chips are drawn from: the disk's
//  analysis, each record's ring levels, and the summary chips (FR-072). An
//  image that does not open has isOpened false, and only its error is in
//  the preview; a 3.5" WOZ has is35 true, and a note is drawn in its place.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskThumbnailResult
{
    uint64_t                                                request  = 0;
    std::wstring                                            path;
    bool                                                    isOpened = false;
    bool                                                    is35     = false;
    std::shared_ptr<const DiskAnalysis>                     analysis;
    vector<std::shared_ptr<const PlatterRenderer::Levels>>  levels;
    vector<std::shared_ptr<const PlatterRenderer::Levels>>  edges;
    vector<SummaryChip>                                     chips;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailWorker
//
//  Builds the preview's thumbnail off the UI thread (R24, SC-009). Each
//  request replaces any that has not started, and a result is stored only
//  while its request is the newest, so a selection that moved on is dropped.
//  The notify function is called on the worker's thread once a result is
//  ready; the UI thread then takes it. The build function is Build unless
//  a test supplies another.
//
////////////////////////////////////////////////////////////////////////////////

class DiskThumbnailWorker
{
public:
    using  Notify  = std::function<void ()>;
    using  Builder = std::function<DiskThumbnailResult (const std::wstring &)>;

    explicit DiskThumbnailWorker (Notify notify, Builder build = nullptr);
    ~DiskThumbnailWorker ();

    DiskThumbnailWorker (const DiskThumbnailWorker &)             = delete;
    DiskThumbnailWorker & operator= (const DiskThumbnailWorker &) = delete;

    uint64_t  Request    (const std::wstring & path);
    void      Cancel     ();
    bool      TakeResult (DiskThumbnailResult & outResult);

    //  The thumbnail of one file, or of a file's bytes, on the calling thread.
    static DiskThumbnailResult  Build          (const std::wstring & path);
    static DiskThumbnailResult  BuildFromBytes (const vector<Byte> & bytes, const std::wstring & path);

private:
    void  Run ();

    Notify                   m_notify;
    Builder                  m_build;
    std::thread              m_thread;
    std::mutex               m_lock;
    std::condition_variable  m_wake;
    std::wstring             m_path;
    uint64_t                 m_latest     = 0;
    uint64_t                 m_pending    = 0;
    bool                     m_hasWork    = false;
    bool                     m_isStopping = false;
    bool                     m_hasResult  = false;
    DiskThumbnailResult      m_result;
};
