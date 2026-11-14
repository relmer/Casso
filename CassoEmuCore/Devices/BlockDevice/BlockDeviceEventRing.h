#pragma once

#include "Pch.h"

#include "Core/SpscEventRing.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceEventRing
//
//  A block device event sink that queues each event for the UI thread. A
//  full ring drops the event and counts it.
//
////////////////////////////////////////////////////////////////////////////////

class BlockDeviceEventRing : public SpscEventRing<BlockDeviceEvent>, public IBlockDeviceEventSink
{
public:
    void OnBlockDeviceEvent (const BlockDeviceEvent & event) override
    {
        bool  pushed = TryPush (event);



        IGNORE_RETURN_VALUE (pushed, false);
    }
};
