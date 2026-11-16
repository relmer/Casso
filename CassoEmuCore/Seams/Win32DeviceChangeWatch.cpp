#include "Pch.h"

#include "Seams/Win32DeviceChangeWatch.h"

//  winioctl.h only DECLARES the disk and volume device-interface GUIDs;
//  something has to define them. They are defined here, in this
//  translation unit alone, rather than by pulling <initguid.h> into the Pch.
extern "C" const GUID  DECLSPEC_SELECTANY  GUID_DEVINTERFACE_DISK_LOCAL =
    { 0x53F56307, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B } };
extern "C" const GUID  DECLSPEC_SELECTANY  GUID_DEVINTERFACE_VOLUME_LOCAL =
    { 0x53F5630D, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B } };





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DeviceChangeWatch::~Win32DeviceChangeWatch
//
////////////////////////////////////////////////////////////////////////////////

Win32DeviceChangeWatch::~Win32DeviceChangeWatch()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DeviceChangeWatch::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DeviceChangeWatch::Start (ChangeFn onChange)
{
    constexpr const wchar_t  * kpszClass = L"CassoDeviceChangeWatch";



    HRESULT                          hr     = S_OK;
    WNDCLASSW                        wc     = {};
    DEV_BROADCAST_DEVICEINTERFACE_W  filter = {};
    HDEVNOTIFY                       notify = nullptr;



    Stop();

    m_onChange = std::move (onChange);

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = GetModuleHandleW (nullptr);
    wc.lpszClassName = kpszClass;
    RegisterClassW (&wc);   // a second instance re-registering is harmless

    m_window = CreateWindowExW (0, kpszClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    CWRA (m_window);

    SetWindowLongPtrW (m_window, GWLP_USERDATA, (LONG_PTR) this);

    filter.dbcc_size       = sizeof (filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    for (const GUID * guid : { &GUID_DEVINTERFACE_DISK_LOCAL, &GUID_DEVINTERFACE_VOLUME_LOCAL })
    {
        filter.dbcc_classguid = *guid;
        notify                = RegisterDeviceNotificationW (m_window, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
        CWRA (notify);

        m_notifies.push_back (notify);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DeviceChangeWatch::Stop
//
////////////////////////////////////////////////////////////////////////////////

void Win32DeviceChangeWatch::Stop()
{
    for (HDEVNOTIFY notify : m_notifies)
    {
        UnregisterDeviceNotification (notify);
    }

    m_notifies.clear();

    if (m_window != nullptr)
    {
        DestroyWindow (m_window);
        m_window = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DeviceChangeWatch::WindowProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32DeviceChangeWatch::WindowProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Win32DeviceChangeWatch  * pThis = (Win32DeviceChangeWatch *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);



    if (msg == WM_DEVICECHANGE && (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE))
    {
        if (pThis != nullptr && pThis->m_onChange)
        {
            pThis->m_onChange();
        }

        return TRUE;
    }

    return DefWindowProcW (hwnd, msg, wp, lp);
}
