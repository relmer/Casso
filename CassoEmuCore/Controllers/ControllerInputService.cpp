#include "Pch.h"

#include "Controllers/ControllerInputService.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriverRead
//
//  What one tick needs to read and evaluate one driving controller, copied
//  out from under the lock so the backend and the evaluator run outside it.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerInputService::DriverRead
{
    ControllerUnitKey  unit;
    std::string        token;
    ControlMapping     mapping;
    float              deadzone         = 0.0f;
    size_t             logicalAxisCount = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputService
//
////////////////////////////////////////////////////////////////////////////////

ControllerInputService::ControllerInputService (IControllerBackend & backend, GamePortInputMixer & mixer) :
    m_backend (backend),
    m_mixer   (mixer)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDevicesChanged
//
//  Raised by the backend when a controller arrives or leaves. The list is not
//  rebuilt here: this runs inside the backend's notification, and the next
//  tick is where reading devices belongs.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::OnDevicesChanged()
{
    m_devicesDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActive
//
//  Casso became the active application, or stopped being it. An inactive
//  Casso releases every controller's contribution, so a button held as the
//  user switches away does not stay down in the guest.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActive (bool isActive)
{
    std::unique_lock<std::mutex>  lock      (m_mutex);
    bool                          wasActive = m_isActive;



    m_isActive = isActive;

    if (!wasActive || isActive)
    {
        return;
    }

    for (auto & [token, driver] : m_drivers)
    {
        driver.logical.reset();
    }

    lock.unlock();

    ReleaseContribution();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelection
//
//  Which controller drives the game port, or none. The controller it replaces
//  stops driving unless a player slot holds it, and whatever it held is
//  released; every other driving controller continues untouched. While
//  multiplayer is on the selection drives nothing, and is only what the
//  machine returns to when the mode goes off.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetSelection (const std::optional<ControllerUnitKey> & selection)
{
    std::unique_lock<std::mutex>  lock       (m_mutex);
    bool                          hasChanged = m_selection != selection;
    GamePortContribution          merged;



    // A pick, or a machine's saved controller handed in, is what the machine
    // keeps -- even when it names the controller already in use after a
    // takeover, which is how a user makes that controller the saved one.
    m_saved = selection;

    if (!hasChanged)
    {
        return;
    }

    m_selection           = selection;
    m_lastSample          = ControllerSample();
    m_isSelectedConnected = false;

    // A driver that is new starts from its own model's mapping and with its
    // rate paddles at center; one that was already driving keeps both.
    SyncDriversLocked();

    // A saved controller restored for a machine may not be attached. The
    // policy is what replaces it, so the next tick has to run it; a pick
    // from the picker is always attached and needs no scan.
    if (selection.has_value() && FindDeviceLocked (selection.value()) == nullptr)
    {
        m_devicesDirty = true;
    }

    merged = BuildMergedLocked();
    lock.unlock();

    Publish (merged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDeadzone
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetDeadzone (float deadzone)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_deadzone = deadzone;

    for (auto & [token, driver] : m_drivers)
    {
        driver.deadzone = deadzone;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHasGamePort
//
//  Whether the machine in front of the user has a game port at all. Without
//  one the policy chooses nothing, and a selection carried in from another
//  machine is kept untouched (FR-017).
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetHasGamePort (bool hasGamePort)
{
    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        if (m_hasGamePort == hasGamePort)
        {
            return;
        }

        m_hasGamePort = hasGamePort;

        if (!hasGamePort)
        {
            for (auto & [token, driver] : m_drivers)
            {
                driver.logical.reset();
            }
        }
    }

    m_devicesDirty = true;

    if (!hasGamePort)
    {
        ReleaseContribution();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelectionChangedFn
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetSelectionChangedFn (SelectionChangedFn onSelectionChanged)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_onSelectionChanged = std::move (onSelectionChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWakeFn
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetWakeFn (WakeFn wake)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_wake = std::move (wake);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetStateChangedFn
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetStateChangedFn (StateChangedFn onStateChanged)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_onStateChanged = std::move (onStateChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisCount
//
//  A machine with fewer axes drops the drivers left with none of them and the
//  values for the axes it lacks; the player slots themselves are kept, so
//  switching back to a machine with four plays them again.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetAxisCount (size_t axisCount)
{
    std::unique_lock<std::mutex>  lock   (m_mutex);
    size_t                        count  = std::min (axisCount, GamePortContribution::kAxisCount);
    GamePortContribution          merged;



    if (m_axisCount == count)
    {
        return;
    }

    m_axisCount = count;
    SyncDriversLocked();

    merged = BuildMergedLocked();
    lock.unlock();

    Publish (merged);
    Wake();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiplayer
//
//  The setup is normalized before it is kept, so what the service plays is
//  never an overlapping or repeated pair of slots, whatever the caller handed
//  in -- a hand-edited prefs file as much as the settings page.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetMultiplayer (MultiplayerSetup setup)
{
    std::unique_lock<std::mutex>  lock       (m_mutex);
    MultiplayerSetup              normalized = ControllerSelectionPolicy::Normalize (std::move (setup));
    GamePortContribution          merged;



    if (m_multiplayer == normalized)
    {
        return;
    }

    m_multiplayer = normalized;
    SyncDriversLocked();

    merged = BuildMergedLocked();
    lock.unlock();

    Publish (merged);
    Wake();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiplayerEnabled
//
//  BOTH SLOTS ARE KEPT when the mode goes off. Turning multiplayer off is how
//  a user hands the game port back to one controller for a single-player game,
//  and throwing the players away would make turning it back on a setup job
//  rather than a click.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetMultiplayerEnabled (bool isEnabled)
{
    MultiplayerSetup  setup  = GetMultiplayer();
    size_t            player = 0;



    setup.isEnabled = isEnabled;

    // TWO PLAYERS START WITH THE CONTROLLERS THAT ARE THERE. An empty slot
    // means nobody plays it, which reads as the mode doing nothing, so every
    // empty slot takes an attached controller the other slot does not hold.
    // Filling only when BOTH were empty left the second slot empty for a user
    // who had already chosen the first player's controller.
    if (isEnabled)
    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        for (player = 0; player < MultiplayerSetup::kPlayerCount; player++)
        {
            size_t  other = (player == 0) ? 1 : 0;

            if (setup.players[player].unit.has_value())
            {
                continue;
            }

            for (const ControllerDeviceInfo & device : m_devices)
            {
                if (setup.players[other].unit.has_value() && setup.players[other].unit.value() == device.unit)
                {
                    continue;
                }

                setup.players[player].unit   = device.unit;
                setup.players[player].target = (player == 0) ? PlayerAxisTarget::Joystick0 : PlayerAxisTarget::Joystick1;
                break;
            }
        }
    }

    SetMultiplayer (setup);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiplayerSlot
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetMultiplayerSlot (
    size_t                                    player,
    const std::optional<ControllerUnitKey> &  unit,
    PlayerAxisTarget                          target)
{
    MultiplayerSetup  setup = GetMultiplayer();



    if (player >= MultiplayerSetup::kPlayerCount)
    {
        return;
    }

    setup.players[player].unit   = unit;
    setup.players[player].target = target;

    SetMultiplayer (setup);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMultiplayer
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup ControllerInputService::GetMultiplayer() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_multiplayer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetPaddleRate
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ResetPaddleRate()
{
    m_rateResetPending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClock
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetClock (ClockFn clock)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_clock           = std::move (clock);
    m_lastTickSeconds = -1.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetModelSettings
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetModelSettings (std::map<std::string, ControllerModelSettings> models)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_profiles.models = std::move (models);

    // A controller in use picks the new settings up now rather than at its
    // next connect, so OK on the Controllers page takes effect at once.
    UnresolveDriversLocked();
    SyncDriversLocked();
    m_rateResetPending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModelSettings
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, ControllerModelSettings> ControllerInputService::GetModelSettings() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_profiles.models;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActiveProfile
//
//  Switching profiles does not reset the machine. The mappings are resolved
//  again at once, the rate paddles return to center, and every controller's
//  contribution is released, so a button the new profile does not bind comes
//  up and an axis it does not drive centers before the next reading submits
//  what the new profile asks for (FR-030).
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActiveProfile (const ControllerUnitKey & unit, const std::string & name)
{
    std::unique_lock<std::mutex>  lock   (m_mutex);
    std::string                   token  = ControllerTokens::UnitToToken (unit);
    auto                          found  = m_activeProfiles.find (token);
    bool                          isSame = false;



    // An entry is kept even for the Default, so choosing it is remembered as
    // a choice; only a matching entry is a no-op.
    isSame = found != m_activeProfiles.end()
             && found->second.size() == name.size()
             && _stricmp (found->second.c_str(), name.c_str()) == 0;

    if (isSame)
    {
        return;
    }

    m_activeProfiles[token] = name;
    m_rateResetPending      = true;
    UnresolveDriversLocked();
    SyncDriversLocked();

    lock.unlock();

    ReleaseContribution();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfile
//
//  Empty for the Default, and for a controller that has never had one chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerInputService::GetActiveProfile (const ControllerUnitKey & unit) const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return GetActiveProfileLocked (unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfileLocked
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerInputService::GetActiveProfileLocked (const ControllerUnitKey & unit) const
{
    auto  found = m_activeProfiles.find (ControllerTokens::UnitToToken (unit));



    return (found != m_activeProfiles.end()) ? found->second : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActiveProfiles
//
//  The whole map, by unit token: set once from the saved prefs, and read back
//  to save them.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActiveProfiles (std::map<std::string, std::string> activeProfiles)
{
    std::unique_lock<std::mutex>  lock (m_mutex);



    m_activeProfiles   = std::move (activeProfiles);
    m_rateResetPending = true;
    UnresolveDriversLocked();
    SyncDriversLocked();

    lock.unlock();

    ReleaseContribution();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ControllerInputService::GetActiveProfiles() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_activeProfiles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCalibrations
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetCalibrations (std::map<std::string, ControllerCalibration> calibrations)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_calibrations = std::move (calibrations);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCalibrations
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, ControllerCalibration> ControllerInputService::GetCalibrations() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_calibrations;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestRescan
//
//  A selection handed in that did not change -- none, to a machine that had
//  none -- would otherwise never reach the policy, since only a device
//  notification marks the list dirty.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RequestRescan()
{
    m_devicesDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInspectedUnit
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetInspectedUnit (const std::optional<ControllerUnitKey> & unit)
{
    WakeFn  wake;
    bool    shouldWake = false;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        // The same unit asked for again keeps its reading, so a page that
        // re-requests does not blank its own live readout.
        if (m_inspectedUnit != unit)
        {
            m_inspectedUnit      = unit;
            m_hasInspectedSample = false;
            shouldWake           = true;
        }
        else if (unit.has_value() && !m_hasInspectedSample)
        {
            shouldWake = true;
        }

        wake = m_wake;
    }

    if (shouldWake && wake)
    {
        wake();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInspectedSample
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ControllerSample> ControllerInputService::GetInspectedSample (const ControllerUnitKey & unit) const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    if (!m_hasInspectedSample || m_inspectedUnit != unit)
    {
        return std::nullopt;
    }

    return m_inspectedSample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Controller thread. The driving controllers first, then the one the
//  Controllers page shows, if the page is open. While the page is open the
//  thread polls at the measured period regardless of what the driving
//  controllers need, since the controller being edited may be one that sends
//  no change events of its own.
//
////////////////////////////////////////////////////////////////////////////////

ControllerWaitSources ControllerInputService::Tick()
{
    ControllerWaitSources             wait      = TickDrivers();
    std::optional<ControllerUnitKey>  inspected;
    ControllerSample                  sample;
    HRESULT                           hr        = S_OK;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        inspected = m_inspectedUnit;
    }

    if (!inspected.has_value())
    {
        return wait;
    }

    hr = m_backend.ReadSample (inspected.value(), sample);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        if (m_inspectedUnit == inspected)
        {
            m_inspectedSample    = sample;
            m_hasInspectedSample = SUCCEEDED (hr) && sample.connected;
        }
    }

    wait.timeoutMs = kPollPeriodMs;

    return wait;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickDrivers
//
//  Controller thread. Reads each driving controller once, submits what they
//  ask of the game port together, then says how long to wait for the next
//  wake: the measured poll period while a controller that must be polled is
//  driving, and otherwise nothing at all, since a DirectInput device wakes
//  the thread itself and no driver means nothing to read.
//
////////////////////////////////////////////////////////////////////////////////

ControllerWaitSources ControllerInputService::TickDrivers()
{
    std::vector<DriverRead>   reads;
    std::vector<std::string>  resetTokens;
    ControllerWaitSources     wait;
    GamePortContribution      merged;
    StateChangedFn            onStateChanged;
    float                     elapsedSeconds = 0.0f;
    bool                      isActive       = false;
    bool                      hasFlipped     = false;
    bool                      needsPoll      = false;



    if (m_rateResetPending.exchange (false))
    {
        for (auto & [token, evaluator] : m_evaluators)
        {
            evaluator.ResetRate();
        }
    }

    if (m_devicesDirty.exchange (false))
    {
        RefreshDevices();
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        elapsedSeconds = MeasureElapsedLocked();
        isActive       = m_isActive;

        SyncDriversLocked();
        resetTokens.swap (m_rateResetTokens);

        for (const auto & [token, driver] : m_drivers)
        {
            reads.push_back ({ driver.unit, token, driver.mapping, driver.deadzone,
                               GetDriverAxesLocked (driver.unit).count() });
        }

        m_lastTick                = TickReport();
        m_lastTick.hasSelection   = m_selection.has_value();
        m_lastTick.isActiveXInput = m_selection.has_value() && m_selection.value().model.kind == ControllerKind::XInput;
        m_lastTick.isAppActive    = m_isActive;
        m_lastTick.deadzone       = m_deadzone;

        if (m_selection.has_value() && m_drivers.contains (ControllerTokens::UnitToToken (m_selection.value())))
        {
            const DriverState &  selected = m_drivers.at (ControllerTokens::UnitToToken (m_selection.value()));

            m_lastTick.hasMapping = selected.mapping != ControlMapping();
            m_lastTick.deadzone   = selected.deadzone;
        }
    }

    for (const std::string & token : resetTokens)
    {
        m_evaluators[token].ResetRate();
    }

    for (const DriverRead & read : reads)
    {
        hasFlipped = TickDriver (read, elapsedSeconds, isActive, wait, needsPoll) || hasFlipped;
    }

    ForgetIdleEvaluators (reads);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        merged                = BuildMergedLocked();
        onStateChanged        = m_onStateChanged;
        m_lastTick.didSubmit  = isActive && merged != GamePortContribution();
        m_lastTick.submitted  = merged;
    }

    Publish (merged);

    // Who owns the axes turns on whether a driving controller reads, so the
    // first successful read after one is chosen has to be announced: until
    // then the axes rest at center however far the stick is pushed.
    if (hasFlipped && onStateChanged)
    {
        onStateChanged();
    }

    if (needsPoll)
    {
        wait.timeoutMs = kPollPeriodMs;
    }

    return wait;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickDriver
//
//  Controller thread. One driving controller: read it, evaluate it while
//  Casso is active, and record what it asks for. A controller that could not
//  be read is gone, not resting, and contributes nothing; nothing of its own
//  to wait on either, since the next device notification is what brings it
//  back. Returns whether it connected or disconnected.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::TickDriver (
    const DriverRead       & read,
    float                    elapsedSeconds,
    bool                     isActive,
    ControllerWaitSources  & wait,
    bool                   & outNeedsPoll)
{
    HRESULT                              hr             = S_OK;
    ControllerSample                     sample;
    ControllerSample                     calibrated;
    std::optional<GamePortContribution>  logical;
    std::vector<HANDLE>                  events;
    bool                                 isConnected    = false;
    bool                                 hasFlipped     = false;
    bool                                 needsTimedPoll = false;



    hr          = m_backend.ReadSample (read.unit, sample);
    isConnected = SUCCEEDED (hr) && sample.connected;
    calibrated  = RecordReading (read, hr, sample, isConnected, hasFlipped);

    if (isConnected && isActive)
    {
        MappingEvaluator &  evaluator = m_evaluators[read.token];

        logical      = evaluator.Evaluate (calibrated, read.mapping, read.deadzone, elapsedSeconds, read.logicalAxisCount);
        outNeedsPoll = outNeedsPoll || evaluator.IsRateMoving();
    }

    if (isConnected)
    {
        // What the thread waits on until the next read: the controller's own
        // change events where it has them, and the measured poll period only
        // for the ones that have none.
        m_backend.GetWakeSources (read.unit, events, needsTimedPoll);
        wait.events.insert (wait.events.end(), events.begin(), events.end());
        outNeedsPoll = outNeedsPoll || needsTimedPoll;
    }

    {
        std::lock_guard<std::mutex>  lock  (m_mutex);
        auto                         found = m_drivers.find (read.token);

        // A setter may have dropped this driver while it was being read.
        if (found != m_drivers.end())
        {
            found->second.logical = logical;
        }
    }

    return hasFlipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordReading
//
//  Under the lock: whether the driver reads, and the reading put through its
//  calibration. A DirectInput unit is read through its own calibration; an
//  Xbox-class controller is factory-calibrated and never gets one (FR-018a).
//  The first reading after it starts driving or comes back is where it rests,
//  so that is the center (FR-007).
//
////////////////////////////////////////////////////////////////////////////////

ControllerSample ControllerInputService::RecordReading (
    const DriverRead        & read,
    HRESULT                   hr,
    const ControllerSample  & sample,
    bool                      isConnected,
    bool                    & outHasFlipped)
{
    std::lock_guard<std::mutex>  lock         (m_mutex);
    auto                         found        = m_drivers.find (read.token);
    ControllerSample             calibrated   = sample;
    bool                         wasConnected = found != m_drivers.end() && found->second.isConnected;
    bool                         isSelected   = m_selection.has_value() && m_selection.value() == read.unit;



    if (isConnected && read.unit.model.kind == ControllerKind::DirectInput)
    {
        ControllerCalibration &  calibration = m_calibrations[read.token];

        if (!wasConnected)
        {
            calibration.CaptureCenter (sample);
        }

        calibration.Observe (sample);
        calibrated = calibration.Apply (sample);
    }

    if (found != m_drivers.end())
    {
        found->second.isConnected = isConnected;
    }

    if (isSelected)
    {
        m_isSelectedConnected  = isConnected;
        m_lastSample           = isConnected ? sample : ControllerSample();
        m_lastTick.readResult  = hr;
        m_lastTick.isConnected = isConnected;
    }

    outHasFlipped = isConnected != wasConnected;

    return calibrated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForgetIdleEvaluators
//
//  Controller thread. A controller that stopped driving gives up its rate
//  paddles; if it drives again it starts from center.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ForgetIdleEvaluators (const std::vector<DriverRead> & reads)
{
    std::erase_if (m_evaluators, [&reads] (const std::pair<const std::string, MappingEvaluator> & entry)
    {
        return std::none_of (reads.begin(), reads.end(), [&entry] (const DriverRead & read) { return read.token == entry.first; });
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSnapshot
//
//  What the UI reads: it never touches a device itself.
//
////////////////////////////////////////////////////////////////////////////////

ControllerInputService::Snapshot ControllerInputService::GetSnapshot() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);
    Snapshot                     snapshot;



    snapshot.devices             = m_devices;
    snapshot.selection           = m_selection;
    snapshot.saved               = m_saved;
    snapshot.activeProfiles      = m_activeProfiles;
    snapshot.lastSample          = m_lastSample;
    snapshot.isSelectedConnected = m_isSelectedConnected;
    snapshot.multiplayer         = m_multiplayer;
    snapshot.isMultiplayerLive   = ControllerSelectionPolicy::IsMultiplayerPlayable (m_multiplayer, m_devices);
    snapshot.axisCount           = m_axisCount;

    for (const auto & [token, driver] : m_drivers)
    {
        snapshot.isAnyDriverConnected = snapshot.isAnyDriverConnected || driver.isConnected;
    }

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLastTickReport
//
////////////////////////////////////////////////////////////////////////////////

ControllerInputService::TickReport ControllerInputService::GetLastTickReport() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_lastTick;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshDevices
//
//  Re-reads what is attached and lets the policy move the selection. A
//  selected controller that is gone hands the selection to the one attached
//  longest, or to nothing, and the contribution it was holding is released at
//  once: with nothing selected there is no read left to fail and release it.
//  Every other driving controller keeps what it holds.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RefreshDevices()
{
    HRESULT                              hr                   = S_OK;
    std::vector<ControllerDeviceInfo>    devices;
    std::vector<ControllerDeviceInfo>    byAttachOrder;
    ControllerSelectionPolicy::Decision  decision;
    SelectionChangedFn                   onSelectionChanged;
    StateChangedFn                       onStateChanged;
    GamePortContribution                 merged;
    std::wstring                         departedDescription;
    bool                                 wasSelectionAttached = false;
    bool                                 hasListChanged       = false;
    size_t                               i                    = 0;



    hr = m_backend.EnumerateDevices (devices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        //  WHETHER THE SELECTION WAS HERE BEFORE THIS SCAN. A selection that
        //  is cleared because its controller left is news; one that is
        //  cleared because a saved controller was never plugged in is not.
        if (m_selection.has_value() && FindDeviceLocked (m_selection.value()) != nullptr)
        {
            wasSelectionAttached = true;
            departedDescription  = FindDeviceLocked (m_selection.value())->description;
        }

        //  WHAT IS ATTACHED, compared on its own. An arrival or removal that
        //  moves nothing else -- a second controller coming or going while
        //  another one drives -- still changes the picker's rows, and nothing
        //  else would announce it.
        hasListChanged = m_devices.size() != devices.size();

        for (i = 0; !hasListChanged && i < devices.size(); i++)
        {
            hasListChanged = !(m_devices[i].unit == devices[i].unit)
                             || m_devices[i].description != devices[i].description;
        }

        m_devices = devices;
        UpdateAttachOrderLocked();

        // A setup saved when Xbox-class units were keyed by XInput slot moves
        // onto the unit in that slot, once. Reporting it as a list change is
        // what gets it saved, so the next launch finds the product keys.
        if (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (m_multiplayer, m_devices))
        {
            m_multiplayer  = ControllerSelectionPolicy::Normalize (m_multiplayer);
            hasListChanged = true;
        }

        byAttachOrder = m_devices;
        std::stable_sort (byAttachOrder.begin(), byAttachOrder.end(),
            [this] (const ControllerDeviceInfo & a, const ControllerDeviceInfo & b)
            {
                return GetAttachOrderLocked (a.unit) < GetAttachOrderLocked (b.unit);
            });

        decision = ControllerSelectionPolicy::Evaluate (m_selection, byAttachOrder, m_hasGamePort, m_multiplayer);

        if (decision.reason == SelectionChangeReason::Cleared && !wasSelectionAttached)
        {
            decision.isAnnounced = false;
        }

        decision.departedDescription = departedDescription;

        if (decision.hasChanged)
        {
            // A unit that came back under another identity keeps the player
            // slot it was in.
            if (decision.reason == SelectionChangeReason::Adoption && m_selection.has_value() && decision.selection.has_value())
            {
                for (MultiplayerSlot & slot : m_multiplayer.players)
                {
                    if (slot.unit.has_value() && slot.unit.value() == m_selection.value())
                    {
                        slot.unit = decision.selection.value();
                    }
                }
            }

            // EVERY CHANGE IS SAVED BUT A CLEAR. Nothing being attached is not
            // a choice, and writing it down threw away the controller the
            // machine had, so the next switch to it picked whatever was
            // attached longest instead.
            if (decision.reason != SelectionChangeReason::Cleared)
            {
                m_saved = decision.selection;
            }

            m_selection           = decision.selection;
            m_lastSample          = ControllerSample();
            m_isSelectedConnected = false;
        }

        // Also resolves the mapping of a driving controller that has just
        // arrived: an empty mapping reads every control as unbound.
        SyncDriversLocked();

        merged             = BuildMergedLocked();
        onSelectionChanged = m_onSelectionChanged;
        onStateChanged     = m_onStateChanged;
    }

    if (decision.hasChanged)
    {
        Publish (merged);
    }

    // Outside the lock: the sink persists the choice and raises a notice, and
    // neither belongs under a lock the controller thread holds every tick.
    if (decision.hasChanged && onSelectionChanged)
    {
        onSelectionChanged (decision);
    }

    // The picker lists what is attached and checks what is selected, and who
    // owns the axes turns on the selection. All of that is decided on the UI
    // thread.
    if ((decision.hasChanged || hasListChanged) && onStateChanged)
    {
        onStateChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Publish
//
//  Submits what the driving controllers ask for together, or releases the
//  controller source when they ask for nothing, without disturbing what any
//  other input source is holding. Called outside the lock.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::Publish (const GamePortContribution & merged)
{
    if (merged == GamePortContribution())
    {
        ReleaseContribution();
        return;
    }

    m_mixer.Submit (GamePortSource::Controller, merged);
    m_hasContribution = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseContribution
//
//  Returns the controllers' axes to center and their buttons to released,
//  without disturbing what any other input source is holding.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ReleaseContribution()
{
    if (m_hasContribution.exchange (false))
    {
        m_mixer.ReleaseSource (GamePortSource::Controller);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Wake
//
//  Brings the controller thread round to read a driver that has just been
//  added, rather than at the end of whatever wait it is in.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::Wake()
{
    WakeFn  wake;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        wake = m_wake;
    }

    if (wake)
    {
        wake();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateAttachOrderLocked
//
//  Stamps each newly attached controller with a rising number and forgets the
//  ones that have gone. The numbers are what makes the controller that takes
//  over the longest-attached one rather than whichever one enumeration
//  happens to list first (FR-008a).
//
//  A controller that leaves and comes back is a NEW arrival, and goes to the
//  back: it was not there for the stretch it was unplugged, so calling it the
//  longest-attached would be counting time it did not serve.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::UpdateAttachOrderLocked()
{
    std::erase_if (m_attachOrder, [this] (const std::pair<ControllerUnitKey, uint64_t> & entry)
    {
        return FindDeviceLocked (entry.first) == nullptr;
    });

    for (const ControllerDeviceInfo & device : m_devices)
    {
        auto  found = std::find_if (m_attachOrder.begin(), m_attachOrder.end(),
            [&device] (const std::pair<ControllerUnitKey, uint64_t> & entry) { return entry.first == device.unit; });

        if (found == m_attachOrder.end())
        {
            m_attachOrder.push_back ({ device.unit, m_nextAttachOrder++ });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAttachOrderLocked
//
//  The number UpdateAttachOrderLocked stamped on this controller: lower means
//  attached longer. A controller with no stamp sorts last.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ControllerInputService::GetAttachOrderLocked (const ControllerUnitKey & unit) const
{
    for (const std::pair<ControllerUnitKey, uint64_t> & entry : m_attachOrder)
    {
        if (entry.first == unit)
        {
            return entry.second;
        }
    }

    return UINT64_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriverUnitsLocked
//
//  The controllers that drive the game port: the selection in single-source
//  mode, and in multiplayer the players whose slots this machine has the
//  paddles for. A player mapped to paddles a //c lacks is not read at all, so
//  neither their axes nor their button reach it -- and the slot is kept, so a
//  //e plays it again (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ControllerUnitKey> ControllerInputService::GetDriverUnitsLocked() const
{
    std::vector<ControllerUnitKey>  units;
    MultiplayerSetup                live   = GetLiveMultiplayerLocked();
    size_t                          player = 0;



    if (!live.isEnabled)
    {
        if (m_selection.has_value())
        {
            units.push_back (m_selection.value());
        }

        return units;
    }

    for (player = 0; player < MultiplayerSetup::kPlayerCount; player++)
    {
        const std::optional<ControllerUnitKey>  & unit      = live.players[player].unit;
        bool                                      isPlaying = ControllerSelectionPolicy::GetAxesForPlayer (live, player, m_axisCount).any();

        if (!unit.has_value() || !isPlaying)
        {
            continue;
        }

        units.push_back (unit.value());
    }

    return units;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLiveMultiplayerLocked
//
//  The saved setup, turned off while it cannot be played.
//
//  EVERY RULE BELOW READS THIS, NOT m_multiplayer. The saved setup is what the
//  user asked for and what the prefs keep; this is what the machine plays. A
//  user whose players are unplugged gets single-source play on whatever is
//  attached, and the mode returns by itself when they plug back in, because
//  nothing about the saved setup changed (FR-040).
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup ControllerInputService::GetLiveMultiplayerLocked() const
{
    MultiplayerSetup  live = m_multiplayer;



    live.isEnabled = ControllerSelectionPolicy::IsMultiplayerPlayable (m_multiplayer, m_devices);

    return live;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLiveMultiplayer
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup ControllerInputService::GetLiveMultiplayer() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return GetLiveMultiplayerLocked();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriverAxesLocked
//
//  The machine paddles one controller drives: its player's, or PDL0 and PDL1
//  for the selection in single-source mode, in both cases less the paddles
//  this machine does not have.
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup::AxisSet ControllerInputService::GetDriverAxesLocked (const ControllerUnitKey & unit) const
{
    MultiplayerSetup           live   = GetLiveMultiplayerLocked();
    std::optional<size_t>      player = ControllerSelectionPolicy::FindPlayer (live, unit);
    MultiplayerSetup::AxisSet  axes;
    size_t                     axis   = 0;



    if (live.isEnabled)
    {
        if (player.has_value())
        {
            axes = ControllerSelectionPolicy::GetAxesForPlayer (live, player.value(), m_axisCount);
        }

        return axes;
    }

    if (m_selection.has_value() && m_selection.value() == unit)
    {
        axes = MultiplayerSetup::AxisSet (ControllerSelectionPolicy::kSingleSourceAxisBits);

        for (axis = m_axisCount; axis < axes.size(); axis++)
        {
            axes.reset (axis);
        }
    }

    return axes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDriverLocked
//
//  In single-source mode the selection drives whether or not it has an axis
//  left, because it still reaches the buttons; in multiplayer a player with no
//  paddle on this machine drives nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::IsDriverLocked (const ControllerUnitKey & unit) const
{
    if (GetLiveMultiplayerLocked().isEnabled)
    {
        return GetDriverAxesLocked (unit).any();
    }

    return m_selection.has_value() && m_selection.value() == unit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncDriversLocked
//
//  Brings the driver list in line with the selection and the player slots. A
//  controller that stopped driving is dropped with whatever it held; one that
//  started driving gets its mapping and has its rate paddles centered on the
//  next tick; one that drives on is left exactly as it was, so a change to
//  another controller never interrupts it (SC-012).
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SyncDriversLocked()
{
    std::vector<ControllerUnitKey>  units = GetDriverUnitsLocked();



    std::erase_if (m_drivers, [&units] (const std::pair<const std::string, DriverState> & entry)
    {
        return std::find (units.begin(), units.end(), entry.second.unit) == units.end();
    });

    for (const ControllerUnitKey & unit : units)
    {
        std::string  token            = ControllerTokens::UnitToToken (unit);
        auto         [found, isAdded] = m_drivers.try_emplace (token);

        if (isAdded)
        {
            found->second.unit = unit;
            m_rateResetTokens.push_back (token);
        }

        ResolveMappingLocked (found->second);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMappingLocked
//
//  Gives a driving controller the mapping it plays with. An empty mapping
//  reads every control as unbound, so a controller without one sits at center
//  with its buttons up no matter what the user does with it -- which is why
//  this runs from the refresh, the setters and the tick alike: a controller
//  can start driving before it is enumerated.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ResolveMappingLocked (DriverState & driver)
{
    const ControllerDeviceInfo  * device  = FindDeviceLocked (driver.unit);
    const ControllerProfile     * profile = nullptr;
    std::string                   active;



    if (device == nullptr || driver.isResolved)
    {
        return;
    }

    // The deadzone belongs to the model, whichever profile is active.
    m_profiles.GetDefaultSettings (device->unit.model, device->controls, driver.mapping, driver.deadzone);
    driver.isResolved = true;

    // THIS CONTROLLER'S profile, not the machine's: two players on two pads
    // of one model can each play their own.
    active = GetActiveProfileLocked (driver.unit);

    if (active.empty())
    {
        return;
    }

    // A remembered profile the model no longer has plays the Default, which
    // is already in hand; nothing is recreated for it (FR-029).
    profile = m_profiles.FindProfile (ControllerTokens::ModelToToken (device->unit.model), active);

    if (profile != nullptr)
    {
        driver.mapping = profile->mapping;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnresolveDriversLocked
//
//  Every driver's mapping is looked up again, and what it held is dropped:
//  the settings or the profile it was resolved from changed.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::UnresolveDriversLocked()
{
    for (auto & [token, driver] : m_drivers)
    {
        driver.mapping    = ControlMapping();
        driver.isResolved = false;
        driver.logical.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildMergedLocked
//
//  Every driving controller's last reading, placed where its slot maps it: its
//  own PDL0, PDL1 and on land on the paddles it drives, in ascending order,
//  and a paddle it does not drive is left for the other player.
//
//  ONE BUTTON LINE PER PLAYER in multiplayer. Player 1's pb0 bindings reach
//  PB0 and player 2's reach PB1, so a two-player game that reads the two lines
//  separately can tell the players apart -- which OR-ing every controller's
//  buttons together made impossible. A player's pb1 and pb2 bindings are kept
//  in the profile and ignored here, and PB2 is unused. In single-source mode
//  the one controller drives PB0-PB2 exactly as before.
//
////////////////////////////////////////////////////////////////////////////////

GamePortContribution ControllerInputService::BuildMergedLocked() const
{
    GamePortContribution  merged;
    size_t                axis    = 0;
    size_t                logical = 0;



    for (const auto & [token, driver] : m_drivers)
    {
        MultiplayerSetup::AxisSet  axes   = GetDriverAxesLocked (driver.unit);
        std::optional<size_t>      player = ControllerSelectionPolicy::FindPlayer (GetLiveMultiplayerLocked(), driver.unit);

        if (!driver.logical.has_value() || !IsDriverLocked (driver.unit))
        {
            continue;
        }

        // `logical` counts only the axes this driver plays, so it can never
        // outrun `axis` and never leaves the array. It is bounded anyway:
        // the invariant is one the reader can follow and the analyzer cannot.
        for (axis = 0, logical = 0; axis < axes.size() && logical < driver.logical->paddle.size(); axis++)
        {
            if (axes.test (axis))
            {
                merged.paddle[axis] = driver.logical->paddle[logical++];
            }
        }

        AddJoyportSwitches (player, driver.logical->switches, merged);

        if (!player.has_value())
        {
            merged.buttons |= driver.logical->buttons;
            continue;
        }

        if (driver.logical->buttons.test (0))
        {
            merged.buttons.set (player.value());
        }
    }

    return merged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddJoyportSwitches
//
//  One driver's Atari switches onto the Joyport's jacks. In multiplayer the
//  player's slot is the jack, slot 1 left and slot 2 right, whatever paddles
//  the slot drives; in single-source mode the one controller appears on both
//  jacks, so a two-player game played by passing the controller reads it on
//  either. A jack no driver reaches stays open.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::AddJoyportSwitches (
    const std::optional<size_t>  & player,
    const JoystickSwitches       & switches,
    GamePortContribution         & merged)
{
    JoyportJacks  jacks = merged.jacks.value_or (JoyportJacks());



    if (!player.has_value())
    {
        jacks.jack[JoyportJacks::kLeftJack]  |= switches;
        jacks.jack[JoyportJacks::kRightJack] |= switches;
    }
    else if (player.value() < JoyportJacks::kJackCount)
    {
        jacks.jack[player.value()] |= switches;
    }

    merged.jacks = jacks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureElapsedLocked
//
//  The time since the last reading, which is what a rate binding moves its
//  paddle by. The first reading has none to measure from.
//
////////////////////////////////////////////////////////////////////////////////

float ControllerInputService::MeasureElapsedLocked()
{
    double  nowSeconds = m_clock ? m_clock()
                                 : std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
    float   elapsed    = (m_lastTickSeconds < 0.0) ? 0.0f : (float) (nowSeconds - m_lastTickSeconds);



    m_lastTickSeconds = nowSeconds;

    return elapsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDeviceLocked
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerInputService::FindDeviceLocked (const ControllerUnitKey & unit) const
{
    for (const ControllerDeviceInfo & device : m_devices)
    {
        if (device.unit == unit)
        {
            return &device;
        }
    }

    return nullptr;
}
