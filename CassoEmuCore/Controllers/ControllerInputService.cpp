#include "Pch.h"

#include "Controllers/ControllerInputService.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
#include "Controllers/JoyportJackRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriverRead
//
//  What one tick needs to read and evaluate one controller, copied out from
//  under the lock so the backend and the evaluator run outside it, and what
//  the read found.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerInputService::DriverRead
{
    ControllerUnitKey  unit;
    std::string        token;
    ControlMapping     mapping;
    float              deadzone         = 0.0f;
    size_t             logicalAxisCount = 0;
    bool               isDriving        = false;
    bool               isWatched        = false;
    bool               isLogged         = false;
    bool               needsTimedPoll   = false;
    bool               isRead           = false;

    HRESULT            readResult       = S_OK;
    ControllerSample   calibrated;
    bool               isConnected      = false;
    bool               hasFlipped       = false;
    bool               hasRealInput     = false;
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
//  one nothing is submitted, and the players are kept untouched for a machine
//  that has one.
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
//  SetSlotsChangedFn
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetSlotsChangedFn (SlotsChangedFn onSlotsChanged)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_onSlotsChanged = std::move (onSlotsChanged);
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
//  A machine with fewer axes drops the values for the axes it lacks, and a
//  player with none of their paddles on it plays nothing; the players
//  themselves are kept, so switching back to a machine with four plays them
//  again.
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
//  SetPlayerEntries
//
//  Takes effect at once, whether or not a picked controller has given input.
//  A controller that stops playing releases what it held; one that starts
//  has its rate paddles centered and is read on the next tick, which the
//  thread is woken for.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetPlayerEntries (const PlayerEntries & entries)
{
    std::unique_lock<std::mutex>  lock       (m_mutex);
    PlayerEntries                 normalized = PlayerSlotPolicy::NormalizeEntries (entries);
    std::optional<SlotsChange>    change;
    GamePortContribution          merged;



    if (m_entries == normalized)
    {
        return;
    }

    m_entries = normalized;
    change    = EvaluateSlotsLocked();

    SyncDriversLocked();
    merged = BuildMergedLocked();
    lock.unlock();

    Publish (merged);
    Wake();
    AnnounceSlots (change);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerEntries
//
////////////////////////////////////////////////////////////////////////////////

PlayerEntries ControllerInputService::GetPlayerEntries() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickPlayerEntry
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::PickPlayerEntry (size_t player, const PlayerEntry & entry)
{
    SetPlayerEntries (PlayerSlotPolicy::ApplyPick (GetPlayerEntries(), player, entry));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerSlots
//
////////////////////////////////////////////////////////////////////////////////

PlayerSlots ControllerInputService::GetPlayerSlots() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_slots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLastHolders
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetLastHolders (const PlayerLastHolders & lastHolders)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_lastHolders = lastHolders;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLastHolders
//
////////////////////////////////////////////////////////////////////////////////

PlayerLastHolders ControllerInputService::GetLastHolders() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_lastHolders;
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
    std::unique_lock<std::mutex>  lock (m_mutex);
    std::optional<SlotsChange>    change;



    m_profiles.models = std::move (models);

    // A controller in use picks the new settings up now rather than at its
    // next connect, so OK on the Controllers page takes effect at once, and a
    // slot that follows the profile moves with it.
    UnresolveDriversLocked();
    change = EvaluateSlotsLocked();
    SyncDriversLocked();
    m_rateResetPending = true;

    lock.unlock();

    AnnounceSlots (change);
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
//  what the new profile asks for (FR-030). A slot that follows the profile
//  takes the target the new one implies.
//
//  A profile of the other mode cannot be chosen, so it leaves the choice as
//  it was.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActiveProfile (const ControllerUnitKey & unit, const std::string & name)
{
    std::unique_lock<std::mutex>  lock        (m_mutex);
    std::string                   token       = ControllerTokens::UnitToToken (unit);
    auto &                        active      = GetModeProfilesLocked();
    auto                          found       = active.find (token);
    bool                          isSame      = false;
    bool                          isOtherMode = IsOfOtherModeLocked (unit.model, name, m_profileMode);
    std::optional<SlotsChange>    change;



    // An entry is kept even for the Default, so choosing it is remembered as
    // a choice; only a matching entry is a no-op.
    isSame = found != active.end()
             && found->second.size() == name.size()
             && _stricmp (found->second.c_str(), name.c_str()) == 0;

    if (isSame || isOtherMode)
    {
        return;
    }

    active[token]      = name;
    m_rateResetPending = true;
    UnresolveDriversLocked();
    change = EvaluateSlotsLocked();
    SyncDriversLocked();

    lock.unlock();

    ReleaseContribution();
    AnnounceSlots (change);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfile
//
//  The profile chosen for the mode being played. Empty for that mode's
//  built-in profile, and for a controller that has never had one chosen in
//  it.
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
    const auto &  active = GetModeProfilesLocked();
    auto          found  = active.find (ControllerTokens::UnitToToken (unit));



    return (found != active.end()) ? found->second : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeProfilesLocked
//
//  The active profiles for the mode being played.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> & ControllerInputService::GetModeProfilesLocked()
{
    return m_profiles.GetActiveProfiles (m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeProfilesLocked
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, std::string> & ControllerInputService::GetModeProfilesLocked() const
{
    return m_profiles.GetActiveProfiles (m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActiveProfiles
//
//  The whole map for one mode, by unit token: set once from the saved prefs,
//  and read back to save them.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActiveProfiles (ProfileMode mode, std::map<std::string, std::string> activeProfiles)
{
    std::unique_lock<std::mutex>  lock (m_mutex);
    std::optional<SlotsChange>    change;



    m_profiles.GetActiveProfiles (mode) = std::move (activeProfiles);

    m_rateResetPending = true;
    UnresolveDriversLocked();
    change = EvaluateSlotsLocked();
    SyncDriversLocked();

    lock.unlock();

    ReleaseContribution();
    AnnounceSlots (change);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetJoyportAttached
//
//  Every controller switches to the profile chosen for the new mode, or with
//  none chosen, to that mode's built-in profile, released as for a profile
//  change, and a slot that follows the profile follows it here too.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetJoyportAttached (bool isAttached)
{
    std::unique_lock<std::mutex>  lock   (m_mutex);
    ProfileMode                   mode   = isAttached ? ProfileMode::Joyport : ProfileMode::Joystick;
    std::optional<SlotsChange>    change;



    if (m_profileMode == mode)
    {
        return;
    }

    m_profileMode      = mode;
    m_rateResetPending = true;
    UnresolveDriversLocked();
    change = EvaluateSlotsLocked();
    SyncDriversLocked();

    lock.unlock();

    ReleaseContribution();
    AnnounceSlots (change);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
//  What is saved, so a choice of the other mode's profile, which can come
//  only from the prefs, is dropped here rather than written back.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ControllerInputService::GetActiveProfiles (ProfileMode mode) const
{
    std::lock_guard<std::mutex>                 lock    (m_mutex);
    const std::map<std::string, std::string> &  all     = m_profiles.GetActiveProfiles (mode);
    std::map<std::string, std::string>          choices;
    ControllerUnitKey                           unit;
    HRESULT                                     hr      = S_OK;



    for (const auto & entry : all)
    {
        hr = ControllerTokens::UnitFromToken (entry.first, unit);

        if (SUCCEEDED (hr) && IsOfOtherModeLocked (unit.model, entry.second, mode))
        {
            continue;
        }

        choices.insert (entry);
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOfOtherModeLocked
//
//  A model with nothing saved still has its built-in profiles by name.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::IsOfOtherModeLocked (const ControllerModelKey & model, const std::string & name, ProfileMode mode) const
{
    auto  found = m_profiles.models.find (ControllerTokens::ModelToToken (model));



    if (found == m_profiles.models.end())
    {
        return ControllerModelSettings().IsOfOtherMode (name, mode);
    }

    return found->second.IsOfOtherMode (name, mode);
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
//  Reads what is attached again on the next tick, which is otherwise done
//  only after a device notification.
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
//  Controller thread. The attached controllers first, then the one the
//  Controllers page shows, if the page is open. While the page is open the
//  thread polls at the measured period regardless of what the playing
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
//  Controller thread. Reads each playing controller once, and while a player
//  on Automatic waits for a controller and Casso is active, watches the ones
//  nobody plays for their first real input: those with change events of
//  their own on every wake, the rest every kIdleWatchPeriodMs. A first input
//  goes into the input log and can give its controller a slot, and one given
//  a slot that way plays the very reading that gave it, rather than one tick
//  later. Then everything the playing controllers ask of the game port is
//  submitted together, and the thread is told how long to wait: the measured
//  poll period while a playing controller must be polled, the time to the
//  next idle read while a watched one must be, the shorter of the two when
//  both, and otherwise nothing at all, since a DirectInput device wakes the
//  thread itself.
//
////////////////////////////////////////////////////////////////////////////////

ControllerWaitSources ControllerInputService::TickDrivers()
{
    std::vector<DriverRead>     reads;
    std::vector<std::string>    resetTokens;
    ControllerWaitSources       wait;
    GamePortContribution        merged;
    StateChangedFn              onStateChanged;
    std::optional<SlotsChange>  change;
    float                       elapsedSeconds = 0.0f;
    double                      nowSeconds     = 0.0;
    double                      sinceWatchMs   = 0.0;
    DWORD                       untilWatchMs   = kIdleWatchPeriodMs;
    bool                        isActive       = false;
    bool                        isWatchOn      = false;
    bool                        isWatchDue     = false;
    bool                        hasTimedWatch  = false;
    bool                        hasFlipped     = false;
    bool                        hasNewInput    = false;
    bool                        needsPoll      = false;
    constexpr double            kMsPerSecond   = 1000.0;



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
        nowSeconds     = m_lastTickSeconds;
        isActive       = m_isActive;
        sinceWatchMs   = (nowSeconds - m_lastWatchSeconds) * kMsPerSecond;

        // THE WATCH IS OFF WHILE CASSO IS INACTIVE, so input made while
        // another application is active never claims a slot, and while no
        // player waits for a controller, so an idle Casso reads nothing.
        isWatchOn  = m_isActive && PlayerSlotPolicy::NeedsIdleWatch (m_entries, m_slots);
        isWatchDue = m_lastWatchSeconds < 0.0 || sinceWatchMs >= kIdleWatchPeriodMs;

        SyncDriversLocked();

        for (const auto & [token, driver] : m_drivers)
        {
            DriverRead  read;
            bool        isWatched = !driver.isDriving;

            if (isWatched && (!isWatchOn || driver.hasFailed))
            {
                continue;
            }

            read.unit      = driver.unit;
            read.token     = token;
            read.deadzone  = driver.deadzone;
            read.isWatched = isWatched;
            read.isLogged  = std::find (m_logs.firstInput.begin(), m_logs.firstInput.end(), driver.unit) != m_logs.firstInput.end();

            reads.push_back (read);
        }

        m_lastTick              = TickReport();
        m_lastTick.hasPlayerOne = m_slots[0].holder.has_value();
        m_lastTick.isAppActive  = m_isActive;
        m_lastTick.deadzone     = m_deadzone;

        if (m_slots[0].holder.has_value())
        {
            auto  found = m_drivers.find (ControllerTokens::UnitToToken (m_slots[0].holder.value()));

            m_lastTick.isActiveXInput = m_slots[0].holder->model.kind == ControllerKind::XInput;

            if (found != m_drivers.end())
            {
                m_lastTick.hasMapping = found->second.mapping != ControlMapping();
                m_lastTick.deadzone   = found->second.deadzone;
            }
        }
    }

    hasTimedWatch = PrepareWatch (reads, isWatchDue, wait);

    for (DriverRead & read : reads)
    {
        if (!read.isRead)
        {
            continue;
        }

        ReadDriver (read, isActive);
        hasFlipped  = hasFlipped  || read.hasFlipped;
        hasNewInput = hasNewInput || read.hasRealInput;
    }

    // First inputs first, so the slots they change decide who is evaluated.
    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        if (hasTimedWatch && isWatchDue)
        {
            m_lastWatchSeconds = nowSeconds;
        }

        for (const DriverRead & read : reads)
        {
            if (read.hasRealInput)
            {
                m_logs.firstInput.push_back (read.unit);
            }
        }

        if (hasNewInput)
        {
            change = EvaluateSlotsLocked();
        }

        SyncDriversLocked();
        resetTokens.swap (m_rateResetTokens);

        for (DriverRead & read : reads)
        {
            auto                                     found = m_drivers.find (read.token);
            std::optional<PlayerTargetRules::Route>  route = GetDriverRouteLocked (read.unit);

            if (found == m_drivers.end())
            {
                continue;
            }

            read.isDriving        = found->second.isDriving;
            read.mapping          = found->second.mapping;
            read.deadzone         = found->second.deadzone;
            read.logicalAxisCount = route.has_value() ? PlayerTargetRules::CountPaddles (route.value()) : 0;
        }
    }

    for (const std::string & token : resetTokens)
    {
        m_evaluators[token].ResetRate();
    }

    for (DriverRead & read : reads)
    {
        if (read.isRead)
        {
            EvaluateDriver (read, elapsedSeconds, isActive, wait, needsPoll);
        }
    }

    ForgetIdleEvaluators();

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        merged                = BuildMergedLocked();
        onStateChanged        = m_onStateChanged;
        m_lastTick.didSubmit  = isActive && merged != GamePortContribution();
        m_lastTick.submitted  = merged;
    }

    Publish (merged);
    AnnounceSlots (change);

    // Who owns the axes turns on whether a playing controller reads, so the
    // first successful read after one starts playing has to be announced:
    // until then the axes rest at center however far the stick is pushed.
    if ((hasFlipped || change.has_value()) && onStateChanged)
    {
        onStateChanged();
    }

    if (needsPoll)
    {
        wait.timeoutMs = kPollPeriodMs;
    }

    if (hasTimedWatch)
    {
        if (!isWatchDue)
        {
            untilWatchMs = (DWORD) std::ceil (std::max (0.0, kIdleWatchPeriodMs - sinceWatchMs));
        }

        wait.timeoutMs = std::min (wait.timeoutMs.value_or (untilWatchMs), untilWatchMs);
    }

    return wait;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrepareWatch
//
//  Controller thread. Which of the controllers read this tick are read at
//  all: every playing one, a watched one with change events of its own --
//  whose events the thread now also waits on -- and a watched one without
//  them only when the idle period is due. Returns whether any watched
//  controller needs reading on that period.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::PrepareWatch (std::vector<DriverRead> & reads, bool isWatchDue, ControllerWaitSources & wait)
{
    bool  hasTimedWatch = false;



    for (DriverRead & read : reads)
    {
        std::vector<HANDLE>  events;

        if (!read.isWatched)
        {
            read.isRead = true;
            continue;
        }

        m_backend.GetWakeSources (read.unit, events, read.needsTimedPoll);
        wait.events.insert (wait.events.end(), events.begin(), events.end());

        read.isRead   = !read.needsTimedPoll || isWatchDue;
        hasTimedWatch = hasTimedWatch || read.needsTimedPoll;
    }

    return hasTimedWatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDriver
//
//  Controller thread. One attached controller: read it, and put the reading
//  through its calibration. A controller that could not be read is gone, not
//  resting. Real input counts only while Casso is active, so playing another
//  game on a controller never gives it a slot here.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ReadDriver (DriverRead & read, bool isActive)
{
    ControllerSample  sample;



    read.readResult  = m_backend.ReadSample (read.unit, sample);
    read.isConnected = SUCCEEDED (read.readResult) && sample.connected;
    read.calibrated  = RecordReading (read, read.readResult, sample, read.isConnected, read.hasFlipped);

    read.hasRealInput = isActive
                        && read.isConnected
                        && !read.isLogged
                        && PlayerSlotPolicy::IsRealInput (read.calibrated, nullptr, read.deadzone);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateDriver
//
//  Controller thread. A controller that plays is evaluated while Casso is
//  active and records what it asks for; one that does not play records
//  nothing. Either way a connected controller says what the thread waits on
//  until its next read: its own change events where it has them, and the
//  measured poll period where it has none.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::EvaluateDriver (
    DriverRead             & read,
    float                    elapsedSeconds,
    bool                     isActive,
    ControllerWaitSources  & wait,
    bool                   & outNeedsPoll)
{
    std::optional<GamePortContribution>  logical;
    std::vector<HANDLE>                  events;
    bool                                 needsTimedPoll = false;



    if (read.isConnected && isActive && read.isDriving)
    {
        MappingEvaluator &  evaluator = m_evaluators[read.token];

        logical      = evaluator.Evaluate (read.calibrated, read.mapping, read.deadzone, elapsedSeconds, read.logicalAxisCount);
        outNeedsPoll = outNeedsPoll || evaluator.IsRateMoving();
    }

    // A watched controller's events were gathered with the watch; one that
    // started playing on this reading waits on its own from now on.
    if (read.isConnected && read.isDriving)
    {
        m_backend.GetWakeSources (read.unit, events, needsTimedPoll);
        wait.events.insert (wait.events.end(), events.begin(), events.end());
        outNeedsPoll = outNeedsPoll || needsTimedPoll;
    }

    {
        std::lock_guard<std::mutex>  lock  (m_mutex);
        auto                         found = m_drivers.find (read.token);

        // A setter may have dropped this controller while it was being read.
        if (found != m_drivers.end())
        {
            found->second.logical = logical;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordReading
//
//  Under the lock: whether the controller reads, and the reading put through
//  its calibration. A DirectInput unit is read through its own calibration;
//  an Xbox-class controller is factory-calibrated and never gets one
//  (FR-018a). The first reading after it connects is where it rests, so that
//  is the center (FR-007).
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
    bool                         isPlayerOne  = m_slots[0].holder == read.unit;



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

        // A watched controller that cannot be read is reported and left out
        // of the watch until it reconnects, never taken as one at rest.
        if (!isConnected && read.isWatched)
        {
            found->second.hasFailed = true;
        }
    }

    if (isPlayerOne)
    {
        m_lastTick.readResult  = hr;
        m_lastTick.isConnected = isConnected;
    }

    // Only a controller that plays changes who owns the axes by reading or
    // failing to; one read only for its first input changes nothing.
    outHasFlipped = isConnected != wasConnected && found != m_drivers.end() && found->second.isDriving;

    return calibrated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForgetIdleEvaluators
//
//  Controller thread. A controller that stopped playing gives up its rate
//  paddles; if it plays again it starts from center.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ForgetIdleEvaluators()
{
    std::vector<std::string>  driving;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        for (const auto & [token, driver] : m_drivers)
        {
            if (driver.isDriving)
            {
                driving.push_back (token);
            }
        }
    }

    std::erase_if (m_evaluators, [&driving] (const std::pair<const std::string, MappingEvaluator> & entry)
    {
        return std::find (driving.begin(), driving.end(), entry.first) == driving.end();
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



    snapshot.devices        = m_devices;
    snapshot.entries        = m_entries;
    snapshot.slots          = m_slots;
    snapshot.activeProfiles = GetModeProfilesLocked();
    snapshot.profileMode    = m_profileMode;
    snapshot.axisCount      = m_axisCount;

    for (const auto & [token, driver] : m_drivers)
    {
        snapshot.isAnyDriverConnected = snapshot.isAnyDriverConnected || (driver.isDriving && driver.isConnected);

        if (driver.hasFailed)
        {
            snapshot.unreadable.push_back (driver.unit);
        }
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
//  Re-reads what is attached and lets the players follow it. A controller
//  that held a slot and has gone releases what it was holding at once: with
//  it gone there is no read left to fail and release it. Every other playing
//  controller keeps what it holds.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RefreshDevices()
{
    HRESULT                            hr             = S_OK;
    std::vector<ControllerDeviceInfo>  devices;
    std::vector<std::wstring>          departed;
    std::optional<SlotsChange>         change;
    StateChangedFn                     onStateChanged;
    GamePortContribution               merged;
    MultiplayerSetup                   picks;
    bool                               hasListChanged = false;
    size_t                             i              = 0;



    hr = m_backend.EnumerateDevices (devices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        //  WHAT IS ATTACHED, compared on its own. An arrival or removal that
        //  moves nothing else -- a second controller coming or going while
        //  another one plays -- still changes the picker's rows, and nothing
        //  else would announce it.
        hasListChanged = m_devices.size() != devices.size();

        for (i = 0; !hasListChanged && i < devices.size(); i++)
        {
            hasListChanged = !(m_devices[i].unit == devices[i].unit)
                             || m_devices[i].description != devices[i].description;
        }

        departed = FindDepartedLocked (devices);
        RecordArrivalsLocked (devices);

        m_devices       = devices;
        m_hasEnumerated = true;

        // A pick saved when Xbox-class units were keyed by XInput slot moves
        // onto the unit in that slot, once. Reporting it as a change to the
        // entries is what gets it saved, so the next launch finds the
        // product keys.
        for (i = 0; i < PlayerSlotPolicy::kPlayerCount; i++)
        {
            picks.players[i].unit = (m_entries[i].kind == PlayerEntryKind::Controller) ? m_entries[i].unit : std::nullopt;
        }

        if (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (picks, m_devices))
        {
            for (i = 0; i < PlayerSlotPolicy::kPlayerCount; i++)
            {
                if (m_entries[i].kind == PlayerEntryKind::Controller)
                {
                    m_entries[i].unit = picks.players[i].unit;
                }
            }

            m_entries = PlayerSlotPolicy::NormalizeEntries (m_entries);
        }

        change = EvaluateSlotsLocked (departed);

        SyncDriversLocked();

        merged         = BuildMergedLocked();
        onStateChanged = m_onStateChanged;
    }

    if (change.has_value())
    {
        Publish (merged);
    }

    // Outside the lock: the shell saves the entries and raises notices, and
    // neither belongs under a lock the controller thread holds every tick.
    AnnounceSlots (change);

    if ((change.has_value() || hasListChanged) && onStateChanged)
    {
        onStateChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordArrivalsLocked
//
//  The connection log takes each distinct controller that arrives after the
//  startup scan, in the order they arrive. The controllers present at startup
//  arrived at no particular moment, so they are no order at all; a
//  controller coming back to the slot held for it, or arriving a second
//  time, is not a new arrival -- a wireless pad that sleeps and wakes does
//  not count twice.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RecordArrivalsLocked (const std::vector<ControllerDeviceInfo> & devices)
{
    if (!m_hasEnumerated)
    {
        return;
    }

    for (const ControllerDeviceInfo & device : devices)
    {
        bool  wasAttached = FindDeviceLocked (device.unit) != nullptr;
        bool  isLogged    = std::find (m_logs.connected.begin(), m_logs.connected.end(), device.unit) != m_logs.connected.end();
        bool  isReturning = std::any_of (m_slots.begin(), m_slots.end(), [&device] (const PlayerSlot & slot)
        {
            return slot.state == PlayerSlotState::Held && slot.holder == device.unit;
        });

        if (wasAttached || isLogged || isReturning)
        {
            continue;
        }

        m_logs.connected.push_back (device.unit);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDepartedLocked
//
//  The descriptions of the controllers that were attached and holding a slot
//  and are not in the new list. A picked controller that never connected
//  departed from nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ControllerInputService::FindDepartedLocked (const std::vector<ControllerDeviceInfo> & devices) const
{
    std::vector<std::wstring>  departed;



    for (const PlayerSlot & slot : m_slots)
    {
        const ControllerDeviceInfo  * before = slot.holder.has_value() ? FindDeviceLocked (slot.holder.value()) : nullptr;
        bool                          isHere = false;

        if (before == nullptr || slot.state == PlayerSlotState::Held || slot.state == PlayerSlotState::Empty)
        {
            continue;
        }

        isHere = std::any_of (devices.begin(), devices.end(), [&slot] (const ControllerDeviceInfo & device) { return device.unit == slot.holder.value(); });

        if (!isHere)
        {
            departed.push_back (before->description);
        }
    }

    return departed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateSlotsLocked
//
//  The players again from what is attached, the entries and the two logs,
//  with each attached controller's active profile deciding the target of a
//  slot that follows it. A picked controller found under another identity is
//  followed there, so the entry holds the controller that is playing. Each
//  slot's holder is recorded as its last holder, with a notice for each slot
//  Automatic gave a different controller. Empty when nothing changed.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ControllerInputService::SlotsChange> ControllerInputService::EvaluateSlotsLocked (std::vector<std::wstring> departed)
{
    PlayerSlotPolicy::MappingsByUnit  mappings;
    PlayerSlots                       previous        = m_slots;
    PlayerEntries                     previousEntries = m_entries;
    PlayerLastHolders                 previousHolders = m_lastHolders;
    SlotsChange                       change;
    size_t                            player          = 0;



    for (const ControllerDeviceInfo & device : m_devices)
    {
        ControlMapping  mapping;
        float           deadzone = 0.0f;

        ResolveUnitLocked (device, mapping, deadzone);
        mappings[ControllerTokens::UnitToToken (device.unit)] = mapping;
    }

    m_slots = PlayerSlotPolicy::Evaluate (m_entries, m_devices, m_logs, previous, mappings);

    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        const PlayerSlot  & slot = m_slots[player];

        if (m_entries[player].kind == PlayerEntryKind::Controller && slot.holder.has_value() &&
            FindDeviceLocked (slot.holder.value()) != nullptr && slot.holder != m_entries[player].unit)
        {
            m_entries[player].unit = slot.holder;
        }
    }

    change.notices = PlayerSlotPolicy::RecordHolders (m_slots, m_devices, m_lastHolders, m_entries, m_profileMode == ProfileMode::Joyport);

    if (m_slots == previous && m_entries == previousEntries && m_lastHolders == previousHolders && departed.empty())
    {
        return std::nullopt;
    }

    change.entries                = m_entries;
    change.slots                  = m_slots;
    change.lastHolders            = m_lastHolders;
    change.haveEntriesChanged     = m_entries != previousEntries;
    change.haveLastHoldersChanged = m_lastHolders != previousHolders;
    change.departedDescriptions   = std::move (departed);

    return change;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnnounceSlots
//
//  Outside the lock.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::AnnounceSlots (const std::optional<SlotsChange> & change)
{
    SlotsChangedFn  onSlotsChanged;



    if (!change.has_value())
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        onSlotsChanged = m_onSlotsChanged;
    }

    if (onSlotsChanged)
    {
        onSlotsChanged (change.value());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Publish
//
//  Submits what the playing controllers ask for together, or releases the
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
//  Brings the controller thread round to read a controller that has just
//  started playing, rather than at the end of whatever wait it is in.
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
//  FindPlayerLocked
//
//  Which player's slot holds this controller, or none.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllerInputService::FindPlayerLocked (const ControllerUnitKey & unit) const
{
    size_t  player = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        if (m_slots[player].holder == unit)
        {
            return player;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriverRouteLocked
//
//  What this controller reaches on the game port, or nothing when it does
//  not play.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PlayerTargetRules::Route> ControllerInputService::GetDriverRouteLocked (const ControllerUnitKey & unit) const
{
    std::optional<size_t>  player = FindPlayerLocked (unit);



    if (!player.has_value())
    {
        return std::nullopt;
    }

    return PlayerSlotPolicy::GetDriverRoute (m_slots, m_entries, player.value(), m_axisCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncDriversLocked
//
//  Brings the read list in line with what is attached, and marks which of
//  them play. A controller that stopped playing drops whatever it held; one
//  that started gets its rate paddles centered on the next tick; one that
//  plays on is left exactly as it was, so a change to another controller
//  never interrupts it (SC-012).
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SyncDriversLocked()
{
    std::erase_if (m_drivers, [this] (const std::pair<const std::string, DriverState> & entry)
    {
        return FindDeviceLocked (entry.second.unit) == nullptr;
    });

    for (const ControllerDeviceInfo & device : m_devices)
    {
        std::string  token            = ControllerTokens::UnitToToken (device.unit);
        auto         [found, isAdded] = m_drivers.try_emplace (token);
        bool         wasDriving       = found->second.isDriving;

        if (isAdded)
        {
            found->second.unit = device.unit;
        }

        found->second.isDriving = GetDriverRouteLocked (device.unit).has_value();

        if (found->second.isDriving && !wasDriving)
        {
            m_rateResetTokens.push_back (token);
        }

        if (!found->second.isDriving)
        {
            found->second.logical.reset();
        }

        ResolveMappingLocked (found->second);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMappingLocked
//
//  Gives an attached controller the mapping it plays with. An empty mapping
//  reads every control as unbound, so a controller without one sits at
//  center with its buttons up no matter what the user does with it -- which
//  is why this runs from the refresh, the setters and the tick alike.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ResolveMappingLocked (DriverState & driver)
{
    const ControllerDeviceInfo  * device = FindDeviceLocked (driver.unit);



    if (device == nullptr || driver.isResolved)
    {
        return;
    }

    ResolveUnitLocked (*device, driver.mapping, driver.deadzone);
    driver.isResolved = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveUnitLocked
//
//  One controller's mapping and deadzone for its active profile. THIS
//  CONTROLLER'S profile, not the machine's: two players on two pads of one
//  model can each play their own. A choice of the other mode's profile, which
//  only the prefs can hold, is no choice.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ResolveUnitLocked (
    const ControllerDeviceInfo  & device,
    ControlMapping              & outMapping,
    float                       & outDeadzone) const
{
    const ControllerProfile  * profile    = nullptr;
    std::string                active     = GetActiveProfileLocked (device.unit);
    ControllerProfileKind      kind       = ControllerModelSettings::GetAutomaticKind (m_profileMode);
    ControllerProfileKind      chosenKind = ControllerProfileKind::User;



    if (IsOfOtherModeLocked (device.unit.model, active, m_profileMode))
    {
        active.clear();
    }

    // A built-in profile chosen by name plays even for a model with nothing
    // saved, which has no profile of that name to find.
    chosenKind = ControllerModelSettings::GetBuiltInKind (active);

    if (chosenKind != ControllerProfileKind::User)
    {
        kind = chosenKind;
        active.clear();
    }

    // The deadzone belongs to the model, whichever profile is active. With no
    // profile chosen for this mode, or one the model no longer has, the
    // controller plays the Joyport profile while a Joyport is attached and
    // the Default otherwise.
    m_profiles.GetBuiltInSettings (kind, device.unit.model, device.formFactor, device.controls, outMapping, outDeadzone);

    if (active.empty())
    {
        return;
    }

    // A remembered profile the model no longer has plays the built-in one
    // already in hand; nothing is recreated for it (FR-029).
    profile = m_profiles.FindProfile (ControllerTokens::ModelToToken (device.unit.model), active);

    if (profile != nullptr)
    {
        outMapping = profile->mapping;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnresolveDriversLocked
//
//  Every controller's mapping is looked up again, and what it held is
//  dropped: the settings or the profile it was resolved from changed.
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
//  Every playing controller's last reading, placed where its route puts it:
//  its own PDL0, PDL1 and on land on the paddles it drives, in order, and its
//  button bindings on the lines its target is wired to. A paddle or a line
//  it does not drive is left absent, for the other player or for rest.
//
////////////////////////////////////////////////////////////////////////////////

GamePortContribution ControllerInputService::BuildMergedLocked() const
{
    GamePortContribution  merged;
    size_t                player       = 0;
    size_t                i            = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        std::optional<PlayerTargetRules::Route>  route   = PlayerSlotPolicy::GetDriverRoute (m_slots, m_entries, player, m_axisCount);
        auto                                     found   = m_drivers.end();
        const GamePortContribution             * logical = nullptr;

        if (!route.has_value())
        {
            continue;
        }

        found = m_drivers.find (ControllerTokens::UnitToToken (m_slots[player].holder.value()));

        if (found == m_drivers.end() || !found->second.logical.has_value())
        {
            continue;
        }

        logical = &found->second.logical.value();

        for (i = 0; i < route->paddles.size() && i < logical->paddle.size(); i++)
        {
            if (route->paddles[i].has_value() && route->paddles[i].value() < merged.paddle.size())
            {
                merged.paddle[route->paddles[i].value()] = logical->paddle[i];
            }
        }

        for (i = 0; i < route->buttons.size(); i++)
        {
            if (route->buttons[i].has_value() && logical->buttons.test (i))
            {
                merged.buttons.set (route->buttons[i].value());
            }
        }
    }

    AddJoyportSwitchesLocked (merged);

    return merged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddJoyportSwitchesLocked
//
//  Each Joyport jack carries the switches of the player JoyportJackRules
//  gives it: one player driving alone is on both jacks, two split them, and
//  a jack held for a player who left reads open. What paddles a slot maps to
//  plays no part. A jack given to Player 1 on the arrow keys is marked for
//  the mixer, which reads the keys.
//
//  THE JACKS ARE LEFT UNSET when no controller's reading reaches them and
//  the keys are not split from anything, so the controllers release as they
//  always have and the keys alone keep both jacks.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::AddJoyportSwitchesLocked (GamePortContribution & merged) const
{
    JoyportJackRules::JackSources          sources   = JoyportJackRules::AssignJacks (JoyportJackRules::ReducePlayers (m_slots, m_entries));
    bool                                   isKeys    = m_entries[0].kind == PlayerEntryKind::ArrowKeys;
    JoyportJacks                           jacks;
    std::bitset<JoyportJacks::kJackCount>  keyJacks;
    bool                                   isRead    = false;
    size_t                                 jack      = 0;



    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        size_t  player = (sources[jack] == JoyportJackSource::Player1) ? 0 : 1;
        auto    found  = m_drivers.end();

        if (sources[jack] == JoyportJackSource::None)
        {
            continue;
        }

        if (player == 0 && isKeys)
        {
            keyJacks.set (jack);
            continue;
        }

        if (!m_slots[player].holder.has_value())
        {
            continue;
        }

        found = m_drivers.find (ControllerTokens::UnitToToken (m_slots[player].holder.value()));

        if (found != m_drivers.end() && found->second.logical.has_value())
        {
            jacks.jack[jack] = found->second.logical->switches;
            isRead           = true;
        }
    }

    if (isRead || (keyJacks.any() && !keyJacks.all()))
    {
        merged.jacks    = jacks;
        merged.keyJacks = keyJacks;
    }
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
