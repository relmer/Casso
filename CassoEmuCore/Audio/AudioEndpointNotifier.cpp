#include "Pch.h"

#include "AudioEndpointNotifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeDefaultRenderChange
//
//  Reads and clears the pending move in one operation, so the several
//  notifications Windows sends while a device settles are serviced once.
//
////////////////////////////////////////////////////////////////////////////////

bool AudioEndpointNotifier::ConsumeDefaultRenderChange()
{
    return m_defaultRenderMoved.exchange (false, std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::QueryInterface (REFIID riid, void ** ppvObject)
{
    HRESULT  hr      = S_OK;
    bool     isKnown = false;



    CBREx (ppvObject != nullptr, E_POINTER);

    *ppvObject = nullptr;

    isKnown = (riid == __uuidof (IUnknown) || riid == __uuidof (IMMNotificationClient));
    CBREx (isKnown, E_NOINTERFACE);

    *ppvObject = static_cast<IMMNotificationClient *> (this);

    AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddRef
//
////////////////////////////////////////////////////////////////////////////////

ULONG STDMETHODCALLTYPE AudioEndpointNotifier::AddRef()
{
    return m_refCount.fetch_add (1, std::memory_order_relaxed) + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Release
//
//  Standard COM release. The last reference deletes, which is why the owner
//  holds this through a ComPtr rather than as a member: a notification still
//  in flight on an MMDevice thread keeps the object alive on its own.
//
////////////////////////////////////////////////////////////////////////////////

ULONG STDMETHODCALLTYPE AudioEndpointNotifier::Release()
{
    ULONG  remaining = m_refCount.fetch_sub (1, std::memory_order_acq_rel) - 1;



    if (remaining == 0)
    {
        delete this;
    }

    return remaining;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDefaultDeviceChanged
//
//  The only notification that matters here. Filtered to eRender / eConsole
//  because that is the pair the host stream opens; a change of the default
//  capture device or of another role moves nothing we are playing through.
//
//  Runs on an MMDevice worker thread, so it does no more than set the flag.
//  Reopening the endpoint from here would tear down the render pump from
//  under a system callback.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::OnDefaultDeviceChanged (
    EDataFlow   flow,
    ERole       role,
    LPCWSTR     deviceId)
{
    UNREFERENCED_PARAMETER (deviceId);

    if (flow == eRender && role == eConsole)
    {
        m_defaultRenderMoved.store (true, std::memory_order_release);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceStateChanged
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::OnDeviceStateChanged (LPCWSTR deviceId, DWORD newState)
{
    UNREFERENCED_PARAMETER (deviceId);
    UNREFERENCED_PARAMETER (newState);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceAdded
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::OnDeviceAdded (LPCWSTR deviceId)
{
    UNREFERENCED_PARAMETER (deviceId);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRemoved
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::OnDeviceRemoved (LPCWSTR deviceId)
{
    UNREFERENCED_PARAMETER (deviceId);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPropertyValueChanged
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE AudioEndpointNotifier::OnPropertyValueChanged (LPCWSTR deviceId, const PROPERTYKEY key)
{
    UNREFERENCED_PARAMETER (deviceId);
    UNREFERENCED_PARAMETER (key);

    return S_OK;
}
