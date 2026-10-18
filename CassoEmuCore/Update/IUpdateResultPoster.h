#pragma once

#include "Pch.h"

#include "Update/UpdateResult.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IUpdateResultPoster
//
//  Hands a finished piece of update work from the worker thread to the UI
//  thread. The poster owns the result from the call on: the window
//  implementation posts it as a message whose receiver frees it, and frees
//  it itself when the post fails.
//
////////////////////////////////////////////////////////////////////////////////

class IUpdateResultPoster
{
public:
    virtual ~IUpdateResultPoster() = default;

    virtual void Post (std::unique_ptr<UpdateResult> result) = 0;
};
