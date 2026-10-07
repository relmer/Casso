#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IHistoryObserver
//
//  Told by reverse execution what it does to the machine's history, on the
//  thread that runs the machine: a keyframe about to be taken live, which
//  can be given bytes to keep beside it; and a keyframe's state loaded into
//  the machine, which then stands at the keyframe's position.
//
////////////////////////////////////////////////////////////////////////////////

class IHistoryObserver
{
public:
    virtual       ~IHistoryObserver() = default;

    virtual void  OnKeyframeAdding (uint64_t position, std::vector<Byte> & outSide) = 0;
    virtual void  OnKeyframeLoaded (uint64_t position) = 0;
};
