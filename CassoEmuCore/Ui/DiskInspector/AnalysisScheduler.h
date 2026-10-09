#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Shell/BackgroundWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordResult
//
//  One record's analysis, with the copy it was made from.
//
////////////////////////////////////////////////////////////////////////////////

struct RecordResult
{
    uint64_t                              mediaId = 0;
    int                                   slot    = -1;
    std::shared_ptr<const DiskCopy>       copy;
    std::shared_ptr<const TrackAnalysis>  analysis;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler
//
//  Analyzes a disk's records on one background thread so the window keeps
//  responding (FR-021). Work is per record and the newest request for a
//  record wins: a pending request is replaced, and a result whose request has
//  since been replaced, or whose disk is no longer the one shown, is dropped.
//  Results wait in a list the owner takes on its own thread; notify is called
//  on the background thread after each one, to wake the owner.
//
////////////////////////////////////////////////////////////////////////////////

class AnalysisScheduler
{
public:
    using Notify = std::function<void ()>;

    explicit AnalysisScheduler (Notify notify);
    ~AnalysisScheduler ();

    AnalysisScheduler (const AnalysisScheduler &)             = delete;
    AnalysisScheduler & operator= (const AnalysisScheduler &) = delete;

    void  Restart     (std::shared_ptr<const DiskCopy> copy, const DecodeSettings & settings);
    void  Update      (std::shared_ptr<const DiskCopy> copy, std::span<const int> slots);
    bool  IsPending   (int slot) const;
    bool  HasPending  () const;
    void  TakeResults (vector<RecordResult> & outResults);

private:
    struct Request
    {
        std::shared_ptr<const DiskCopy>  copy;
        uint64_t                         generation = 0;
    };

    void  RequestLocked (std::shared_ptr<const DiskCopy> copy, int slot);
    void  RunOne        ();

    Notify                    m_notify;
    mutable std::mutex        m_lock;
    uint64_t                  m_mediaId     = 0;
    DecodeSettings            m_settings;
    uint64_t                  m_generation  = 0;
    std::map<int, Request>    m_pending;
    std::map<int, uint64_t>   m_wanted;
    int                       m_running     = -1;
    vector<RecordResult>      m_results;
    std::atomic<bool>         m_isStopping  = false;
    BackgroundWorkQueue       m_queue;
};
