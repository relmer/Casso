#pragma once

#include "Pch.h"

#include "Controllers/DirectInputSampleDecoder.h"
#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ControllerBackend
//
//  Real controllers: XInput for Xbox-class devices, DirectInput 8 for the
//  rest, and HID device notifications for hot-plug. Every method runs on the
//  controller thread, which is also where the message-only window this owns
//  is created and pumped.
//
//  The decoding rules are not here; they are in the sample decoders, which
//  tests drive with synthetic device states. What is left here is the part
//  that cannot be tested without hardware: opening devices, reading them,
//  and noticing when the set changes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ControllerBackend : public IControllerBackend
{
public:

    ~Win32ControllerBackend () override;

    HRESULT  Initialize       (IControllerBackendEvents * pEvents) override;
    void     Shutdown         () override;
    HRESULT  EnumerateDevices (std::vector<ControllerDeviceInfo> & outDevices) override;
    HRESULT  ReadSample       (const ControllerUnitKey & unit, ControllerSample & outSample) override;
    void     GetWakeSources   (const ControllerUnitKey & unit,
                               std::vector<HANDLE>     & outEvents,
                               bool                    & outNeedsTimedPoll) override;
    bool     RecheckXInputSlots () override;

private:

    // One opened DirectInput device and what enumeration found on it.
    struct DirectInputDevice
    {
        ControllerUnitKey          unit;
        std::wstring               description;
        ControllerFormFactor       formFactor = ControllerFormFactor::Joystick;
        IDirectInputDevice8W     * device     = nullptr;
        HANDLE                     event      = nullptr;
        DirectInputObjectLayout    layout;
        bool                       isPolled   = false;
        bool                       isAcquired = false;
    };

    // One XInput slot that reported a controller.
    struct XInputSlot
    {
        DWORD             index      = 0;
        DWORD             lastPacket = 0;
        bool              hasSample  = false;
        ControllerSample  sample;
    };

    static constexpr int    kXInputSlotCount     = 4;
    static constexpr int    kCapabilitiesOrdinal = 108;
    static constexpr UINT   kRescanSoonTimerId   = 1;
    static constexpr UINT   kRescanLateTimerId   = 2;
    static constexpr UINT   kRescanSoonMs        = 300;
    static constexpr UINT   kRescanLateMs        = 2000;

    static LRESULT CALLBACK NotifyWindowProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static BOOL    CALLBACK EnumDeviceCallback (const DIDEVICEINSTANCEW * pInstance, void * pContext);
    static ControllerFormFactor  GetFormFactor (const DIDEVICEINSTANCEW & instance);
    static BOOL    CALLBACK EnumObjectCallback (const DIDEVICEOBJECTINSTANCEW * pObject, void * pContext);

    HRESULT  CreateNotifyWindow  ();
    HRESULT  OpenDirectInput     ();
    void     CloseDevices        ();
    HRESULT  AddDirectInputDevice (const DIDEVICEINSTANCEW & instance);
    HRESULT  ReadDirectInput     (DirectInputDevice & device, ControllerSample & outSample);
    HRESULT  ReadXInput          (const ControllerUnitKey & unit, ControllerSample & outSample);
    bool     IsXInputPath        (const std::wstring & path) const;
    std::wstring  GetDevicePath  (IDirectInputDevice8W & device) const;
    std::string   GetSerialNumber (const std::wstring & path) const;
    static std::string  ToNarrow (const std::wstring & wide);
    ControllerUnitKey  MakeUnitKey (IDirectInputDevice8W & device, const DIDEVICEINSTANCEW & instance) const;
    std::wstring       GetXInputDescription (DWORD slot) const;

    DirectInputDevice *  FindDirectInputDevice (const ControllerUnitKey & unit);

    IControllerBackendEvents                  * m_events          = nullptr;
    IDirectInput8W                            * m_directInput     = nullptr;
    HWND                                        m_notifyWindow    = nullptr;
    HDEVNOTIFY                                  m_notifyHandle    = nullptr;
    std::vector<DirectInputDevice>              m_diDevices;
    std::array<XInputSlot, kXInputSlotCount>    m_xinputSlots;
    std::bitset<kXInputSlotCount>               m_xinputConnected;
};
