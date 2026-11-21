#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockJobs
//
//  IBlockJobs that runs each job's work and completion on the calling
//  thread as it is queued, and lists the jobs' labels.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockJobs : public IBlockJobs
{
public:
    bool                       isBusy  = false;
    int                        cancels = 0;
    std::vector<std::wstring>  labels;

    void  QueueJob (BlockInspectorWorker::Job job) override
    {
        BlockJobProgress  progress;

        labels.push_back (job.label);

        if (job.work)
        {
            job.work (progress);
        }

        if (job.finish)
        {
            job.finish();
        }
    }

    bool  IsBusy     () const override { return isBusy; }
    void  CancelJobs () override       { cancels++; }
};
