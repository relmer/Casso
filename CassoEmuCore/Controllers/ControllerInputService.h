#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/GamePortInputMixer.h"
#include "Controllers/MappingEvaluator.h"
#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputService
//
//  What the controller thread does on every wake: read the selected
//  controller, turn its state into a game-port contribution, and submit that
//  to the mixer. It also answers how long the thread should wait for the next
//  wake, which is what keeps an idle Casso idle.
//
//  Nothing here touches a device or a machine directly: the backend reads
//  controllers and the mixer writes the machine, so every rule in this class
//  is exercised in tests with a scripted backend and a recording sink.
//
//  A read that fails is a disconnect, never a controller resting at center:
//  reporting rest for a controller that is gone would leave a game believing
//  the stick is centered and the buttons up, which is indistinguishable from
//  a healthy controller doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerInputService : public IControllerBackendEvents
{
public:

    // The measured XInput report interval on this machine (research R13).
    static constexpr DWORD  kPollPeriodMs = 8;

    struct Snapshot
    {
        std::vector<ControllerDeviceInfo>  devices;
        std::optional<ControllerUnitKey>   selection;

        // What the machine keeps. The same as `selection` except after a clear,
        // which is never saved: the machine keeps the controller it last had
        // (FR-011).
        std::optional<ControllerUnitKey>   saved;
        ControllerSample                   lastSample;
        bool                               isSelectedConnected = false;
    };

    // Raised on the controller thread when the policy moves the selection on
    // its own, so the shell can persist the choice and say so.
    using SelectionChangedFn = std::function<void (const ControllerSelectionPolicy::Decision &)>;

    // Raised when the selected controller connects or disconnects. The axis
    // owner depends on it, and the owner is decided on the UI thread.
    using StateChangedFn = std::function<void ()>;

    ControllerInputService (IControllerBackend & backend, GamePortInputMixer & mixer);

    void  OnDevicesChanged () override;

    void  SetActive             (bool isActive);
    void  SetSelection          (const std::optional<ControllerUnitKey> & selection);
    void  SetDeadzone           (float deadzone);
    void  SetHasGamePort        (bool hasGamePort);
    void  SetSelectionChangedFn (SelectionChangedFn onSelectionChanged);
    void  SetStateChangedFn     (StateChangedFn onStateChanged);

    // Runs the selection policy on the next tick, for a machine switched to:
    // one with no controller saved counts as a controller connecting (FR-032).
    void  RequestRescan         ();

    // The controller the Settings sheet's Controllers page shows, read on
    // every tick while it is set whether or not it is the selected one, so
    // the page can assign, calibrate and show live readings for any attached
    // controller. Cleared when the page closes, which ends the extra reads.
    void                             SetInspectedUnit   (const std::optional<ControllerUnitKey> & unit);
    std::optional<ControllerSample>  GetInspectedSample (const ControllerUnitKey & unit) const;

    // Rate bindings' paddles back to center, for a machine switch or a profile
    // change (FR-021a). Takes effect on the controller thread's next reading.
    void  ResetPaddleRate ();

    // Seconds on a monotonic clock. Tests supply their own, so a rate
    // binding's movement can be checked without waiting.
    using ClockFn = std::function<double ()>;

    void  SetClock (ClockFn clock);

    // Each controller model's saved deadzone and profiles, by model token. A
    // controller plays with its model's Default profile from the next time it
    // is selected or connects.
    void                                            SetModelSettings (std::map<std::string, ControllerModelSettings> models);
    std::map<std::string, ControllerModelSettings>  GetModelSettings () const;

    // Every DirectInput unit's calibration, by unit token. Set once from the
    // saved prefs; read back to save them, including what automatic
    // calibration has learned since.
    void                                          SetCalibrations (std::map<std::string, ControllerCalibration> calibrations);
    std::map<std::string, ControllerCalibration>  GetCalibrations () const;

    // Reads the selected controller once and returns what the thread should
    // wait on before reading again.
    ControllerWaitSources  Tick ();

    // What the last tick decided, for the trace. Every step between a
    // controller moving and the game port changing, so a failure says which
    // step it failed at rather than only that nothing happened.
    struct TickReport
    {
        bool                  hasSelection      = false;
        bool                  isActiveXInput    = false;
        HRESULT               readResult        = S_OK;
        bool                  isConnected       = false;   // the read succeeded and reported connected
        bool                  hasMapping        = false;   // the active unit has a non-empty mapping
        bool                  isAppActive       = false;
        bool                  didSubmit         = false;
        float                 deadzone          = 0.0f;
        GamePortContribution  submitted;
    };

    TickReport  GetLastTickReport () const;

    Snapshot  GetSnapshot () const;

private:

    void  RefreshDevices          ();
    ControllerWaitSources  TickSelected ();
    void  ReleaseContribution     ();
    void  EnsureMappingForActive  ();
    bool  IsSelectedDevicePresent () const;

    // All of these assume m_mutex is already held.
    const ControllerDeviceInfo *  FindDeviceLocked          (const ControllerUnitKey & unit) const;
    const ControllerDeviceInfo *  FindActiveDeviceLocked    () const;
    void                          EnsureMappingForActiveLocked ();
    void                          UpdateAttachOrderLocked   ();
    uint64_t                      GetAttachOrderLocked      (const ControllerUnitKey & unit) const;

    IControllerBackend                 & m_backend;
    GamePortInputMixer                 & m_mixer;
    MappingEvaluator                     m_evaluator;
    ControlMapping                       m_mapping;
    std::vector<ControllerDeviceInfo>    m_devices;
    std::optional<ControllerUnitKey>     m_selection;
    std::optional<ControllerUnitKey>     m_saved;

    // When each attached controller was first seen, so the one that takes
    // over from a controller that leaves is the one that has been there
    // longest, not whichever enumeration happens to list first (FR-008a).
    std::vector<std::pair<ControllerUnitKey, uint64_t>>  m_attachOrder;
    uint64_t                                             m_nextAttachOrder = 0;

    std::map<std::string, ControllerCalibration>         m_calibrations;
    ControllerProfileStore                               m_profiles;

    ClockFn                                              m_clock;
    double                                               m_lastTickSeconds  = -1.0;
    std::atomic<bool>                                    m_rateResetPending {false};

    std::optional<ControllerUnitKey>                     m_inspectedUnit;
    ControllerSample                                     m_inspectedSample;
    bool                                                 m_hasInspectedSample = false;

    ControllerSample                     m_lastSample;
    TickReport                           m_lastTick;
    SelectionChangedFn                   m_onSelectionChanged;
    StateChangedFn                       m_onStateChanged;
    float                                m_deadzone            = 0.0f;
    bool                                 m_isActive            = true;
    bool                                 m_hasGamePort         = true;
    bool                                 m_isSelectedConnected = false;
    bool                                 m_hasContribution     = false;
    std::atomic<bool>                    m_devicesDirty        {true};
    mutable std::mutex                   m_mutex;
};
