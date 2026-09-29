#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/GamePortInputMixer.h"
#include "Controllers/MappingEvaluator.h"
#include "Controllers/PlayerSlotPolicy.h"
#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputService
//
//  What the controller thread does on every wake: read the attached
//  controllers, turn the ones that play into a game-port contribution, and
//  submit what they ask for together to the mixer. It also answers how long
//  the thread should wait for the next wake, which is what keeps an idle
//  Casso idle.
//
//  WHO PLAYS IS THE TWO PLAYER SLOTS. Each player has an entry -- Automatic, a
//  picked controller, the keys or the mouse for Player 1, or Disabled for
//  Player 2 -- and PlayerSlotPolicy turns the entries, what is attached, and
//  the order controllers arrived in and were first used into the slots. The
//  service keeps those two orders: the controllers that connect after the
//  startup scan, and each controller's first real input while Casso is
//  active. A controller in no slot is read only for its first input and
//  never reaches the game port.
//
//  One player playing drives PDL0, PDL1 and PB0-PB2 as a single controller
//  always has; two each drive their own slot's paddles and lines. Each is read
//  and evaluated on its own, so one controller leaving releases only what it
//  held.
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
    static constexpr DWORD  kPollPeriodMs      = 8;

    // How often a controller nobody plays is read for its first input, when
    // it has no change events of its own. Slow enough to cost nothing
    // measurable while idle, quick enough that a person joining a game does
    // not notice.
    static constexpr DWORD  kIdleWatchPeriodMs = 100;

    // How many readings of the inspected controller are kept for the page
    // between two of its polls: a second's worth at kPollPeriodMs, far more
    // than a poll at display rate leaves waiting.
    static constexpr size_t kInspectedHistoryMax = 128;

    struct Snapshot
    {
        std::vector<ControllerDeviceInfo>      devices;
        PlayerEntries                          entries;
        PlayerSlots                            slots;

        // Each attached controller's kind of profile, from its player's mode
        // or the Joyport, and its active profile of that kind, by unit token;
        // a missing or empty entry means that kind's built-in profile.
        std::map<std::string, ProfileMode>     profileModes;
        std::map<std::string, std::string>     activeProfiles;
        bool                                   isJoyportAttached    = false;
        bool                                   hasJoyport           = false;
        size_t                                 axisCount            = GamePortContribution::kAxisCount;

        // Whether any controller that drives the game port reads.
        bool                                   isAnyDriverConnected = false;

        // Attached controllers whose last read failed while they were
        // watched for their first input. They are not read again until they
        // reconnect.
        std::vector<ControllerUnitKey>         unreadable;
    };

    // What changed in the slots, raised on whichever thread changed them. The
    // entries come along because a picked controller that came back under
    // another identity is followed there, which the prefs should keep.
    struct SlotsChange
    {
        PlayerEntries              entries;
        PlayerSlots                slots;
        bool                       haveEntriesChanged = false;

        // Each controller that held a slot and has disconnected.
        std::vector<std::wstring>  departedDescriptions;

        // A notice for each slot Automatic gave a controller other than the
        // one that last held it, and the last holders after this change,
        // which the shell saves when they moved.
        std::vector<std::wstring>  notices;
        PlayerLastHolders          lastHolders;
        bool                       haveLastHoldersChanged = false;
    };

    using SlotsChangedFn = std::function<void (const SlotsChange &)>;

    // Raised when a controller that drives the game port connects or
    // disconnects, or the attached list changes. The axis owner and the
    // picker's rows depend on it, and both are decided on the UI thread.
    using StateChangedFn = std::function<void ()>;

    // Brings the controller thread out of its wait, for a change that gives
    // it something new to read before its current wait would end.
    using WakeFn = std::function<void ()>;

    ControllerInputService (IControllerBackend & backend, GamePortInputMixer & mixer);

    void  OnDevicesChanged () override;

    void  SetActive          (bool isActive);
    void  SetDeadzone        (float deadzone);
    void  SetHasGamePort     (bool hasGamePort);
    void  SetSlotsChangedFn  (SlotsChangedFn onSlotsChanged);
    void  SetStateChangedFn  (StateChangedFn onStateChanged);
    void  SetWakeFn          (WakeFn wake);

    // How many paddle axes the machine has. A player slot mapped to a paddle
    // past it is kept and plays nothing, so a machine with more axes restores
    // it.
    void  SetAxisCount       (size_t axisCount);

    // Both players' entries, replacing any before. A second pick of the
    // first player's controller, and an entry the player cannot have, read
    // as Automatic.
    void           SetPlayerEntries (const PlayerEntries & entries);
    PlayerEntries  GetPlayerEntries () const;

    // One player's entry. Picking the controller the other player picked
    // returns the other player to Automatic.
    void           PickPlayerEntry  (size_t player, const PlayerEntry & entry);

    // One player's mode, which decides what the player drives and which
    // kind of profile its controller plays.
    void           SetPlayerMode    (size_t player, PlayerMode mode);

    PlayerSlots    GetPlayerSlots   () const;

    // The controller that last held each slot, set from the saved prefs
    // before the first tick and read back to save them. It decides only
    // whether an assignment is announced.
    void               SetLastHolders (const PlayerLastHolders & lastHolders);
    PlayerLastHolders  GetLastHolders () const;

    // Rescans what is attached on the next tick, for a machine switched to.
    void  RequestRescan      ();

    // The controller the Settings sheet's Controllers page shows, read on
    // every tick while it is set whether or not it plays, so the page can
    // assign, calibrate and show live readings for any attached controller.
    // Cleared when the page closes, which ends the extra reads. A request for
    // a new unit, or for one not yet read, wakes the thread: with an
    // event-driven controller playing it may otherwise sleep until that
    // controller moves.
    void                             SetInspectedUnit   (const std::optional<ControllerUnitKey> & unit);
    std::optional<ControllerSample>  GetInspectedSample (const ControllerUnitKey & unit) const;

    // Every reading of the inspected controller since the last call, oldest
    // first, up to kInspectedHistoryMax of the latest; none for another unit.
    // A press and release between two of the page's polls is in here even
    // though neither poll's latest reading shows it.
    std::vector<ControllerSample>    TakeInspectedSamples (const ControllerUnitKey & unit);

    // Rate bindings' paddles back to center, for a machine switch or a profile
    // change (FR-021a). Takes effect on the controller thread's next reading.
    void  ResetPaddleRate ();

    // Seconds on a monotonic clock. Tests supply their own, so a rate
    // binding's movement can be checked without waiting.
    using ClockFn = std::function<double ()>;

    void  SetClock (ClockFn clock);

    // Each controller model's saved dead zone and profiles, by model token. A
    // controller plays with its model's Default profile from the next time it
    // plays or connects.
    void                                            SetModelSettings (std::map<std::string, ControllerModelSettings> models);
    std::map<std::string, ControllerModelSettings>  GetModelSettings () const;

    // Each controller's active profile of the kind it plays, by name; empty
    // means that kind's built-in profile. The kind comes from the mode of the
    // player whose slot holds the controller, Joystick for one in no slot,
    // or Joyport while the Joyport is attached. The choice belongs to the
    // controller, not the machine or the player, so a controller plays its
    // profile on any machine. A name its model does not have plays the
    // built-in profile, and so does a profile of another kind, which cannot
    // be chosen. A change takes effect on the next reading, releasing
    // whatever the old profile held. The whole maps, one per kind, are set
    // from and saved to the prefs; what is read back to save leaves out
    // choices of another kind.
    void                                SetActiveProfile  (const ControllerUnitKey & unit, const std::string & name);
    std::string                         GetActiveProfile  (const ControllerUnitKey & unit) const;
    void                                SetActiveProfiles (ProfileMode mode, std::map<std::string, std::string> activeProfiles);
    std::map<std::string, std::string>  GetActiveProfiles (ProfileMode mode) const;

    // Whether the running machine has a Joyport. It is on while a player's
    // mode puts it in one of the jacks, and that player plays its Joyport
    // profile.
    void                                SetJoyportAvailable (bool hasJoyport);

    // Every DirectInput unit's calibration, by unit token. Set once from the
    // saved prefs; read back to save them, including what automatic
    // calibration has learned since.
    void                                          SetCalibrations (std::map<std::string, ControllerCalibration> calibrations);
    std::map<std::string, ControllerCalibration>  GetCalibrations () const;

    // Reads the attached controllers once and returns what the thread should
    // wait on before reading again.
    ControllerWaitSources  Tick ();

    // What the last tick decided, for the trace. Every step between a
    // controller moving and the game port changing, so a failure says which
    // step it failed at rather than only that nothing happened. The read
    // fields describe Player 1's controller; the submission is every driving
    // controller merged.
    struct TickReport
    {
        bool                  hasPlayerOne      = false;
        bool                  isActiveXInput    = false;
        HRESULT               readResult        = S_OK;
        bool                  isConnected       = false;   // the read succeeded and reported connected
        bool                  hasMapping        = false;   // Player 1's controller has a non-empty mapping
        bool                  isAppActive       = false;
        bool                  didSubmit         = false;
        float                 deadzone          = 0.0f;
        GamePortContribution  submitted;
    };

    TickReport  GetLastTickReport () const;

    Snapshot  GetSnapshot () const;

private:

    // One attached controller the tick reads, guarded by m_mutex: the ones
    // that play, and the ones read only for their first input. Its evaluator
    // is not here: that belongs to the controller thread alone.
    struct DriverState
    {
        ControllerUnitKey                    unit;
        ControlMapping                       mapping;
        float                                deadzone    = 0.0f;
        bool                                 isResolved  = false;
        bool                                 isConnected = false;
        bool                                 isDriving   = false;
        bool                                 hasFailed   = false;

        // What its last reading asked for, on its own PDL0-PDL3 before its
        // slot places them. Absent while it contributes nothing.
        std::optional<GamePortContribution>  logical;

        // The kind of profile the mapping was resolved for. A change of the
        // player's mode, or of the Joyport, resolves it again.
        ProfileMode                          mode        = ProfileMode::Joystick;
    };

    // One controller's work for one tick, copied out of the lock, and what
    // its read found.
    struct DriverRead;

    void                   RefreshDevices       ();
    ControllerWaitSources  TickDrivers          ();
    bool                   PrepareWatch         (std::vector<DriverRead> & reads, bool isWatchDue, ControllerWaitSources & wait);
    void                   ReadDriver           (DriverRead & read, bool isActive);
    void                   EvaluateDriver       (DriverRead & read, float elapsedSeconds, bool isActive, ControllerWaitSources & wait, bool & outNeedsPoll);
    ControllerSample       RecordReading        (const DriverRead & read, HRESULT hr, const ControllerSample & sample, bool isConnected, bool & outHasFlipped);
    void                   ForgetIdleEvaluators ();
    void                   Publish              (const GamePortContribution & merged);
    void                   ReleaseContribution  ();
    void                   Wake                 ();
    void                   AnnounceSlots        (const std::optional<SlotsChange> & change);

    // All of these assume m_mutex is already held.
    const ControllerDeviceInfo *    FindDeviceLocked         (const ControllerUnitKey & unit) const;
    std::string                     GetActiveProfileLocked   (const ControllerUnitKey & unit) const;
    ProfileMode                     GetUnitModeLocked        (const ControllerUnitKey & unit) const;
    std::optional<size_t>           FindPlayerLocked         (const ControllerUnitKey & unit) const;
    void                            SyncDriversLocked        ();
    void                            ResolveMappingLocked     (DriverState & driver);
    void                            ResolveUnitLocked        (const ControllerDeviceInfo & device, ControlMapping & outMapping, float & outDeadzone) const;
    void                            UnresolveDriversLocked   ();
    GamePortContribution            BuildMergedLocked        () const;
    float                           MeasureElapsedLocked     ();
    bool                            IsOfOtherModeLocked      (const ControllerModelKey & model, const std::string & name, ProfileMode mode) const;
    std::optional<SlotsChange>      EvaluateSlotsLocked      (std::vector<std::wstring> departed = {});
    void                            RecordArrivalsLocked     (const std::vector<ControllerDeviceInfo> & devices);
    std::vector<std::wstring>       FindDepartedLocked       (const std::vector<ControllerDeviceInfo> & devices) const;

    // What a controller reaches on the game port, or nothing when it does not
    // play. Assumes m_mutex is held.
    std::optional<PlayerTargetRules::Route>  GetDriverRouteLocked (const ControllerUnitKey & unit) const;


    // Places each player's switches on the Joyport jacks the players' states
    // give them. Assumes m_mutex is held.
    void  AddJoyportSwitchesLocked (GamePortContribution & merged) const;

    IControllerBackend                 & m_backend;
    GamePortInputMixer                 & m_mixer;
    std::vector<ControllerDeviceInfo>    m_devices;

    // The players: what the user chose, what is played, and the two orders
    // Automatic fills them in. m_hasEnumerated separates the controllers
    // present at startup, which are no arrival, from the ones that connect
    // later.
    PlayerEntries                                        m_entries       = PlayerSlotPolicy::MakeDefaultEntries();
    PlayerSlots                                          m_slots;
    PlayerOrderLogs                                      m_logs;
    PlayerLastHolders                                    m_lastHolders;
    bool                                                 m_hasEnumerated = false;

    // Every controller the tick reads, by unit token.
    std::map<std::string, DriverState>                   m_drivers;
    size_t                                               m_axisCount = GamePortContribution::kAxisCount;

    // Controller thread only: each driver's rate paddles, by unit token, and
    // the drivers new since the last tick, whose paddles start at center.
    std::map<std::string, MappingEvaluator>              m_evaluators;
    std::vector<std::string>                             m_rateResetTokens;

    std::map<std::string, ControllerCalibration>         m_calibrations;
    ControllerProfileStore                               m_profiles;
    bool                                                 m_hasJoyport        = false;

    ClockFn                                              m_clock;
    double                                               m_lastTickSeconds  = -1.0;
    double                                               m_lastWatchSeconds = -1.0;
    std::atomic<bool>                                    m_rateResetPending {false};

    std::optional<ControllerUnitKey>                     m_inspectedUnit;
    ControllerSample                                     m_inspectedSample;
    bool                                                 m_hasInspectedSample = false;
    std::deque<ControllerSample>                         m_inspectedHistory;

    TickReport                           m_lastTick;
    SlotsChangedFn                       m_onSlotsChanged;
    StateChangedFn                       m_onStateChanged;
    WakeFn                               m_wake;
    float                                m_deadzone            = 0.0f;
    bool                                 m_isActive            = true;
    bool                                 m_hasGamePort         = true;
    std::atomic<bool>                    m_hasContribution     {false};
    std::atomic<bool>                    m_devicesDirty        {true};
    mutable std::mutex                   m_mutex;
};
