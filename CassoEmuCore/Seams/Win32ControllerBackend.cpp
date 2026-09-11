#include "Pch.h"

#include "Seams/Win32ControllerBackend.h"

#include "Controllers/XInputSampleDecoder.h"

#pragma comment (lib, "xinput.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "hid.lib")





struct AxisGuidSlot
{
    const GUID  * pGuid;
    int           axisIndex;
};

// DirectInput axis object types to sample axis slots, in DIJOYSTATE2 order.
// The two sliders share a GUID and are taken in enumeration order.
static constexpr AxisGuidSlot  s_kAxisGuids[] =
{
    { &GUID_XAxis,  0 },
    { &GUID_YAxis,  1 },
    { &GUID_ZAxis,  2 },
    { &GUID_RxAxis, 3 },
    { &GUID_RyAxis, 4 },
    { &GUID_RzAxis, 5 },
};

// What object enumeration is filling in while it walks one device.
struct EnumObjectContext
{
    DirectInputObjectLayout  * pLayout    = nullptr;
    int                        sliderSeen = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ~Win32ControllerBackend
//
////////////////////////////////////////////////////////////////////////////////

Win32ControllerBackend::~Win32ControllerBackend()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Controller thread. The window, the HID notification registration and the
//  DirectInput interface all belong to this thread; nothing here touches a
//  device, which EnumerateDevices does.
//
//  A missing DirectInput is not fatal: XInput controllers still work, and the
//  reverse holds too. Only losing both fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::Initialize (IControllerBackendEvents * pEvents)
{
    HRESULT  hr        = S_OK;
    HRESULT  hrWindow  = S_OK;
    HRESULT  hrDirect  = S_OK;



    CBRAEx (pEvents != nullptr, E_INVALIDARG);

    m_events = pEvents;

    hrWindow = CreateNotifyWindow();
    IGNORE_RETURN_VALUE (hrWindow, S_OK);

    hrDirect = OpenDirectInput();
    IGNORE_RETURN_VALUE (hrDirect, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void Win32ControllerBackend::Shutdown()
{
    CloseDevices();

    if (m_directInput != nullptr)
    {
        m_directInput->Release();
        m_directInput = nullptr;
    }

    if (m_notifyHandle != nullptr)
    {
        UnregisterDeviceNotification (m_notifyHandle);
        m_notifyHandle = nullptr;
    }

    if (m_notifyWindow != nullptr)
    {
        DestroyWindow (m_notifyWindow);
        m_notifyWindow = nullptr;
    }

    m_events = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateNotifyWindow
//
//  A message-only window, registered for HID interface arrivals and removals.
//  Xbox controllers present HID interfaces too, so one registration covers
//  both device kinds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::CreateNotifyWindow()
{
    constexpr const wchar_t  * kpszClass = L"CassoControllerNotify";



    HRESULT                             hr     = S_OK;
    WNDCLASSW                           wc     = {};
    DEV_BROADCAST_DEVICEINTERFACE_W     filter = {};



    wc.lpfnWndProc   = NotifyWindowProc;
    wc.hInstance     = GetModuleHandleW (nullptr);
    wc.lpszClassName = kpszClass;
    RegisterClassW (&wc);   // a second instance re-registering is harmless

    m_notifyWindow = CreateWindowExW (0, kpszClass, L"", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, wc.hInstance, this);
    CWRA (m_notifyWindow);

    SetWindowLongPtrW (m_notifyWindow, GWLP_USERDATA, (LONG_PTR) this);

    filter.dbcc_size       = sizeof (filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid  = GUID_DEVINTERFACE_HID;

    m_notifyHandle = RegisterDeviceNotificationW (m_notifyWindow, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    CWRA (m_notifyHandle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyWindowProc
//
//  An arrival or removal is not read immediately: the device that raised it
//  may not be readable yet, and a removed one may still enumerate. Two timers
//  rescan shortly after and again later, and each raises the event once.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ControllerBackend::NotifyWindowProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Win32ControllerBackend  * pThis = (Win32ControllerBackend *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);



    if (msg == WM_DEVICECHANGE && (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE))
    {
        SetTimer (hwnd, kRescanSoonTimerId, kRescanSoonMs, nullptr);
        SetTimer (hwnd, kRescanLateTimerId, kRescanLateMs, nullptr);

        return TRUE;
    }

    if (msg == WM_TIMER && (wp == kRescanSoonTimerId || wp == kRescanLateTimerId))
    {
        KillTimer (hwnd, (UINT_PTR) wp);

        if (pThis != nullptr && pThis->m_events != nullptr)
        {
            pThis->m_events->OnDevicesChanged();
        }

        return 0;
    }

    return DefWindowProcW (hwnd, msg, wp, lp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDirectInput
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::OpenDirectInput()
{
    HRESULT  hr = S_OK;



    hr = DirectInput8Create (GetModuleHandleW (nullptr), DIRECTINPUT_VERSION,
                             IID_IDirectInput8W, (void **) &m_directInput, nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnumerateDevices
//
//  Every attached controller exactly once: the Xbox-class controllers first,
//  as a SINGLE entry backed by the lowest connected XInput slot (they share
//  one model key, FR-018a), then the DirectInput devices that are not XInput
//  devices in disguise.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::EnumerateDevices (std::vector<ControllerDeviceInfo> & outDevices)
{
    HRESULT  hr         = S_OK;
    int      lowestXbox = -1;



    outDevices.clear();
    m_xinputConnected.reset();

    for (int slot = 0; slot < kXInputSlotCount; slot++)
    {
        XINPUT_STATE  state  = {};
        DWORD         result = XInputGetState ((DWORD) slot, &state);

        if (result == ERROR_SUCCESS)
        {
            m_xinputConnected.set ((size_t) slot);

            if (lowestXbox < 0)
            {
                lowestXbox = slot;
            }
        }
    }

    if (lowestXbox >= 0)
    {
        ControllerDeviceInfo  info;

        info.unit.model.kind = ControllerKind::XInput;
        info.description     = GetXInputDescription ((DWORD) lowestXbox);
        info.xinputSlot      = lowestXbox;
        info.controls        = XInputSampleDecoder::ListControls();
        outDevices.push_back (info);
    }

    CloseDevices();

    if (m_directInput != nullptr)
    {
        hr = m_directInput->EnumDevices (DI8DEVCLASS_GAMECTRL, EnumDeviceCallback, this, DIEDFL_ATTACHEDONLY);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const DirectInputDevice & device : m_diDevices)
    {
        ControllerDeviceInfo  info;

        info.unit     = device.unit;
        info.controls = DirectInputSampleDecoder::ListControls (device.layout);
        outDevices.push_back (info);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnumDeviceCallback
//
////////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK Win32ControllerBackend::EnumDeviceCallback (const DIDEVICEINSTANCEW * pInstance, void * pContext)
{
    Win32ControllerBackend  * pThis = (Win32ControllerBackend *) pContext;
    HRESULT                   hr    = S_OK;



    if (pThis != nullptr && pInstance != nullptr)
    {
        hr = pThis->AddDirectInputDevice (*pInstance);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    return DIENUM_CONTINUE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDirectInputDevice
//
//  Opens one device, unless its path says it is an XInput device: those are
//  read through XInput, where the two triggers are separate axes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::AddDirectInputDevice (const DIDEVICEINSTANCEW & instance)
{
    HRESULT            hr        = S_OK;
    DirectInputDevice  opened;
    std::wstring       path;
    bool               isXInput  = false;
    DIDEVCAPS          caps      = {};
    EnumObjectContext  context;
    DIPROPRANGE        range     = {};



    hr = m_directInput->CreateDevice (instance.guidInstance, &opened.device, nullptr);
    CHR (hr);

    path     = GetDevicePath (*opened.device);
    isXInput = IsXInputPath (path);

    // An Xbox controller enumerates here as well, and reading it through
    // DirectInput would collapse its two triggers onto one axis. It is read
    // through XInput instead, so skipping it is success, not failure.
    BAIL_OUT_IF (isXInput, S_OK);

    opened.unit = MakeUnitKey (*opened.device, instance);

    caps.dwSize = sizeof (caps);
    hr          = opened.device->GetCapabilities (&caps);
    CHR (hr);

    opened.isPolled          = (caps.dwFlags & DIDC_POLLEDDEVICE) != 0;
    opened.layout.buttonCount = (int) caps.dwButtons;
    opened.layout.hatCount    = (int) caps.dwPOVs;

    hr = opened.device->SetDataFormat (&c_dfDIJoystick2);
    CHR (hr);

    hr = opened.device->SetCooperativeLevel (m_notifyWindow, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    IGNORE_RETURN_VALUE (hr, S_OK);

    context.pLayout = &opened.layout;
    hr              = opened.device->EnumObjects (EnumObjectCallback, &context, DIDFT_AXIS);
    IGNORE_RETURN_VALUE (hr, S_OK);

    // Every axis reports over the same range, so the decoder needs no
    // per-device scaling.
    range.diph.dwSize       = sizeof (range);
    range.diph.dwHeaderSize = sizeof (DIPROPHEADER);
    range.diph.dwHow        = DIPH_DEVICE;
    range.lMin              = -32768;
    range.lMax              = 32767;
    hr                      = opened.device->SetProperty (DIPROP_RANGE, &range.diph);
    IGNORE_RETURN_VALUE (hr, S_OK);

    opened.event = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CWRA (opened.event);

    hr = opened.device->SetEventNotification (opened.event);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = opened.device->Acquire();
    opened.isAcquired = SUCCEEDED (hr);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_diDevices.push_back (opened);
    opened.device = nullptr;   // owned by the list now
    opened.event  = nullptr;

Error:
    if (opened.device != nullptr)
    {
        opened.device->Release();
    }

    if (opened.event != nullptr)
    {
        CloseHandle (opened.event);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnumObjectCallback
//
//  Records which axis slots the device actually reports. A device with no Z
//  axis must not read DIJOYSTATE2's zero there as an axis resting at center.
//
////////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK Win32ControllerBackend::EnumObjectCallback (const DIDEVICEOBJECTINSTANCEW * pObject, void * pContext)
{
    constexpr int        kFirstSliderAxis = 6;
    EnumObjectContext  * pContextTyped    = (EnumObjectContext *) pContext;



    if (pObject == nullptr || pContextTyped == nullptr || pContextTyped->pLayout == nullptr)
    {
        return DIENUM_CONTINUE;
    }

    for (const AxisGuidSlot & entry : s_kAxisGuids)
    {
        if (pObject->guidType == *entry.pGuid)
        {
            pContextTyped->pLayout->presentAxes.set ((size_t) entry.axisIndex);

            return DIENUM_CONTINUE;
        }
    }

    if (pObject->guidType == GUID_Slider && pContextTyped->sliderSeen < 2)
    {
        pContextTyped->pLayout->presentAxes.set ((size_t) (kFirstSliderAxis + pContextTyped->sliderSeen));
        pContextTyped->sliderSeen++;
    }

    return DIENUM_CONTINUE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloseDevices
//
////////////////////////////////////////////////////////////////////////////////

void Win32ControllerBackend::CloseDevices()
{
    for (DirectInputDevice & device : m_diDevices)
    {
        if (device.device != nullptr)
        {
            device.device->Unacquire();
            device.device->SetEventNotification (nullptr);
            device.device->Release();
        }

        if (device.event != nullptr)
        {
            CloseHandle (device.event);
        }
    }

    m_diDevices.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSample
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::ReadSample (const ControllerUnitKey & unit, ControllerSample & outSample)
{
    HRESULT              hr     = S_OK;
    DirectInputDevice  * device = nullptr;



    outSample = ControllerSample();

    if (unit.model.kind == ControllerKind::XInput)
    {
        hr = ReadXInput (unit, outSample);
        CHR (hr);
    }
    else
    {
        device = FindDirectInputDevice (unit);

        CBREx (device != nullptr, HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED));

        hr = ReadDirectInput (*device, outSample);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadXInput
//
//  The lowest connected slot backs the single Xbox-class entry. An unchanged
//  packet number means the state did not change, so the previous sample is
//  returned without decoding it again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::ReadXInput (const ControllerUnitKey & unit, ControllerSample & outSample)
{
    HRESULT             hr      = S_OK;
    int                 slot    = -1;
    XINPUT_STATE        state   = {};
    DWORD               result  = ERROR_DEVICE_NOT_CONNECTED;
    XInputGamepadState  gamepad;



    UNREFERENCED_PARAMETER (unit);

    for (int index = 0; index < kXInputSlotCount && slot < 0; index++)
    {
        if (m_xinputConnected.test ((size_t) index))
        {
            slot = index;
        }
    }

    CBREx (slot >= 0, HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED));

    result = XInputGetState ((DWORD) slot, &state);

    if (result != ERROR_SUCCESS)
    {
        m_xinputConnected.reset ((size_t) slot);
        m_xinputSlots[(size_t) slot].hasSample = false;
    }

    CBREx (result == ERROR_SUCCESS, HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED));

    if (m_xinputSlots[(size_t) slot].hasSample &&
        m_xinputSlots[(size_t) slot].lastPacket == state.dwPacketNumber)
    {
        outSample = m_xinputSlots[(size_t) slot].sample;
        return hr;
    }

    gamepad.buttons      = state.Gamepad.wButtons;
    gamepad.leftTrigger  = state.Gamepad.bLeftTrigger;
    gamepad.rightTrigger = state.Gamepad.bRightTrigger;
    gamepad.thumbLX      = state.Gamepad.sThumbLX;
    gamepad.thumbLY      = state.Gamepad.sThumbLY;
    gamepad.thumbRX      = state.Gamepad.sThumbRX;
    gamepad.thumbRY      = state.Gamepad.sThumbRY;

    outSample = XInputSampleDecoder::Decode (gamepad);

    m_xinputSlots[(size_t) slot].lastPacket = state.dwPacketNumber;
    m_xinputSlots[(size_t) slot].sample     = outSample;
    m_xinputSlots[(size_t) slot].hasSample   = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDirectInput
//
//  A lost or unacquired device is reacquired once before the read is reported
//  as a disconnect, which is the ordinary case after the session that owned
//  it exclusively goes away.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ControllerBackend::ReadDirectInput (DirectInputDevice & device, ControllerSample & outSample)
{
    HRESULT                   hr         = S_OK;
    HRESULT                   hrState    = S_OK;
    HRESULT                   hrAcquire  = S_OK;
    bool                      hasState   = false;
    DIJOYSTATE2               state      = {};
    DirectInputJoystickState  mirror;



    device.device->Poll();
    hrState = device.device->GetDeviceState (sizeof (state), &state);

    if (hrState == DIERR_INPUTLOST || hrState == DIERR_NOTACQUIRED)
    {
        hrAcquire         = device.device->Acquire();
        device.isAcquired = SUCCEEDED (hrAcquire);

        device.device->Poll();
        hrState = device.device->GetDeviceState (sizeof (state), &state);
    }

    hasState = SUCCEEDED (hrState);

    CBREx (hasState, HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED));

    mirror.x  = state.lX;
    mirror.y  = state.lY;
    mirror.z  = state.lZ;
    mirror.rx = state.lRx;
    mirror.ry = state.lRy;
    mirror.rz = state.lRz;

    for (int slider = 0; slider < DirectInputJoystickState::kSliderCount; slider++)
    {
        mirror.slider[slider] = state.rglSlider[slider];
    }

    for (int hat = 0; hat < DirectInputJoystickState::kPovCount; hat++)
    {
        mirror.pov[hat] = state.rgdwPOV[hat];
    }

    for (int button = 0; button < DirectInputJoystickState::kButtonCount; button++)
    {
        mirror.buttons[button] = state.rgbButtons[button];
    }

    outSample = DirectInputSampleDecoder::Decode (mirror, device.layout);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWakeSources
//
//  A DirectInput device signals its own event when its state changes, unless
//  it is a polled device. XInput has no such event, so an Xbox-class
//  controller is the one case that needs a timer.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ControllerBackend::GetWakeSources (
    const ControllerUnitKey  & unit,
    std::vector<HANDLE>      & outEvents,
    bool                     & outNeedsTimedPoll)
{
    DirectInputDevice  * device = nullptr;



    outEvents.clear();
    outNeedsTimedPoll = false;

    if (unit.model.kind == ControllerKind::XInput)
    {
        outNeedsTimedPoll = true;

        return;
    }

    device = FindDirectInputDevice (unit);

    if (device == nullptr)
    {
        return;
    }

    if (device->event != nullptr)
    {
        outEvents.push_back (device->event);
    }

    outNeedsTimedPoll = device->isPolled || device->event == nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDirectInputDevice
//
////////////////////////////////////////////////////////////////////////////////

Win32ControllerBackend::DirectInputDevice * Win32ControllerBackend::FindDirectInputDevice (const ControllerUnitKey & unit)
{
    auto  found = std::find_if (m_diDevices.begin(), m_diDevices.end(),
                                [&unit] (const DirectInputDevice & device) { return device.unit == unit; });



    return found != m_diDevices.end() ? &*found : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsXInputPath
//
//  Every XInput device's interface path carries IG_, which is how an Xbox
//  controller is told from an ordinary game controller without WMI. Confirmed
//  on this machine for USB and Bluetooth alike.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ControllerBackend::IsXInputPath (const std::wstring & path) const
{
    std::wstring  upper = path;



    std::transform (upper.begin(), upper.end(), upper.begin(), [] (wchar_t ch) { return (wchar_t) towupper (ch); });

    return upper.find (L"IG_") != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDevicePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ControllerBackend::GetDevicePath (IDirectInputDevice8W & device) const
{
    HRESULT            hr   = S_OK;
    DIPROPGUIDANDPATH  prop = {};



    prop.diph.dwSize       = sizeof (prop);
    prop.diph.dwHeaderSize = sizeof (DIPROPHEADER);
    prop.diph.dwHow        = DIPH_DEVICE;

    hr = device.GetProperty (DIPROP_GUIDANDPATH, &prop.diph);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return SUCCEEDED (hr) ? std::wstring (prop.wszPath) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToNarrow
//
//  Unit ids are stored in JSON, which is narrow text. A serial number is
//  whatever the device reports, so the conversion goes through UTF-8 rather
//  than dropping the high half of each character: a mangled serial is a key
//  that recognizes the wrong controller, or no controller at all.
//
////////////////////////////////////////////////////////////////////////////////

std::string Win32ControllerBackend::ToNarrow (const std::wstring & wide)
{
    std::string  narrow;
    int          needed = 0;



    if (wide.empty())
    {
        return narrow;
    }

    needed = WideCharToMultiByte (CP_UTF8, 0, wide.c_str(), (int) wide.size(), nullptr, 0, nullptr, nullptr);

    if (needed <= 0)
    {
        return narrow;
    }

    narrow.resize ((size_t) needed);
    WideCharToMultiByte (CP_UTF8, 0, wide.c_str(), (int) wide.size(), narrow.data(), needed, nullptr, nullptr);

    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSerialNumber
//
//  The HID serial number, when the device reports one, which is what lets a
//  controller be recognized as the same unit after it moves to another port.
//  Many controllers report none; the caller falls back to the instance GUID.
//
////////////////////////////////////////////////////////////////////////////////

std::string Win32ControllerBackend::GetSerialNumber (const std::wstring & path) const
{
    constexpr size_t  kSerialChars = 128;



    std::string  serial;
    HANDLE       file                 = INVALID_HANDLE_VALUE;
    wchar_t      buffer[kSerialChars] = {};
    BOOLEAN      gotSerial            = FALSE;



    if (path.empty())
    {
        return serial;
    }

    file = CreateFileW (path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, 0, nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        return serial;
    }

    gotSerial = HidD_GetSerialNumberString (file, buffer, (ULONG) sizeof (buffer));
    CloseHandle (file);

    if (gotSerial)
    {
        serial = ToNarrow (buffer);
    }

    return serial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeUnitKey
//
//  Model from the device's vendor and product; unit from its serial number
//  when it has one, otherwise from the instance GUID, which DirectInput
//  documents as reusable on this computer.
//
////////////////////////////////////////////////////////////////////////////////

ControllerUnitKey Win32ControllerBackend::MakeUnitKey (
    IDirectInputDevice8W       & device,
    const DIDEVICEINSTANCEW    & instance) const
{
    HRESULT            hr           = S_OK;
    ControllerUnitKey  unit;
    DIPROPDWORD        vidPid       = {};
    std::wstring       path         = GetDevicePath (device);
    std::string        serial       = GetSerialNumber (path);
    wchar_t            guidText[64] = {};



    unit.model.kind = ControllerKind::DirectInput;

    vidPid.diph.dwSize       = sizeof (vidPid);
    vidPid.diph.dwHeaderSize = sizeof (DIPROPHEADER);
    vidPid.diph.dwHow        = DIPH_DEVICE;

    hr = device.GetProperty (DIPROP_VIDPID, &vidPid.diph);

    if (SUCCEEDED (hr))
    {
        unit.model.vendorId  = LOWORD (vidPid.dwData);
        unit.model.productId = HIWORD (vidPid.dwData);
    }

    if (!serial.empty())
    {
        unit.unitId = serial;
        unit.source = ControllerUnitSource::Serial;
    }
    else if (StringFromGUID2 (instance.guidInstance, guidText, ARRAYSIZE (guidText)) > 0)
    {
        unit.unitId = ToNarrow (guidText);
        unit.source = ControllerUnitSource::InstanceGuid;
    }

    return unit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetXInputDescription
//
//  XInputGetCapabilitiesEx, ordinal 108 of xinput1_4.dll, reports the slot's
//  vendor and product. It is undocumented and used for the description only:
//  the model key is the same for every Xbox-class controller (FR-018a), so a
//  missing export costs nothing but the numbers in the text.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ControllerBackend::GetXInputDescription (DWORD slot) const
{
    struct CapabilitiesEx
    {
        XINPUT_CAPABILITIES  caps;
        WORD                 vendorId;
        WORD                 productId;
        WORD                 revisionId;
        DWORD                reserved;
    };

    using CapabilitiesExFn = DWORD (WINAPI *) (DWORD, DWORD, DWORD, CapabilitiesEx *);

    HMODULE           module      = GetModuleHandleW (L"xinput1_4.dll");
    CapabilitiesExFn  getCapsEx   = nullptr;
    CapabilitiesEx    capsEx      = {};
    DWORD             result      = ERROR_DEVICE_NOT_CONNECTED;



    if (module == nullptr)
    {
        module = LoadLibraryW (L"xinput1_4.dll");
    }

    if (module != nullptr)
    {
        getCapsEx = (CapabilitiesExFn) GetProcAddress (module, MAKEINTRESOURCEA (kCapabilitiesOrdinal));
    }

    if (getCapsEx != nullptr)
    {
        result = getCapsEx (1, slot, 0, &capsEx);
    }

    if (result != ERROR_SUCCESS)
    {
        return L"Xbox Controller";
    }

    return std::format (L"Xbox Controller ({:04x}:{:04x})", capsEx.vendorId, capsEx.productId);
}
