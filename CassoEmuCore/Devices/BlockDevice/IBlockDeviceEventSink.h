#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceEvent.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IBlockDeviceEventSink
//
//  Receives each completed block device operation, on the emulation thread.
//  A device with no sink attached does not build the event.
//
////////////////////////////////////////////////////////////////////////////////

class IBlockDeviceEventSink
{
public:
    virtual ~IBlockDeviceEventSink() = default;

    virtual void OnBlockDeviceEvent (const BlockDeviceEvent & event) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IBlockDeviceEventSource
//
//  A device that reports block device events to one sink, or to none.
//
////////////////////////////////////////////////////////////////////////////////

class IBlockDeviceEventSource
{
public:
    virtual ~IBlockDeviceEventSource() = default;

    virtual void SetBlockDeviceEventSink (IBlockDeviceEventSink * sink) = 0;
};
