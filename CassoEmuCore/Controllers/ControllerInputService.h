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
//  What the controller thread does on every wake: read each controller that
//  drives the game port, turn its state into a game-port contribution, and
//  submit what they ask for together to the mixer. It also answers how long
//  the thread should wait for the next wake, which is what keeps an idle
//  Casso idle.
//
//  WHICH CONTROLLERS DRIVE THE GAME PORT FOLLOWS THE MACHINE'S MODE. In
//  single-source mode it is the selection alone, driving PDL0/PDL1 and
//  PB0-PB2, which is what a machine has always done. In multiplayer mode it is
//  the two player slots, each playing the paddles its slot maps to and one
//  button line of its own (see MultiplayerSetup); the selection drives
//  nothing. Each driver is read and evaluated on its own, so one controller
//  leaving releases only what it held.
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
        std::vector<ControllerDeviceInfo>      devices;
        std::optional<ControllerUnitKey>       selection;

        // What the machine keeps. The same as `selection` except after a clear,
        // which is never saved: the machine keeps the controller it last had
        // (FR-011).
        std::optional<ControllerUnitKey>       saved;

        // The machine's active profile by name; empty means Default.
        std::string                            activeProfile;
        ControllerSample                       lastSample;
        bool                                   isSelectedConnected  = false;

        // The machine's two-player setup, and how many axes it has.
        MultiplayerSetup                       multiplayer;
        size_t                                 axisCount            = GamePortContribution::kAxisCount;

        // Whether any controller that drives the game port reads: the
        // selection, or either player's controller.
        bool                                   isAnyDriverConnected = false;
    };

    // Raised on the controller thread when the policy moves the selection on
    // its own, so the shell can persist the choice and say so.
    using SelectionChangedFn = std::function<void (const ControllerSelectionPolicy::Decision &)>;

    // Raised when a controller that drives the game port connects or
    // disconnects. The axis owner depends on it, and the owner is decided on
    // the UI thread.
    using StateChangedFn = std::function<void ()>;

    // Brings the controller thread out of its wait, for a change that gives
    // it something new to read before its current wait would end.
    using WakeFn = std::function<void ()>;

    ControllerInputService (IControllerBackend & backend, GamePortInputMixer & mixer);

    void  OnDevicesChanged () override;

    void  SetActive             (bool isActive);
    void  SetSelection          (const std::optional<ControllerUnitKey> & selection);
    void  SetDeadzone           (float deadzone);
    void  SetHasGamePort        (bool hasGamePort);
    void  SetSelectionChangedFn (SelectionChangedFn onSelectionChanged);
    void  SetStateChangedFn     (StateChangedFn onStateChanged);
    void  SetWakeFn             (WakeFn wake);

    // How many paddle axes the machine has. A player slot mapped to a paddle
    // past it is kept and plays nothing, so a machine with more axes restores
    // it (FR-034, FR-035).
    void  SetAxisCount          (size_t axisCount);

    // The machine's two-player setup, replacing any before. It is normalized
    // first, so an overlapping or repeated slot is refused rather than played
    // (FR-036).
    void  SetMultiplayer        (MultiplayerSetup setup);

    // Turns the mode on or off, keeping both slots. Off is single-source mode,
    // where the machine behaves exactly as it did before the mode existed;
    // picking a single source from the toolbar picker turns it off.
    void  SetMultiplayerEnabled (bool isEnabled);

    // One player's controller and what it maps to. A slot that cannot be
    // played beside the other one is emptied by the same normalization.
    void  SetMultiplayerSlot    (size_t                                    player,
                                 const std::optional<ControllerUnitKey> &  unit,
                                 PlayerAxisTarget                          target);

    MultiplayerSetup  GetMultiplayer () const;

    // Runs the selection policy on the next tick, for a machine switched to:
    // one with no controller saved counts as a controller connecting (FR-032).
    void  RequestRescan         ();

    // The controller the Settings sheet's Controllers page shows, read on
    // every tick while it is set whether or not it is the selected one, so
    // the page can assign, calibrate and show live readings for any attached
    // controller. Cleared when the page closes, which ends the extra reads.
    // A request for a new unit, or for one not yet read, wakes the thread:
    // with an event-driven controller selected it may otherwise sleep until
    // that controller moves.
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

    // The profile the machine plays with, by name; empty means Default. Every
    // controller that drives the game port plays its own model's profile of
    // that name. A name a model does not have plays its Default, and nothing
    // is created or saved for it. A change takes effect on the next reading,
    // releasing whatever the old profile held.
    void         SetActiveProfile (const std::string & name);
    std::string  GetActiveProfile () const;

    // Every DirectInput unit's calibration, by unit token. Set once from the
    // saved prefs; read back to save them, including what automatic
    // calibration has learned since.
    void                                          SetCalibrations (std::map<std::string, ControllerCalibration> calibrations);
    std::map<std::string, ControllerCalibration>  GetCalibrations () const;

    // Reads the controllers that drive the game port once and returns what
    // the thread should wait on before reading again.
    ControllerWaitSources  Tick ();

    // What the last tick decided, for the trace. Every step between a
    // controller moving and the game port changing, so a failure says which
    // step it failed at rather than only that nothing happened. The read
    // fields describe the selected controller; the submission is every
    // driving controller merged.
    struct TickReport
    {
        bool                  hasSelection      = false;
        bool                  isActiveXInput    = false;
        HRESULT               readResult        = S_OK;
        bool                  isConnected       = false;   // the read succeeded and reported connected
        bool                  hasMapping        = false;   // the selected unit has a non-empty mapping
        bool                  isAppActive       = false;
        bool                  didSubmit         = false;
        float                 deadzone          = 0.0f;
        GamePortContribution  submitted;
    };

    TickReport  GetLastTickReport () const;

    Snapshot  GetSnapshot () const;

private:

    // One controller that drives the game port, guarded by m_mutex. Its
    // evaluator is not here: that belongs to the controller thread alone.
    struct DriverState
    {
        ControllerUnitKey                    unit;
        ControlMapping                       mapping;
        float                                deadzone    = 0.0f;
        bool                                 isResolved  = false;
        bool                                 isConnected = false;

        // What its last reading asked for, on its own PDL0-PDL3 before the
        // assignment places them. Absent while it contributes nothing.
        std::optional<GamePortContribution>  logical;
    };

    // One driver's work for one tick, copied out of the lock.
    struct DriverRead;

    void                   RefreshDevices       ();
    ControllerWaitSources  TickDrivers          ();
    bool                   TickDriver           (const DriverRead & read, float elapsedSeconds, bool isActive, ControllerWaitSources & wait, bool & outNeedsPoll);
    ControllerSample       RecordReading        (const DriverRead & read, HRESULT hr, const ControllerSample & sample, bool isConnected, bool & outHasFlipped);
    void                   ForgetIdleEvaluators (const std::vector<DriverRead> & reads);
    void                   Publish              (const GamePortContribution & merged);
    void                   ReleaseContribution  ();
    void                   Wake                 ();

    // All of these assume m_mutex is already held.
    const ControllerDeviceInfo *    FindDeviceLocked         (const ControllerUnitKey & unit) const;
    std::vector<ControllerUnitKey>  GetDriverUnitsLocked     () const;
    MultiplayerSetup::AxisSet       GetDriverAxesLocked      (const ControllerUnitKey & unit) const;
    bool                            IsDriverLocked           (const ControllerUnitKey & unit) const;
    void                            SyncDriversLocked        ();
    void                            ResolveMappingLocked     (DriverState & driver);
    void                            UnresolveDriversLocked   ();
    GamePortContribution            BuildMergedLocked        () const;
    float                           MeasureElapsedLocked     ();
    void                            UpdateAttachOrderLocked  ();
    uint64_t                        GetAttachOrderLocked     (const ControllerUnitKey & unit) const;

    IControllerBackend                 & m_backend;
    GamePortInputMixer                 & m_mixer;
    std::vector<ControllerDeviceInfo>    m_devices;
    std::optional<ControllerUnitKey>     m_selection;
    std::optional<ControllerUnitKey>     m_saved;

    // Every controller that drives the game port, by unit token.
    std::map<std::string, DriverState>                   m_drivers;
    MultiplayerSetup                                     m_multiplayer;
    size_t                                               m_axisCount = GamePortContribution::kAxisCount;

    // Controller thread only: each driver's rate paddles, by unit token, and
    // the drivers new since the last tick, whose paddles start at center.
    std::map<std::string, MappingEvaluator>              m_evaluators;
    std::vector<std::string>                             m_rateResetTokens;

    // When each attached controller was first seen, so the one that takes
    // over from a controller that leaves is the one that has been there
    // longest, not whichever enumeration happens to list first (FR-008a).
    std::vector<std::pair<ControllerUnitKey, uint64_t>>  m_attachOrder;
    uint64_t                                             m_nextAttachOrder = 0;

    std::map<std::string, ControllerCalibration>         m_calibrations;
    ControllerProfileStore                               m_profiles;
    std::string                                          m_activeProfile;

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
    WakeFn                               m_wake;
    float                                m_deadzone            = 0.0f;
    bool                                 m_isActive            = true;
    bool                                 m_hasGamePort         = true;
    bool                                 m_isSelectedConnected = false;
    std::atomic<bool>                    m_hasContribution     {false};
    std::atomic<bool>                    m_devicesDirty        {true};
    mutable std::mutex                   m_mutex;
};
