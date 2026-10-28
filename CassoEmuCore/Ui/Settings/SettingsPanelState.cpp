#include "Pch.h"

#include "SettingsPanelState.h"
#include "Ui/Settings/SettingsJson.h"


#include "Config/MonitorCatalog.h"
#include "Controllers/ControllerTokens.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Machines/MachineDefinitions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanelState::SettingsPanelState()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromMachine
//
//  Reload the snapshot from a freshly-merged machine config.
//  `mergedJson` is the result of `UserConfigStore::Load` (or
//  equivalently `MergeJson (defaultJson, userJson)`). `defaultJson`
//  is the unmerged embedded default for the machine; it is kept so
//  `Apply` can diff against it for `SaveDelta`. Both must be JSON
//  objects -- anything else returns E_INVALIDARG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::LoadFromMachine (
    const std::string  & machineName,
    const JsonValue    & defaultJson,
    const JsonValue    & mergedJson)
{
    HRESULT   hr              = S_OK;
    JsonType  defaultRootType = JsonType::Null;
    JsonType  mergedRootType  = JsonType::Null;



    defaultRootType = defaultJson.GetType();
    mergedRootType  = mergedJson.GetType();

    CBR (defaultRootType == JsonType::Object);
    CBR (mergedRootType  == JsonType::Object);

    m_machineName = machineName;
    m_defaultJson = SettingsJson::CloneJson (defaultJson);
    m_mergedJson  = SettingsJson::CloneJson (mergedJson);

    //
    //  A shipped machine's document no longer lists its internal devices --
    //  the machine states those in code. The hardware tree still has to show
    //  them, so the definition's list is put into the working copy here, once,
    //  rather than every reader learning where a device list comes from.
    //
    //  A machine with no definition is left alone: its document IS the only
    //  description of it.
    //
    SettingsJson::AddDefinedDevices (m_machineName, m_mergedJson);
    SettingsJson::AddDefinedDevices (m_machineName, m_defaultJson);

    m_original = Snapshot {};

    //  Read from the augmented copy, not the caller's: the device list the
    //  hardware tree needs was just put there.
    hr = SettingsJson::ExtractUiPrefs (m_mergedJson, m_original.prefs);
    CHR (hr);

    hr = SettingsJson::ExtractMachineInfo (machineName, m_mergedJson, m_machineInfo);
    CHR (hr);

    hr = SettingsJson::ExtractHardware (m_mergedJson, m_original.hardware);
    CHR (hr);

    hr = SettingsJson::ExtractMachinePorts (mergedJson, m_original.machinePorts);
    CHR (hr);

    // The back-panel disk port outranks the legacy externalDriveConnected
    // boolean ExtractUiPrefs just read. Both can be on disk at once -- the
    // fold that retires the boolean only runs on a version bump -- and the
    // port is the one Settings writes from here on.
    //
    // A MACHINE WITH A DISK ][ CARD KEEPS ITS SECOND DRIVE ON THE CARD, and
    // the flag has to be read from there too. Left at the legacy boolean, it
    // is false for every carded machine that never had one -- and Apply
    // pushes the flag to the shell on every OK, which detached the card's
    // second drive and ejected its disk whatever tab the user had been on.
    // SecondDriveAttached reads whichever store the machine keeps, so it
    // answers both cases; a machine with neither keeps the boolean.
    m_current = m_original;

    if (HasSecondDriveStore())
    {
        m_original.prefs.externalDriveConnected = SecondDriveAttached();
    }

    m_current = m_original;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Reset `current` to match `original` (used by the Cancel button
//  and implicitly by `LoadFromMachine`).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::Cancel()
{
    m_current = m_original;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::BuildCurrentJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue SettingsPanelState::BuildCurrentJson() const
{
    return SettingsJson::BuildJson (m_mergedJson, m_current.hardware, m_current.prefs, m_current.machinePorts);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::IsDirty() const
{
    return !SettingsJson::ArePrefsEqual    (m_original.prefs,    m_current.prefs)
        || !SettingsJson::AreHardwareEqual (m_original.hardware, m_current.hardware)
        || AreSerialPortsChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequiresReset
//
//  A reset is required iff the hardware-enable bits changed.
//  Speed / color / sound / mechanism / write-protect, every serial
//  connector's attachment, the ImageWriter II's connector and the Uthernet
//  II's settings are all live-applicable.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::RequiresReset() const
{
    size_t  i       = 0;
    bool    changed = m_original.hardware.size() != m_current.hardware.size();



    for (i = 0; !changed && i < m_current.hardware.size(); ++i)
    {
        changed = m_original.hardware[i].enabled != m_current.hardware[i].enabled;
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mutators
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetSpeedMode (SettingsSpeedMode mode)
{
    m_current.prefs.speedMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColorMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetColorMode (SettingsColorMode mode)
{
    m_current.prefs.colorMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetWriteMode (SettingsWriteMode mode)
{
    m_current.prefs.writeMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppySound
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetFloppySound (bool enabled)
{
    m_current.prefs.floppySoundEnabled = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFastTapeLoading
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetFastTapeLoading (bool enabled)
{
    m_current.prefs.fastTapeLoading = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTapeVolume
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetTapeVolume (float gain)
{
    m_current.prefs.tapeVolume = gain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTapeAutoStop
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetTapeAutoStop (bool enabled)
{
    m_current.prefs.tapeAutoStop = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTapeIdleStop
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetTapeIdleStop (bool enabled)
{
    m_current.prefs.tapeIdleStop = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTapeEightBit
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetTapeEightBit (bool enabled)
{
    m_current.prefs.tapeEightBit = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMechanism
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetMechanism (const std::string & mechanism)
{
    m_current.prefs.floppyMechanism = mechanism;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveMotorVolume / SetDriveHeadVolume / SetDriveDoorVolume
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetDriveMotorVolume (float gain)
{
    m_current.prefs.driveMotorVolume = gain;
}





void SettingsPanelState::SetDriveHeadVolume (float gain)
{
    m_current.prefs.driveHeadVolume = gain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveDoorVolume
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetDriveDoorVolume (float gain)
{
    m_current.prefs.driveDoorVolume = gain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveOnePan / SetDriveTwoPan
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetDriveOnePan (float pan)
{
    m_current.prefs.driveOnePan = pan;
}





void SettingsPanelState::SetDriveTwoPan (float pan)
{
    m_current.prefs.driveTwoPan = pan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetWriteProtect (int drive, bool wp)
{
    if (drive < 0 || drive >= 2)
    {
        return;
    }

    m_current.prefs.writeProtect[drive] = wp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetExternalDriveConnected
//
//  //c external-drive port toggle. A live-effect setting -- it only
//  reveals/hides the second drive-mount widget, so unlike a hardware
//  enable it never sets RequiresReset.
//
//  The staged boolean drives the UI and the live apply; the staged PORT is
//  what gets written to disk. Both move together here so there is exactly one
//  place where "the external drive is attached" is decided, which is the
//  whole point of retiring the standalone boolean.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetExternalDriveConnected (bool connected)
{
    m_current.prefs.externalDriveConnected = connected;

    for (SettingsMachinePort & port : m_current.machinePorts)
    {
        if (port.name == kpszDiskPortName)
        {
            port.device = connected ? kpszDiskIicDrive : "";
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSecondDriveStore
//
//  Whether the machine keeps a second drive anywhere Settings can read it:
//  a back-panel disk port, or an enabled Disk ][ card. SecondDriveAttached
//  below reads it.
//
//  The machine's second drive is read from whichever store that machine keeps
//  it in. A //c has a back-panel disk port; everything else has a Disk ][
//  card whose second connector is the drive in question.
//
//  A card that declares NO ports has not been described rather than emptied,
//  so it reads as attached -- the two drives it has always behaved as having.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::HasSecondDriveStore() const
{
    for (const SettingsMachinePort & port : m_current.machinePorts)
    {
        if (port.name == kpszDiskPortName)
        {
            return true;
        }
    }

    for (const HardwareEntry & entry : m_current.hardware)
    {
        if (entry.kind == HardwareEntryKind::Slot && entry.enabled && entry.type == kpszDiskIiDevice)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SecondDriveAttached
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::SecondDriveAttached() const
{
    for (const SettingsMachinePort & port : m_current.machinePorts)
    {
        if (port.name == kpszDiskPortName)
        {
            return !port.device.empty();
        }
    }

    for (const HardwareEntry & entry : m_current.hardware)
    {
        if (entry.kind != HardwareEntryKind::Slot ||
            !entry.enabled || entry.type != kpszDiskIiDevice)
        {
            continue;
        }

        if (entry.ports.size() < kSecondDrivePortIndex + 1)
        {
            return true;
        }

        return !entry.ports[kSecondDrivePortIndex].empty();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSecondDriveAttached
//
//  The write side of SecondDriveAttached, routed to the same store it reads.
//
//  MATERIALIZES BOTH CONNECTORS on a card that declared none. A one-element
//  port list would take the FIRST drive away as a side effect of detaching
//  the second, which is the kind of bug that looks like the model is wrong
//  rather than the config.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetSecondDriveAttached (bool attached)
{
    for (SettingsMachinePort & port : m_current.machinePorts)
    {
        if (port.name == kpszDiskPortName)
        {
            SetExternalDriveConnected (attached);
            return;
        }
    }

    for (HardwareEntry & entry : m_current.hardware)
    {
        if (entry.kind != HardwareEntryKind::Slot ||
            !entry.enabled || entry.type != kpszDiskIiDevice)
        {
            continue;
        }

        while (entry.ports.size() < kSecondDrivePortIndex + 1)
        {
            entry.ports.push_back (kpszDiskIiDrive);
        }

        entry.ports[kSecondDrivePortIndex] = attached ? kpszDiskIiDrive : "";

        // Keep the live-apply path fed: the sink pushes this on Apply and the
        // shell turns it into the same chrome relayout either machine needs.
        m_current.prefs.externalDriveConnected = attached;
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseConnected
//
//  //c mouse-port toggle. Live UI pref: never sets RequiresReset.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetMouseConnected (bool connected)
{
    m_current.prefs.mouseConnected = connected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHardwareEnabled
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::SetHardwareEnabled (size_t index, bool enabled)
{
    HRESULT            hr            = S_OK;
    size_t             hardwareCount = 0;
    PrinterConnection  printer;
    bool               hadPrinter    = false;



    hardwareCount = m_current.hardware.size();
    CBR (index < hardwareCount);

    if (! enabled)
    {
        // FR-007 / FR-008: required and platform-locked entries
        // cannot be turned off. User input -- non-asserting.
        CBR (m_current.hardware[index].capability == CapabilityFlag::Optional);
    }

    //  Disconnect the ImageWriter II from a card before turning the card
    //  off, so a Super Serial Card's mode is restored, and report the change
    //  for the Printing tab.
    printer    = m_connectors.FindPrinterConnection (m_current.hardware, m_current.machinePorts);
    hadPrinter = !enabled
              && m_current.hardware[index].kind == HardwareEntryKind::Slot
              && printer.number == m_current.hardware[index].slot
              && (printer.kind == PrinterConnection::Kind::ParallelCard || printer.kind == PrinterConnection::Kind::SerialCard);

    if (hadPrinter)
    {
        SetPrinterConnection (PrinterConnection());

        m_connectors.m_notice = std::format (L"The ImageWriter II has been disconnected from the {} in slot {} because the card is turned off. "
                                L"You can reconnect it on the Printing tab.",
                                printer.kind == PrinterConnection::Kind::ParallelCard ? L"parallel card" : L"Super Serial Card", printer.number);
    }

    m_current.hardware[index].enabled = enabled;

    // A network card turned on with no MAC address gets one, as a card
    // comes with an address on its label.
    if (enabled
        && m_current.hardware[index].type == kpszNetworkCardDevice
        && !m_current.hardware[index].network.mac.IsSet())
    {
        m_current.hardware[index].network.mac = m_connectors.MakeRandomMac();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshMergedJson
//
//  Replaces the document BuildJson roots the save on. The caller re-reads it
//  from the store just before applying, so a key written behind this modeless
//  sheet's back -- the per-machine input mapping, a //c case switch, a disk
//  mounted while it was open -- is carried forward as it stands on disk rather
//  than as it stood when the sheet opened.
//
//  The staged edits live in m_current and the baselines in m_original, so
//  re-pointing this changes what the save carries forward, not what the sheet
//  believes the user chose.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::RefreshMergedJson (const JsonValue & mergedJson)
{
    std::vector<SettingsMachinePort>  ports;
    std::vector<HardwareEntry>        hardware;
    JsonValue                         augmented;
    HRESULT                           hr        = S_OK;



    m_mergedJson = SettingsJson::CloneJson (mergedJson);

    // The second drive is taken from disk too. No page edits it -- it is
    // connected from the Storage menu and the drives' right-click menus,
    // which save it the moment it changes -- so the copy read when the sheet
    // opened is the stale one, and BuildJson writes it out on OK. The serial
    // ports ARE edited here, on the Hardware page, so a staged change to one
    // is put back on top of the list read from disk.
    hr = SettingsJson::ExtractMachinePorts (mergedJson, ports);

    if (SUCCEEDED (hr))
    {
        std::array<std::string, std::size (kpszSerialPortNames)>  staged;
        std::array<bool, std::size (kpszSerialPortNames)>         changed = {};

        for (size_t i = 0; i < std::size (kpszSerialPortNames); i++)
        {
            staged[i]  = GetSerialPortDevice ((int) i + 1);
            changed[i] = staged[i] != SettingsJson::GetPortDevice (m_original.machinePorts, kpszSerialPortNames[i]);
        }

        m_current.machinePorts  = ports;
        m_original.machinePorts = ports;

        for (size_t i = 0; i < std::size (kpszSerialPortNames); i++)
        {
            if (changed[i])
            {
                SetSerialPortDevice ((int) i + 1, staged[i]);
            }
        }
    }

    augmented = SettingsJson::CloneJson (mergedJson);
    SettingsJson::AddDefinedDevices (m_machineName, augmented);
    hr = SettingsJson::ExtractHardware (augmented, hardware);

    // A card's connectors are taken from disk too, unless a page in this sheet
    // changed them: the Super Serial Card's connector is chosen on its page,
    // and the copy on disk is the one the user just replaced.
    for (size_t i = 0; SUCCEEDED (hr) && i < hardware.size() && i < m_current.hardware.size(); i++)
    {
        HardwareEntry &  current   = m_current.hardware[i];
        HardwareEntry &  original  = m_original.hardware[i];
        bool             sameCard  = hardware[i].type == current.type && hardware[i].slot == current.slot;
        bool             unchanged = current.ports == original.ports && current.portPath == original.portPath;

        if (sameCard && unchanged)
        {
            current.ports     = hardware[i].ports;
            current.portPath  = hardware[i].portPath;
            original.ports    = hardware[i].ports;
            original.portPath = hardware[i].portPath;
        }
    }

    // The recorder likewise: connected from the Storage menu, never a page.
    {
        SettingsUiPrefs  fresh;

        hr = SettingsJson::ExtractUiPrefs (mergedJson, fresh);

        if (SUCCEEDED (hr))
        {
            m_current.prefs.tapeRecorderConnected  = fresh.tapeRecorderConnected;
            m_original.prefs.tapeRecorderConnected = fresh.tapeRecorderConnected;
        }
    }

    if (HasSecondDriveStore())
    {
        m_current.prefs.externalDriveConnected  = SecondDriveAttached();
        m_original.prefs.externalDriveConnected = m_current.prefs.externalDriveConnected;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apply
//
//  Pushes the live-applicable diffs through `sink` and emits the
//  updated machine JSON ready for `UserConfigStore::SaveDelta`.
//  Always emits `outCurrentJson` (even when nothing changed) so
//  callers can drop the result straight into SaveDelta.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::Apply (
    ISettingsApplySink & sink,
    JsonValue          & outCurrentJson) const
{
    HRESULT  hr      = S_OK;
    int      i       = 0;
    int      network = -1;



    // Live-effect fields (FR-011 -- always pushed; cheap, idempotent).
    sink.ApplySpeedMode   (m_current.prefs.speedMode);
    sink.ApplyColorMode   (m_current.prefs.colorMode);
    sink.ApplyFloppySound (m_current.prefs.floppySoundEnabled);
    sink.ApplyMechanism   (m_current.prefs.floppyMechanism);
    sink.ApplyFastTapeLoading (m_current.prefs.fastTapeLoading);
    sink.ApplyTapeVolume      (m_current.prefs.tapeVolume);
    sink.ApplyTapeAutoStop    (m_current.prefs.tapeAutoStop);
    sink.ApplyTapeIdleStop    (m_current.prefs.tapeIdleStop);
    sink.ApplyTapeEightBit    (m_current.prefs.tapeEightBit);
    sink.ApplyDriveVolumes (m_current.prefs.driveMotorVolume,
                            m_current.prefs.driveHeadVolume,
                            m_current.prefs.driveDoorVolume);
    sink.ApplyDrivePan     (m_current.prefs.driveOnePan,
                            m_current.prefs.driveTwoPan);
    for (i = 0; i < 2; ++i)
    {
        sink.ApplyWriteProtect (i, m_current.prefs.writeProtect[i]);
    }

    sink.ApplyExternalDriveConnected (m_current.prefs.externalDriveConnected);
    sink.ApplyMouseConnected (m_current.prefs.mouseConnected);

    // A connector is reconnected only when its device changed, since
    // reconnecting drops whatever is connected to it. A reset rebuilds them
    // all from the saved configuration instead.
    if (!RequiresReset())
    {
        m_connectors.ApplyConnectors (sink);
    }

    // The Super Serial Card's switches take effect without a reset: they are
    // register reads. Pushed whenever the machine lists the card turned on.
    if (HasEnabledSerialCard())
    {
        const HardwareEntry &  card = m_current.hardware[static_cast<size_t> (FindSerialCard())];

        if (!card.switchesSw1.empty())
        {
            sink.ApplySerialCardSwitches (card.switchesSw1, card.switchesSw2);
        }
    }

    // The Uthernet II's LAN access and port forwards apply to the running
    // card. Turning the card on or off takes the reset below.
    network = m_connectors.FindNetworkCard();

    if (network >= 0 && !RequiresReset()
        && m_current.hardware[(size_t) network].enabled
        && !(m_original.hardware[(size_t) network].network == m_current.hardware[(size_t) network].network))
    {
        sink.ApplyNetworkCard (m_current.hardware[(size_t) network].network);
    }

    // FR-010: any hardware enable diff requires the caller to confirm
    // and the machine to be reset. Queue the reset request; the
    // production sink (EmulatorShell) gates it behind a modal confirm.
    if (RequiresReset())
    {
        sink.QueueMachineReset();
    }

    outCurrentJson = SettingsJson::BuildJson (m_mergedJson, m_current.hardware, m_current.prefs,
                                m_current.machinePorts);

    return hr;
}
