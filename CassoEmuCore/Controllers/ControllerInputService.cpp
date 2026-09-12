#include "Pch.h"

#include "Controllers/ControllerInputService.h"

#include "Controllers/DeadzoneShaper.h"





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
//  Casso releases the controller's contribution, so a button held as the user
//  switches away does not stay down in the guest.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetActive (bool isActive)
{
    std::unique_lock<std::mutex>  lock (m_mutex);
    bool                          wasActive = m_isActive;



    m_isActive = isActive;
    lock.unlock();

    if (wasActive && !isActive)
    {
        ReleaseContribution();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelection
//
//  Which controller drives the game port, or none.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::SetSelection (const std::optional<ControllerUnitKey> & selection)
{
    std::unique_lock<std::mutex>  lock       (m_mutex);
    bool                          hasChanged = m_selection != selection;



    if (!hasChanged)
    {
        return;
    }

    m_selection            = selection;
    m_lastSample           = ControllerSample();
    m_isSelectedConnected  = false;
    m_standIn              = std::nullopt;
    m_selectionDescription = std::wstring();

    // The mapping belongs to the controller that was chosen, so a different
    // controller starts from its own defaults rather than inheriting the last
    // one's bindings.
    m_mapping              = ControlMapping();
    UpdateActiveUnitLocked();
    EnsureMappingForActiveLocked();
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
//  Tick
//
//  Controller thread. Reads the selected controller once and submits what it
//  asks of the game port, then says how long to wait for the next wake: the
//  measured poll period while a controller that must be polled is selected,
//  and otherwise nothing at all, since a DirectInput device wakes the thread
//  itself and an empty selection has nothing to read.
//
////////////////////////////////////////////////////////////////////////////////

ControllerWaitSources ControllerInputService::Tick()
{
    HRESULT                           hr                = S_OK;
    std::optional<ControllerUnitKey>  active;
    bool                              isSelectionActive = false;
    ControllerSample                  sample;
    ControlMapping                    mapping;
    ControllerWaitSources             wait;
    float                             deadzone          = 0.0f;
    bool                              isActive          = false;
    bool                              wasConnected      = false;
    bool                              isConnected       = false;
    bool                              needsTimedPoll    = false;
    StateChangedFn                    onStateChanged;



    if (m_devicesDirty.exchange (false))
    {
        RefreshDevices();
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        active            = m_activeUnit;
        isSelectionActive = m_activeUnit.has_value() && m_activeUnit == m_selection;
        mapping           = m_mapping;
        deadzone          = m_deadzone;
        isActive          = m_isActive;
        wasConnected      = m_isSelectedConnected;
    }

    if (!active.has_value())
    {
        return wait;
    }

    hr          = m_backend.ReadSample (active.value(), sample);
    isConnected = SUCCEEDED (hr) && sample.connected;

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        // A stand-in reading fine does not make the CHOSEN controller
        // connected: the picker and the prefs both still point at the one
        // the user asked for, and it is not there.
        m_isSelectedConnected = isConnected && isSelectionActive;
        m_lastSample          = isConnected ? sample : ControllerSample();
        onStateChanged        = m_onStateChanged;
    }

    // Who owns the axes turns on whether the chosen controller is there, so
    // the first successful read after it is chosen has to be announced: until
    // then the axes rest at center however far the stick is pushed.
    if ((isConnected && isSelectionActive) != wasConnected && onStateChanged)
    {
        onStateChanged();
    }

    if (!isConnected)
    {
        // A controller that could not be read is gone, not resting. Nothing
        // to wait on either: the next device notification is what brings it
        // back, and that arrives as a window message.
        ReleaseContribution();

        return wait;
    }

    if (!isActive)
    {
        ReleaseContribution();
    }
    else
    {
        m_mixer.Submit (GamePortSource::Controller, m_evaluator.Evaluate (sample, mapping, deadzone));
        m_hasContribution = true;
    }

    // What the thread waits on until the next read: the controller's own
    // change events where it has them, and the measured poll period only for
    // the ones that have none.
    m_backend.GetWakeSources (active.value(), wait.events, needsTimedPoll);

    if (needsTimedPoll)
    {
        wait.timeoutMs = kPollPeriodMs;
    }

    return wait;
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



    snapshot.devices              = m_devices;
    snapshot.selection            = m_selection;
    snapshot.standIn              = m_standIn;
    snapshot.selectionDescription = m_selectionDescription;
    snapshot.lastSample           = m_lastSample;
    snapshot.isSelectedConnected  = m_isSelectedConnected;

    if (m_standIn.has_value())
    {
        const ControllerDeviceInfo *  device = FindDeviceLocked (m_standIn.value());

        if (device != nullptr)
        {
            snapshot.standInDescription = device->description;
        }
    }

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshDevices
//
//  Re-reads what is attached. The selected controller keeps its mapping while
//  it is present; a selection whose controller is absent is left alone, since
//  a controller that comes back is the same one the user chose.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RefreshDevices()
{
    HRESULT                              hr                 = S_OK;
    std::vector<ControllerDeviceInfo>    devices;
    ControllerSelectionPolicy::Decision  decision;
    SelectionChangedFn                   onSelectionChanged;
    StateChangedFn                       onStateChanged;
    bool                                 hasActiveChanged   = false;



    hr = m_backend.EnumerateDevices (devices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_devices = devices;
        decision  = ControllerSelectionPolicy::Evaluate (m_selection, m_devices, m_hasGamePort);

        if (decision.hasChanged)
        {
            // A controller the policy chose brings its own mapping with it,
            // so the mapping the previous selection had is dropped first.
            m_selection            = decision.selection;
            m_mapping              = ControlMapping();
            m_lastSample           = ControllerSample();
            m_selectionDescription = std::wstring();
        }

        UpdateAttachOrderLocked();
        hasActiveChanged = UpdateActiveUnitLocked();

        if (hasActiveChanged)
        {
            // The controller being read changed, so the mapping the last one
            // played with goes with it: a stand-in has its own controls.
            m_mapping    = ControlMapping();
            m_lastSample = ControllerSample();
        }

        EnsureMappingForActiveLocked();
        onSelectionChanged = m_onSelectionChanged;
        onStateChanged     = m_onStateChanged;
    }

    // Outside the lock: the sink persists the choice and raises a notice, and
    // neither belongs under a lock the controller thread holds every tick.
    if (decision.hasChanged && onSelectionChanged)
    {
        onSelectionChanged (decision);
    }

    // A stand-in taking over or handing back changes what the picker says and
    // who owns the axes, and both of those are decided on the UI thread.
    if (hasActiveChanged && onStateChanged)
    {
        onStateChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseContribution
//
//  Returns the controller's axes to center and its buttons to released,
//  without disturbing what any other input source is holding.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::ReleaseContribution()
{
    if (!m_hasContribution)
    {
        return;
    }

    m_mixer.ReleaseSource (GamePortSource::Controller);
    m_hasContribution = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateAttachOrderLocked
//
//  Stamps each newly attached controller with a rising number and forgets the
//  ones that have gone. The numbers are what makes the stand-in the
//  longest-attached controller rather than whichever one enumeration happens
//  to list first (FR-008a).
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
//  UpdateActiveUnitLocked
//
//  Which controller is read. The chosen one whenever it is attached; while it
//  is not, the longest-attached other controller stands in for it (FR-008a).
//
//  With nothing to stand in, the active unit stays the CHOSEN one even though
//  it is not there. That read fails, and a failed read is the disconnect path
//  -- which is what returns the axes to center and releases the buttons. An
//  empty active unit would skip that and leave the paddles wherever the last
//  read put them.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::UpdateActiveUnitLocked()
{
    std::optional<ControllerUnitKey>  active  = m_selection;
    std::optional<ControllerUnitKey>  standIn;
    const ControllerDeviceInfo *      chosen  = nullptr;
    uint64_t                          longest = 0;



    if (m_selection.has_value())
    {
        chosen = FindDeviceLocked (m_selection.value());
    }

    if (chosen != nullptr)
    {
        m_selectionDescription = chosen->description;
    }
    else if (m_selection.has_value())
    {
        for (const std::pair<ControllerUnitKey, uint64_t> & entry : m_attachOrder)
        {
            if (!standIn.has_value() || entry.second < longest)
            {
                standIn = entry.first;
                longest = entry.second;
            }
        }

        if (standIn.has_value())
        {
            active = standIn;
        }
    }

    if (m_standIn == standIn && m_activeUnit == active)
    {
        return false;
    }

    m_standIn    = standIn;
    m_activeUnit = active;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureMappingForActive / EnsureMappingForActiveLocked
//
//  Gives the controller being read the mapping it plays with. An empty
//  mapping reads every control as unbound, so a controller without one sits
//  at center with its buttons up no matter what the user does with it --
//  which is why this runs from both the refresh and the tick, rather than
//  only from the refresh: the first refresh happens before any controller is
//  selected.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::EnsureMappingForActive()
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    EnsureMappingForActiveLocked();
}


void ControllerInputService::EnsureMappingForActiveLocked()
{
    const ControllerDeviceInfo *  active = FindActiveDeviceLocked();



    if (active == nullptr || m_mapping != ControlMapping())
    {
        return;
    }

    m_mapping  = DefaultMapping::For (active->unit.model, active->controls);
    m_deadzone = DeadzoneShaper::GetDefaultDeadzone (active->unit.model.kind);
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





////////////////////////////////////////////////////////////////////////////////
//
//  FindActiveDeviceLocked
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerInputService::FindActiveDeviceLocked() const
{
    if (!m_activeUnit.has_value())
    {
        return nullptr;
    }

    return FindDeviceLocked (m_activeUnit.value());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSelectedDevicePresent
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::IsSelectedDevicePresent() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return FindActiveDeviceLocked() != nullptr;
}
