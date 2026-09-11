#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IControllerBackendEvents
//
//  What the backend reports on its own: the set of attached controllers may
//  have changed. Raised on the controller thread.
//
////////////////////////////////////////////////////////////////////////////////

class IControllerBackendEvents
{
public:

    virtual ~IControllerBackendEvents () = default;

    virtual void OnDevicesChanged () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IControllerBackend
//
//  The only boundary between controller logic and real devices. Every call
//  is made on the controller thread. ReadSample never reports a device it
//  could not read as a healthy rest sample: a vanished device fails with
//  ERROR_DEVICE_NOT_CONNECTED and a sample whose connected flag is false.
//
//  GetWakeSources reports, for one controller, the events that signal a
//  change in its state and whether it has none and must be read on a timer.
//
////////////////////////////////////////////////////////////////////////////////

class IControllerBackend
{
public:

    virtual ~IControllerBackend () = default;

    virtual HRESULT  Initialize       (IControllerBackendEvents * pEvents)                    = 0;
    virtual void     Shutdown         ()                                                      = 0;
    virtual HRESULT  EnumerateDevices (std::vector<ControllerDeviceInfo> & outDevices)        = 0;
    virtual HRESULT  ReadSample       (const ControllerUnitKey & unit, ControllerSample & outSample) = 0;
    virtual void     GetWakeSources   (const ControllerUnitKey   & unit,
                                       std::vector<HANDLE>       & outEvents,
                                       bool                      & outNeedsTimedPoll)          = 0;
};
