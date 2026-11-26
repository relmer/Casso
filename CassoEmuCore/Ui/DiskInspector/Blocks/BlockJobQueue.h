#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockJobQueue
//
//  Block mode's jobs on the window's BlockInspectorWorker, and the progress
//  of a long job, for the panel's progress row. With no worker, as in a
//  test, a job runs on the calling thread.
//
////////////////////////////////////////////////////////////////////////////////

class BlockJobQueue : public IBlockJobs
{
public:
    using QueuedFn = std::function<void ()>;

    void  SetWorker   (BlockInspectorWorker * worker) { m_worker   = worker; }
    void  SetOnQueued (QueuedFn fn)                   { m_onQueued = std::move (fn); }
    bool  HasWorker   () const                        { return m_worker != nullptr; }

    void  QueueJob   (BlockInspectorWorker::Job job) override;
    bool  IsBusy     () const override { return m_worker != nullptr && m_worker->IsBusy(); }
    void  CancelJobs () override       { if (m_worker != nullptr) { m_worker->Cancel(); } }

    //  Reads the long job's label and progress from the worker.
    void  Poll ();

    bool  IsLongJob () const { return m_isLongJob; }
    bool  CanCancel () const { return m_canCancel; }

    //  The share of the long job done, from 0 to 1, and its label with the
    //  percent done when the job has a total.
    double        GetShare           () const;
    std::wstring  BuildProgressLabel () const;

private:
    BlockInspectorWorker  * m_worker    = nullptr;
    QueuedFn                m_onQueued;
    std::wstring            m_label;
    uint64_t                m_done      = 0;
    uint64_t                m_total     = 0;
    bool                    m_isLongJob = false;
    bool                    m_canCancel = false;
};
