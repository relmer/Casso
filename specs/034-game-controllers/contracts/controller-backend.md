# Contract: Controller Backend Seam

**Feature**: `034-game-controllers` | **Data model**: [../data-model.md](../data-model.md)

The only boundary between controller logic and real devices. Follows the `IHostCapsLock` / `Win32HostCapsLock` / `FakeHostCapsLock` precedent (`CassoEmuCore/Seams/`).

## Interface

`CassoEmuCore/Seams/IControllerBackend.h`

```cpp
class IControllerBackend
{
public:

    virtual ~IControllerBackend () = default;

    virtual HRESULT Initialize         (IControllerBackendEvents * pEvents) = 0;
    virtual void    Shutdown           ()                                     = 0;
    virtual HRESULT EnumerateDevices   (std::vector<ControllerDeviceInfo> & outDevices) = 0;
    virtual HRESULT ReadSample         (const ControllerUnitKey & unit,
                                        ControllerSample        & outSample) = 0;
    virtual void    GetWakeSources     (std::vector<HANDLE> & outEvents,
                                        bool                & outNeedsTimedPoll) = 0;
};

class IControllerBackendEvents
{
public:

    virtual ~IControllerBackendEvents () = default;

    virtual void OnDevicesChanged () = 0;
};
```

## Behavior

| Call | Contract |
|---|---|
| `Initialize` | Called on the controller thread. Creates the message-only window, registers for HID notifications, creates DirectInput, resolves `XInputGetCapabilitiesEx` (ordinal 108) if present. Failure of DirectInput or XInput individually is not fatal: the backend runs with whichever API succeeded and reports the missing one through `EnumerateDevices` returning no devices of that kind plus a logged HRESULT. Fails only if neither is available. |
| `EnumerateDevices` | Returns every attached controller exactly once: XInput slots that report connected, then DirectInput game controllers (`DI8DEVCLASS_GAMECTRL`, attached only) whose `DIPROP_GUIDANDPATH` does not contain `IG_`. Order is stable for an unchanged set of devices. |
| `ReadSample` | Fills a normalized sample (R7). A device that is gone returns `HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED)` and `outSample.connected = false`. `DIERR_INPUTLOST` / `DIERR_NOTACQUIRED` trigger one `Acquire` + `Poll` retry before failing. Never returns a centered sample with `S_OK` for a device it could not read (FR-015). |
| Wake sources | The backend exposes the DirectInput event handles it registered with `SetEventNotification` and reports which devices need polling (`DIDC_POLLEDDEVICE`) or are XInput, so the thread waits on events and uses a timeout only for those (R13). A read of an XInput slot whose `dwPacketNumber` is unchanged returns the previous sample without decoding. |
| `OnDevicesChanged` | Raised on the controller thread after a HID arrival or removal, at the +300 ms and +2 s rescans (R4). There is no periodic rescan; `outNeedsTimedPoll` is false whenever no XInput controller, no polled DirectInput device, and no receiver needing the R4 fallback is attached. |
| Activation | The backend does not know about focus. `ControllerInputService` gates samples on Casso's activation state (R2), set from the shell's `WM_ACTIVATEAPP` handling. |

## Threading

All methods are called only from `ControllerInputThread`. The backend holds no locks and exposes nothing to other threads.

## Implementations

| Class | Location | Purpose |
|---|---|---|
| `Win32ControllerBackend` | `CassoEmuCore/Seams/Win32ControllerBackend.h/.cpp` | Real devices. Decoding (POV to D-pad, range normalization, XInput button bits to `ControlId`) is delegated to the pure `DirectInputSampleDecoder` and `XInputSampleDecoder` so it is tested without devices. Links `xinput.lib`, `dinput8.lib`, `dxguid.lib`, `hid.lib` by `#pragma comment (lib, ...)` in this `.cpp`, per the `WasapiAudio.cpp` convention. |
| `FakeControllerBackend` | `UnitTest/ControllerTests/FakeControllerBackend.h` | Scripted devices: add, remove, set sample, fail next read, raise `OnDevicesChanged`. |

## Unit-test obligations

- Every decoder rule is covered by synthetic `DIJOYSTATE2` / `XINPUT_STATE` values, including POV centered (`LOWORD == 0xFFFF`), each diagonal, and axis range extremes.
- The service's handling of each `ReadSample` failure is covered through `FakeControllerBackend`, and each such test must observe the disconnected state rather than a rest sample reported as healthy.
