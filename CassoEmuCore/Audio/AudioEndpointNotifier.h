#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AudioEndpointNotifier
//
//  IMMNotificationClient that watches the system's default RENDER endpoint in
//  the eConsole role -- the same (flow, role) pair the host audio stream opens
//  -- and records a move of it as a one-shot flag.
//
//  A flag rather than a direct call into the audio object, because the
//  notification arrives on an MMDevice worker thread while the render pump and
//  the emulation thread are both live. The owner polls
//  ConsumeDefaultRenderChange on the one thread allowed to tear the endpoint
//  down, so the notification path needs no lock and cannot reach an object
//  that is already being destroyed: the notifier is reference-counted, so it
//  outlives its own callbacks.
//
//  Repeat notifications collapse. Windows reports the default endpoint moving
//  several times while a device settles, and one flag with one consumer is all
//  that survives.
//
//  Every other IMMNotificationClient method is a required stub. Device
//  arrival, removal, state and property changes do not move the default
//  endpoint by themselves, and when one of them does, this interface reports
//  the move through OnDefaultDeviceChanged as well.
//
////////////////////////////////////////////////////////////////////////////////

class AudioEndpointNotifier : public IMMNotificationClient
{
public:
    AudioEndpointNotifier() = default;
    virtual ~AudioEndpointNotifier() = default;

    // Takes the pending default-render-endpoint move and clears it.
    bool  ConsumeDefaultRenderChange();

    // IUnknown. These signatures are fixed from outside, so the naming rules
    // for this file's own methods do not reach them.
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void ** ppvObject) override;
    ULONG   STDMETHODCALLTYPE AddRef         () override;
    ULONG   STDMETHODCALLTYPE Release        () override;

    // IMMNotificationClient
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged (EDataFlow flow, ERole role, LPCWSTR deviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged   (LPCWSTR deviceId, DWORD newState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded          (LPCWSTR deviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved        (LPCWSTR deviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged (LPCWSTR deviceId, const PROPERTYKEY key) override;

private:
    std::atomic<ULONG>  m_refCount           { 1 };
    std::atomic<bool>   m_defaultRenderMoved { false };
};
