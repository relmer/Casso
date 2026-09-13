#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
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

        // The controller reading the axes while the chosen one is absent
        // (FR-008a). Absent whenever the chosen controller is doing it
        // itself, which is the ordinary case.
        std::optional<ControllerUnitKey>   standIn;

        // What to call each of them. The chosen controller's description is
        // remembered from the last enumeration that carried it, so a picker
        // row for a controller that has gone can still name it.
        std::wstring                       selectionDescription;
        std::wstring                       standInDescription;

        ControllerSample                   lastSample;
        bool                               isSelectedConnected = false;
    };

    // Raised on the controller thread when the policy chooses or adopts a
    // controller on its own, so the shell can persist the choice and say so.
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

    // Reads the selected controller once and returns what the thread should
    // wait on before reading again.
    ControllerWaitSources  Tick ();

    // What the last tick decided, for the trace. Every step between a
    // controller moving and the game port changing, so a failure says which
    // step it failed at rather than only that nothing happened.
    struct TickReport
    {
        bool                  hasSelection      = false;
        bool                  hasStandIn        = false;
        bool                  hasActiveUnit     = false;
        bool                  isSelectionActive = false;   // the active unit IS the selection
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
    void  ReleaseContribution     ();
    void  EnsureMappingForActive  ();
    bool  IsSelectedDevicePresent () const;

    // All of these assume m_mutex is already held.
    const ControllerDeviceInfo *  FindDeviceLocked          (const ControllerUnitKey & unit) const;
    const ControllerDeviceInfo *  FindActiveDeviceLocked    () const;
    void                          EnsureMappingForActiveLocked ();
    void                          UpdateAttachOrderLocked   ();

    // Recomputes which controller is actually read, and returns whether that
    // changed. The chosen one while it is there, its stand-in while it is
    // not, and the chosen one either way when nothing can stand in, so that
    // the read fails and the disconnect path runs.
    bool                          UpdateActiveUnitLocked    ();

    IControllerBackend                 & m_backend;
    GamePortInputMixer                 & m_mixer;
    MappingEvaluator                     m_evaluator;
    ControlMapping                       m_mapping;
    std::vector<ControllerDeviceInfo>    m_devices;
    std::optional<ControllerUnitKey>     m_selection;
    std::optional<ControllerUnitKey>     m_standIn;
    std::optional<ControllerUnitKey>     m_activeUnit;
    std::wstring                         m_selectionDescription;

    // When each attached controller was first seen, so the stand-in is the
    // one that has been there longest and does not move as unrelated
    // controllers come and go (FR-008a).
    std::vector<std::pair<ControllerUnitKey, uint64_t>>  m_attachOrder;
    uint64_t                                             m_nextAttachOrder = 0;

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
