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

    m_selection           = selection;
    m_lastSample          = ControllerSample();
    m_isSelectedConnected = false;

    // The mapping belongs to the controller that was chosen, so a different
    // controller starts from its own defaults rather than inheriting the last
    // one's bindings.
    m_mapping             = ControlMapping();
    EnsureMappingForSelectionLocked();
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
    HRESULT                           hr             = S_OK;
    std::optional<ControllerUnitKey>  selection;
    ControllerSample                  sample;
    ControlMapping                    mapping;
    ControllerWaitSources             wait;
    float                             deadzone       = 0.0f;
    bool                              isActive       = false;
    bool                              wasConnected   = false;
    bool                              isConnected    = false;
    bool                              needsTimedPoll = false;



    if (m_devicesDirty.exchange (false))
    {
        RefreshDevices();
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        selection    = m_selection;
        mapping      = m_mapping;
        deadzone     = m_deadzone;
        isActive     = m_isActive;
        wasConnected = m_isSelectedConnected;
    }

    if (!selection.has_value())
    {
        // Temporary for the first playable slice: with nothing chosen, the
        // first controller found drives the game port, so a controller plays
        // as soon as it is plugged in and before any UI exists to choose it.
        // The selection policy replaces this, including remembering the
        // choice per machine.
        {
            std::lock_guard<std::mutex>  lock (m_mutex);

            if (!m_devices.empty())
            {
                m_selection = m_devices.front().unit;
                selection   = m_selection;
                EnsureMappingForSelectionLocked();
                mapping     = m_mapping;
                deadzone    = m_deadzone;
            }
        }

        if (!selection.has_value())
        {
            return wait;
        }
    }

    hr          = m_backend.ReadSample (selection.value(), sample);
    isConnected = SUCCEEDED (hr) && sample.connected;

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_isSelectedConnected = isConnected;
        m_lastSample          = isConnected ? sample : ControllerSample();
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
        // Temporary for the first playable slice: a live controller takes the
        // axes while it is driving them, and hands them back when it stops.
        // The selection policy replaces this with the arrow-key fallback.
        m_mixer.SetAxisOwner (AxisOwner::Controller);
        m_mixer.Submit (GamePortSource::Controller, m_evaluator.Evaluate (sample, mapping, deadzone));
        m_hasContribution = true;
    }

    UNREFERENCED_PARAMETER (wasConnected);

    // What the thread waits on until the next read: the controller's own
    // change events where it has them, and the measured poll period only for
    // the ones that have none.
    m_backend.GetWakeSources (selection.value(), wait.events, needsTimedPoll);

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
//  RefreshDevices
//
//  Re-reads what is attached. The selected controller keeps its mapping while
//  it is present; a selection whose controller is absent is left alone, since
//  a controller that comes back is the same one the user chose.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::RefreshDevices()
{
    HRESULT                            hr = S_OK;
    std::vector<ControllerDeviceInfo>  devices;



    hr = m_backend.EnumerateDevices (devices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_devices = devices;
        EnsureMappingForSelectionLocked();
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
//  EnsureMappingForSelection / EnsureMappingForSelectionLocked
//
//  Gives the selected controller the mapping it plays with. An empty mapping
//  reads every control as unbound, so a controller without one sits at center
//  with its buttons up no matter what the user does with it -- which is why
//  this runs from both the refresh and the tick, rather than only from the
//  refresh: the first refresh happens before any controller is selected.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerInputService::EnsureMappingForSelection()
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    EnsureMappingForSelectionLocked();
}


void ControllerInputService::EnsureMappingForSelectionLocked()
{
    const ControllerDeviceInfo *  selected = FindSelectedDeviceLocked();



    if (selected == nullptr || m_mapping != ControlMapping())
    {
        return;
    }

    m_mapping  = DefaultMapping::For (selected->unit.model, selected->controls);
    m_deadzone = DeadzoneShaper::GetDefaultDeadzone (selected->unit.model.kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSelectedDeviceLocked
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerInputService::FindSelectedDeviceLocked() const
{
    if (!m_selection.has_value())
    {
        return nullptr;
    }

    for (const ControllerDeviceInfo & device : m_devices)
    {
        if (device.unit == m_selection.value())
        {
            return &device;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSelectedDevicePresent
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerInputService::IsSelectedDevicePresent() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return FindSelectedDeviceLocked() != nullptr;
}
