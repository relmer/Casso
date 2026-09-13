#pragma once

#include "Pch.h"

#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeControllerBackend
//
//  Scripted controllers for tests: add and remove devices, set what each one
//  reads, make the next read fail, and raise the device-change event. A
//  device that is not attached fails its read the way a vanished real device
//  does, so a test can never mistake "unplugged" for "at rest".
//
////////////////////////////////////////////////////////////////////////////////

class FakeControllerBackend : public IControllerBackend
{
public:

    struct Device
    {
        ControllerDeviceInfo  info;
        ControllerSample      sample;
        bool                  needsTimedPoll = false;
    };

    HRESULT Initialize (IControllerBackendEvents * pEvents) override
    {
        events = pEvents;
        initializeCount++;
        return S_OK;
    }

    void Shutdown() override
    {
        events = nullptr;
        shutdownCount++;
    }

    HRESULT EnumerateDevices (std::vector<ControllerDeviceInfo> & outDevices) override
    {
        outDevices.clear();

        for (const Device & device : devices)
        {
            outDevices.push_back (device.info);
        }

        enumerateCount++;
        return S_OK;
    }

    HRESULT ReadSample (const ControllerUnitKey & unit, ControllerSample & outSample) override
    {
        HRESULT         hr     = S_OK;
        const Device  * device = FindDevice (unit);



        readCount++;
        outSample = ControllerSample();

        if (failNextRead != S_OK)
        {
            hr           = failNextRead;
            failNextRead = S_OK;
        }
        else if (device == nullptr)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
        }
        else
        {
            outSample           = device->sample;
            outSample.connected = true;
        }

        return hr;
    }

    void GetWakeSources (const ControllerUnitKey & unit, std::vector<HANDLE> & outEvents, bool & outNeedsTimedPoll) override
    {
        const Device  * device = FindDevice (unit);



        outEvents.clear();
        outNeedsTimedPoll = device != nullptr && device->needsTimedPoll;
    }

    void AddDevice (const ControllerDeviceInfo & info, bool needsTimedPoll = false)
    {
        Device  device;

        device.info           = info;
        device.needsTimedPoll = needsTimedPoll;
        devices.push_back (device);
    }

    void RemoveDevice (const ControllerUnitKey & unit)
    {
        std::erase_if (devices, [&unit] (const Device & device) { return device.info.unit == unit; });
    }

    void SetSample (const ControllerUnitKey & unit, const ControllerSample & sample)
    {
        Device  * device = FindDevice (unit);

        if (device != nullptr)
        {
            device->sample = sample;
        }
    }

    void RaiseDevicesChanged()
    {
        if (events != nullptr)
        {
            events->OnDevicesChanged();
        }
    }

    Device * FindDevice (const ControllerUnitKey & unit)
    {
        auto  found = std::find_if (devices.begin(), devices.end(), [&unit] (const Device & device) { return device.info.unit == unit; });

        return found != devices.end() ? &*found : nullptr;
    }

    std::vector<Device>          devices;
    IControllerBackendEvents   * events          = nullptr;
    HRESULT                      failNextRead    = S_OK;
    int                          initializeCount = 0;
    int                          shutdownCount   = 0;
    int                          enumerateCount  = 0;
    int                          readCount       = 0;
};
