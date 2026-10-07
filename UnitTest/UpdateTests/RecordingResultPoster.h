#pragma once

#include "Pch.h"

#include "Update/IUpdateResultPoster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingResultPoster
//
//  Keeps every posted result for the test to read once the service's
//  workers have been joined.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingResultPoster : public IUpdateResultPoster
{
public:
    std::mutex                                  mutex;
    std::vector<std::unique_ptr<UpdateResult>>  results;

    void Post (std::unique_ptr<UpdateResult> result) override
    {
        std::lock_guard<std::mutex>  lock (mutex);

        results.push_back (std::move (result));
    }
};
