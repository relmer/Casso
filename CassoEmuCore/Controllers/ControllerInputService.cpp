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

    // The mapping belongs to the controller that was chosen, so a different
    // controller starts from its own defaults rather than inheriting the last
    // one's bindings.
    m_mapping              = ControlMapping();
    EnsureMappingForActiveLocked();

    // A saved controller restored for a machine may not be attached. The
    // policy is what replaces it, so the next tick has to run it; a pick
    // from the picker is always attached and needs no scan.
    if (selection.has_value() && FindDeviceLocked (selection.value()) == nullptr)
    {
        m_devicesDirty = true;
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

        active       = m_selection;
        mapping      = m_mapping;
        deadzone     = m_deadzone;
        isActive     = m_isActive;
        wasConnected = m_isSelectedConnected;
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_lastTick                = TickReport();
        m_lastTick.hasSelection   = active.has_value();
        m_lastTick.isActiveXInput = active.has_value()
                                    && active.value().model.kind == ControllerKind::XInput;
        m_lastTick.hasMapping     = (m_mapping != ControlMapping());
        m_lastTick.isAppActive    = m_isActive;
        m_lastTick.deadzone       = m_deadzone;
    }

    if (!active.has_value())
    {
        return wait;
    }

    hr          = m_backend.ReadSample (active.value(), sample);
    isConnected = SUCCEEDED (hr) && sample.connected;

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_lastTick.readResult  = hr;
        m_lastTick.isConnected = isConnected;
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_isSelectedConnected = isConnected;
        m_lastSample          = isConnected ? sample : ControllerSample();
        onStateChanged        = m_onStateChanged;
    }

    // Who owns the axes turns on whether the selected controller reads, so
    // the first successful read after it is chosen has to be announced: until
    // then the axes rest at center however far the stick is pushed.
    if (isConnected != wasConnected && onStateChanged)
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
        GamePortContribution  contribution = m_evaluator.Evaluate (sample, mapping, deadzone);

        m_mixer.Submit (GamePortSource::Controller, contribution);
        m_hasContribution = true;

        {
            std::lock_guard<std::mutex>  lock (m_mutex);

            m_lastTick.didSubmit = true;
            m_lastTick.submitted = contribution;
        }
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



    snapshot.devices             = m_devices;
    snapshot.selection           = m_selection;
    snapshot.lastSample          = m_lastSample;
    snapshot.isSelectedConnected = m_isSelectedConnected;

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

        byAttachOrder = m_devices;
        std::stable_sort (byAttachOrder.begin(), byAttachOrder.end(),
            [this] (const ControllerDeviceInfo & a, const ControllerDeviceInfo & b)
            {
                return GetAttachOrderLocked (a.unit) < GetAttachOrderLocked (b.unit);
            });

        decision = ControllerSelectionPolicy::Evaluate (m_selection, byAttachOrder, m_hasGamePort);

        if (decision.reason == SelectionChangeReason::Cleared && !wasSelectionAttached)
        {
            decision.isAnnounced = false;
        }

        decision.departedDescription = departedDescription;

        if (decision.hasChanged)
        {
            // The mapping belongs to the controller it was made for, so the
            // next one starts from its own defaults.
            m_selection           = decision.selection;
            m_mapping             = ControlMapping();
            m_lastSample          = ControllerSample();
            m_isSelectedConnected = false;
        }

        EnsureMappingForActiveLocked();
        onSelectionChanged = m_onSelectionChanged;
        onStateChanged     = m_onStateChanged;
    }

    if (decision.hasChanged)
    {
        ReleaseContribution();
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
    if (!m_selection.has_value())
    {
        return nullptr;
    }

    return FindDeviceLocked (m_selection.value());
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
