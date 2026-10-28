#include "Pch.h"

#include "Shell/Components/ShellControllers.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/MachineHost.h"
#include "Config/GlobalUserPrefs.h"
#include "Shell/ShellDebugger.h"
#include "Shell/IInputHost.h"
#include "Core/JsonWriter.h"
#include "Machines/MachineDefinition.h"
#include "Shell/EmulatorShellInternal.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"
#include "resource.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::SyncJoyport
//
//  UI thread, after the players' modes may have changed: the running
//  machine's Joyport follows them on the next button read, with no reset.
//  The keys are resubmitted, since Player 1's keys in a jack close its
//  switches rather than drive the paddle inputs, and the Alt keys drop out
//  of the fire keys while the Joyport is on.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::SyncJoyport()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_input.m_machine.GetLifetimeLock());
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = m_input.m_machine.GetHostInputGate().TryEnter (gate);



    // Behind live the Joyport holds the recorded settings, which the guest
    // reads; the new ones reach it once the machine is live.
    if (isGateOpen)
    {
        ApplyJoyportToMachine();
    }
    else
    {
        m_input.m_debugger.OweJoyportSync();
    }

    gate.Leave();
    lifetime.unlock();

    if (m_input.m_arrowsJoystick)
    {
        m_input.UpdateJoystickAxesFromKeys();
        m_input.UpdateJoystickButtonsFromKeys();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::IsJoyportInEffect
//
//  Whether the running machine reads the Joyport: a player is in one of its
//  jacks, on a machine that has one. A machine builds a Joyport exactly when
//  it has the annunciators to drive one.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellControllers::IsJoyportInEffect() const
{
    PlayerEntries  entries;



    if (m_controllerService != nullptr)
    {
        entries = m_controllerService->GetPlayerEntries();
    }

    return PlayerModeRules::IsJoyportOn (entries, m_input.m_machine.GetJoyport() != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::IsPlayerOneOnJoyport
//
////////////////////////////////////////////////////////////////////////////////

bool ShellControllers::IsPlayerOneOnJoyport() const
{
    PlayerEntries  entries;



    if (m_controllerService != nullptr)
    {
        entries = m_controllerService->GetPlayerEntries();
    }

    return PlayerModeRules::IsOnJoyport (entries, 0, m_input.m_machine.GetJoyport() != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::IsJoyportOffered
//
//  Whether the running machine can take a Joyport, which is what the picker's
//  row and the Controllers page's switch are offered on: both apply at once
//  to the running machine, so a machine staged elsewhere has no say.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellControllers::IsJoyportOffered() const
{
    return m_input.m_machine.GetJoyport() != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::PickPlayer
//
//  UI thread. The user picked an entry in a player's submenu. A controller
//  takes effect at once, whether or not it has been used, and one the other
//  player picked sends that player back to Automatic (FR-041).
//
//  The keys and the mouse go through their own setters, which hand PDL0 and
//  PDL1 over and take the pointer, and record Player 1's entry through the
//  axis owner. Any other entry for Player 1 gives them up. The entry is set
//  FIRST, so the setters that give them up find Player 1 already on it and
//  do not pass through Automatic on the way. From the keys to the mouse, or
//  back, the setter turns the other off and syncs once with both changed, so
//  Player 1's entry is set once, straight to the new one. The entries are
//  saved with the global prefs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::PickPlayer (size_t player, PlayerEntry entry)
{
    bool  isPlayerOne = player == 0;



    if (isPlayerOne && entry.kind == PlayerEntryKind::ArrowKeys)
    {
        m_input.SetArrowsJoystick (true);
        return;
    }

    if (isPlayerOne && entry.kind == PlayerEntryKind::MousePaddle)
    {
        m_input.SetPointerMapping (InputMappingMode::Paddle);
        return;
    }

    PickPlayerEntry (player, entry);

    if (isPlayerOne && m_input.m_arrowsJoystick)
    {
        m_input.SetArrowsJoystick (false);
    }

    if (isPlayerOne && m_input.m_pointerMode == InputMappingMode::Paddle)
    {
        m_input.SetPointerMapping (InputMappingMode::Off);
    }

    SyncJoyport();
    m_input.SyncGamePortAxisOwner();
    m_input.SyncInputModeUi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::SetPlayerMode
//
//  UI thread. The mode is set FIRST, and the service returns Player 1 to
//  Automatic when the new mode cannot have the keys or the mouse; turning
//  those off afterwards then finds Player 1 already on Automatic and picks
//  nothing on the way. The machine's Joyport follows the new modes, and the
//  entries are saved with the global prefs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::SetPlayerMode (size_t player, PlayerMode mode)
{
    bool  isPlayerOne = player == 0;



    if (m_controllerService == nullptr)
    {
        return;
    }

    m_controllerService->SetPlayerMode (player, mode);

    if (isPlayerOne && PlayerModeRules::IsPaddleMode (mode) && m_input.m_arrowsJoystick)
    {
        m_input.SetArrowsJoystick (false);
    }

    if (isPlayerOne && !PlayerModeRules::IsPaddleMode (mode) && m_input.m_pointerMode == InputMappingMode::Paddle)
    {
        m_input.SetPointerMapping (InputMappingMode::Off);
    }

    SyncJoyport();
    m_input.SyncGamePortAxisOwner();
    m_input.SyncInputModeUi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::PickPlayerEntry
//
//  Hands one player's entry to the service and wakes its thread, so the
//  choice takes effect on the next read rather than at the end of whatever
//  wait the thread is in.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::PickPlayerEntry (size_t player, const PlayerEntry & entry)
{
    if (m_controllerService == nullptr)
    {
        return;
    }

    m_controllerService->PickPlayerEntry (player, entry);

    if (m_controllerThread != nullptr)
    {
        m_controllerThread->Wake();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::SyncPaddleSourceList
//
//  Rebuilds the picker from what is attached and what is playing: a row for
//  each player, the profiles of whatever controller plays there, and the
//  word the closed picker wears. The menu holds the rows by pointer, so the
//  strip is handed the new list whenever they are rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::SyncPaddleSourceList()
{
    InputModeRules::PickerSource      source;
    ControllerInputService::Snapshot  snapshot;



    // Without the controller service there is nothing to list, so the
    // drop-down keeps its own rows rather than going empty.
    if (m_controllerService == nullptr)
    {
        return;
    }

    snapshot = m_controllerService->GetSnapshot();

    // A picked controller that is unplugged is still listed, under the
    // description it had while it was here.
    for (const ControllerDeviceInfo & device : snapshot.devices)
    {
        m_controllerDescriptions[ControllerTokens::UnitToToken (device.unit)] = device.description;
    }

    source.entries           = snapshot.entries;
    source.slots             = snapshot.slots;
    source.devices           = snapshot.devices;
    source.profiles          = GetPickerProfileChoices (snapshot);
    source.knownDescriptions = m_controllerDescriptions;
    source.hasJoyport        = IsJoyportOffered();

    m_input.m_host.SetPlayerPicker (InputModeRules::BuildPicker (source));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::GetPickerProfileChoices
//
//  Each attached controller's profiles of the kind it plays -- its player's
//  mode, either Joyport jack being the Joyport kind -- that kind's built-in
//  profile first, which a model with nothing saved still lists, and its
//  active one.
//  By unit token, since two pads of one model can play different profiles.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, InputModeRules::ProfileChoices> ShellControllers::GetPickerProfileChoices (
    const ControllerInputService::Snapshot & snapshot) const
{
    std::map<std::string, ControllerModelSettings>         models  = m_controllerService->GetModelSettings();
    std::map<std::string, InputModeRules::ProfileChoices>  choices;



    for (const ControllerDeviceInfo & device : snapshot.devices)
    {
        std::string                       token  = ControllerTokens::UnitToToken (device.unit);
        auto                              model  = models.find (ControllerTokens::ModelToToken (device.unit.model));
        auto                              active = snapshot.activeProfiles.find (token);
        auto                              kind   = snapshot.profileModes.find (token);
        ProfileMode                       mode   = (kind != snapshot.profileModes.end()) ? kind->second : ProfileMode::Joystick;
        InputModeRules::ProfileChoices  & entry  = choices[token];

        entry.names  = (model != models.end()) ? model->second.GetProfileNames (mode)
                                               : ControllerModelSettings().GetProfileNames (mode);
        entry.active = (active != snapshot.activeProfiles.end()) ? active->second : std::string();
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::PickControllerProfile
//
//  UI thread. The user chose the profile one controller plays with. The rate
//  paddles return to center, since the old profile's position means nothing
//  to the new one, and the choice is saved with the global controller
//  settings, since it belongs to the controller on every machine.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::PickControllerProfile (ControllerUnitKey unit, std::string profileName)
{
    if (m_controllerService == nullptr)
    {
        return;
    }

    m_controllerService->SetActiveProfile (unit, profileName);
    m_controllerService->ResetPaddleRate();

    if (m_controllerThread != nullptr)
    {
        m_controllerThread->Wake();
    }

    SaveControllerPrefs();
    SyncPaddleSourceList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::StartNewControllerProfile
//
//  New... at the foot of a player's profile section. Settings opens on the
//  Controllers page with Editing on that section's controller, and the New
//  Profile dialog comes up for it. Opened without the Controllers page's own
//  landing, which would move Editing to player one's controller first.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::StartNewControllerProfile (ControllerUnitKey unit)
{
    m_input.m_host.StartNewControllerProfile (unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::ApplyControllerSlotsChange
//
//  UI thread. The players' slots changed, or a controller came or went. The
//  axis owner and the picker follow either way, and the global prefs take
//  the entries when a picked controller was followed to another identity,
//  and the last holders when a slot's holder changed.
//
//  A CONTROLLER CONNECTING TURNS NOTHING OFF. The keys or the mouse picked
//  for Player 1 stay until the user picks something else.
//
//  Each notice -- a controller that left, or one Automatic gave a slot that
//  another controller held last time -- goes onto the notice stack over the
//  picture, never into a dialog: the user did not ask about this, so
//  stopping the machine to have it acknowledged would interrupt them to
//  report something they may not care about.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::ApplyControllerSlotsChange (
    const std::vector<std::wstring>  & notices,
    bool                               haveEntriesChanged,
    bool                               haveLastHoldersChanged)
{
    m_input.SyncGamePortAxisOwner();

    if (haveEntriesChanged)
    {
        m_input.SyncInputModeUi();
    }
    else if (haveLastHoldersChanged)
    {
        SaveControllerPrefs();
    }

    for (const std::wstring & notice : notices)
    {
        m_input.m_host.ShowNotice (notice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::TraceControllerState
//
//  Every step between a controller moving and the game port changing, once a
//  second, to the debugger, while CASSO_CONTROLLER_TRACE is set. Off and free
//  otherwise: the variable is read once.
//
//  A controller that does nothing is the hardest fault to reason about,
//  because every stage fails the same way -- nothing happens. Reading which
//  stage stopped beats guessing at the one before it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::TraceControllerState()
{
    static int                          s_enabled = -1;
    ControllerInputService::TickReport  tick;
    ControllerInputService::Snapshot    snapshot;
    InputModeRules::State               state;
    wchar_t                             line[512] = {};
    int64_t                             nowMs     = 0;



    if (s_enabled < 0)
    {
        wchar_t  value[8] = {};

        s_enabled = (GetEnvironmentVariableW (L"CASSO_CONTROLLER_TRACE", value, ARRAYSIZE (value)) > 0) ? 1 : 0;
    }

    if (s_enabled == 0 || m_controllerService == nullptr)
    {
        return;
    }

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();

    if (nowMs - m_controllerTraceMs < 1000)
    {
        return;
    }

    m_controllerTraceMs = nowMs;

    tick     = m_controllerService->GetLastTickReport();
    snapshot = m_controllerService->GetSnapshot();

    state.arrowsJoystick       = m_input.m_arrowsJoystick;
    state.mousePaddle          = (m_input.m_pointerMode == InputMappingMode::Paddle);
    state.hasController        = snapshot.slots[0].holder.has_value() || snapshot.slots[1].holder.has_value();
    state.isControllerAttached = snapshot.isAnyDriverConnected;

    swprintf_s (line,
        L"[controller] devices=%zu sel=%d xinput=%d "
        L"read=0x%08X connected=%d mapping=%d appActive=%d submit=%d "
        L"paddle=%d,%d buttons=%d%d%d deadZone=%.2f owner=%d\n",
        snapshot.devices.size(),
        tick.hasPlayerOne, tick.isActiveXInput, (unsigned int) tick.readResult, tick.isConnected,
        tick.hasMapping, tick.isAppActive, tick.didSubmit,
        tick.submitted.paddle[0].has_value() ? (int) tick.submitted.paddle[0].value() : -1,
        tick.submitted.paddle[1].has_value() ? (int) tick.submitted.paddle[1].value() : -1,
        tick.submitted.buttons.test (0), tick.submitted.buttons.test (1),
        tick.submitted.buttons.test (2),
        tick.deadzone,
        (int) InputModeRules::GetAxisOwners (state)[0]);

    OutputDebugStringW (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::LoadControllerPrefs
//
//  Hands the saved controller settings, calibrations, active profiles,
//  players and last holders to the service. Anything that cannot be used is
//  said once: it falls back to the default mapping, to automatic
//  calibration or to Automatic, and the next save drops it, so there is
//  nothing to say again.
//
//  THE PLAYERS ARE ADOPTED ONCE. With no players saved, the launched
//  machine's own selection becomes them and is saved at once, so the saved
//  players mark the adoption done and no other machine's selection is ever
//  read for it. Player 1's keys are taken here too, before anything can
//  reconcile Player 1 with a shell that has not heard of them yet.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::LoadControllerPrefs()
{
    ControllerProfileStore    store;
    std::vector<std::string>  rejected;
    PlayerEntries             entries;
    PlayerLastHolders         lastHolders;
    JsonValue                 doc;
    const JsonValue         * uiPrefs   = nullptr;
    bool                      isAdopted = false;



    if (m_controllerService == nullptr)
    {
        return;
    }

    store.FromJson (m_input.m_host.GetPrefs().controllers, rejected);
    m_hadSavedPlayerModes = store.hasPlayerModes;

    m_controllerService->SetModelSettings  (store.models);
    m_controllerService->SetCalibrations   (store.calibrations);
    m_controllerService->SetActiveProfiles (ProfileMode::Joystick, store.activeProfiles);
    m_controllerService->SetActiveProfiles (ProfileMode::Paddle,   store.paddleActiveProfiles);
    m_controllerService->SetActiveProfiles (ProfileMode::Joyport,  store.joyportActiveProfiles);

    if (store.players.has_value())
    {
        entries     = store.players.value();
        lastHolders = store.lastHolders;
    }
    else
    {
        m_input.m_host.LoadMachineUiPrefs (doc, uiPrefs);
        entries   = MachineInputPrefs::ReadAdoptedPlayers (uiPrefs, lastHolders);
        isAdopted = true;
    }

    m_controllerService->SetLastHolders   (lastHolders);
    m_controllerService->SetPlayerEntries (entries);

    m_input.m_arrowsJoystick = m_controllerService->GetPlayerEntries()[0].kind == PlayerEntryKind::ArrowKeys;

    if (isAdopted)
    {
        SaveControllerPrefs();
    }

    if (!rejected.empty())
    {
        m_input.m_host.PostNotice (L"Some saved controller settings couldn't be read, so those settings were reset.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::SaveControllerPrefs
//
//  Writes every controller model's settings, every unit's calibration, the
//  active profiles, the players' entries and the last holders into the
//  global prefs, and saves them only when that changed what they hold: an
//  automatic calibration that learned nothing new costs no write.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::SaveControllerPrefs()
{
    ControllerProfileStore  store;
    JsonValue               controllers;



    if (m_controllerService == nullptr)
    {
        return;
    }

    store.models                = m_controllerService->GetModelSettings();
    store.calibrations          = m_controllerService->GetCalibrations();
    store.activeProfiles        = m_controllerService->GetActiveProfiles (ProfileMode::Joystick);
    store.paddleActiveProfiles  = m_controllerService->GetActiveProfiles (ProfileMode::Paddle);
    store.joyportActiveProfiles = m_controllerService->GetActiveProfiles (ProfileMode::Joyport);
    store.players               = m_controllerService->GetPlayerEntries();
    store.lastHolders           = m_controllerService->GetLastHolders();
    controllers                 = store.ToJson (m_input.m_host.GetPrefs().controllers);

    if (JsonWriter::Write (controllers) == JsonWriter::Write (m_input.m_host.GetPrefs().controllers))
    {
        return;
    }

    m_input.m_host.GetPrefs().controllers = std::move (controllers);
    m_input.m_host.SavePrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::AdoptControllerForMachine
//
//  Sets the service up for the machine being entered: its axis count, the
//  move of its old active profile, a rate reset and a rescan. The players are
//  global and are not touched: they describe the controllers on the desk,
//  not the machine.
//
//  An UNREADABLE OR UNKNOWN TOKEN IS TREATED AS NO CONTROLLER, not as an
//  error: there is only no old profile to move.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::AdoptControllerForMachine (const JsonValue * uiPrefs, const std::string & machineId)
{
    HRESULT                              hr         = S_OK;
    std::string                          token;
    std::string                          legacyProfile;
    std::map<std::string, std::string>   normalProfiles;
    std::optional<ControllerUnitKey>     selection;
    ControllerUnitKey                    unit;
    bool                                 isAdopted  = false;
    const MachineDefinition            * definition = MachineDefinitions::Find (machineId);



    if (m_controllerService == nullptr)
    {
        return;
    }

    // How many axes this machine has, before anything is mapped onto them: a
    // player slot for paddles it lacks is kept and plays nothing (FR-035).
    //
    // From `machineId`, NOT from m_input.m_machine. On the switch path this runs
    // before the new config is adopted, so m_input.m_machine is still the machine
    // being LEFT: reading it left the count a machine behind, which gave a
    // //c player two the //e's PDL2/PDL3 -- no stick, but a live button --
    // and cost an //e player two the axes the machine does have.
    if (definition != nullptr)
    {
        m_controllerService->SetAxisCount (static_cast<size_t> (definition->gamePortAxisCount));
    }

    token = MachineInputPrefs::ReadControllerToken (uiPrefs);

    if (!token.empty())
    {
        hr = ControllerTokens::UnitFromToken (token, unit);

        if (SUCCEEDED (hr))
        {
            selection = unit;
        }
    }

    // A machine's own active profile is from before each controller carried
    // its own. It passes to the machine's saved controller once, when that
    // controller has none recorded, and is written back to no machine after.
    legacyProfile  = MachineInputPrefs::ReadProfileName (uiPrefs);
    normalProfiles = m_controllerService->GetActiveProfiles (ProfileMode::Joystick);
    isAdopted      = ControllerProfileStore::TryAdoptLegacyProfile (normalProfiles, selection, legacyProfile);

    if (isAdopted)
    {
        m_controllerService->SetActiveProfiles (ProfileMode::Joystick, normalProfiles);
    }

    // A rate binding's paddle position belongs to the machine it was moved on.
    m_controllerService->ResetPaddleRate();

    // The players are decided against what is attached now.
    m_controllerService->RequestRescan();

    if (m_controllerThread != nullptr)
    {
        m_controllerThread->Wake();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::MigrateJoyportAtLaunch
//
//  The Joyport setting that per-player modes replaced is read only while no
//  player mode has been saved: a Joyport that was on puts Player 1 in the
//  left jack and Player 2 on Same as Player 1, and the players' modes are
//  saved at once, which marks it read (PlayerModeRules::MigrateAdapter). The
//  old global key is then removed from the global prefs; each machine's own
//  value is left for older builds. Then the machine just built takes the
//  players' modes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::MigrateJoyportAtLaunch (const JsonValue * uiPrefs)
{
    JoyportMigration  migration = PlayerModeRules::MigrateAdapter (m_hadSavedPlayerModes, m_input.m_host.GetPrefs().gamePortAdapter,
                                                                   uiPrefs, m_input.m_machine.GetJoyport() != nullptr);



    if (migration.isJoyport && m_controllerService != nullptr)
    {
        m_controllerService->SetPlayerEntries (PlayerModeRules::ApplyMigration (m_controllerService->GetPlayerEntries()));
    }

    // Saves only what changed, so a launch with the modes already saved
    // writes nothing here.
    SaveControllerPrefs();
    m_hadSavedPlayerModes = true;

    if (migration.shouldRemoveKey)
    {
        m_input.m_host.GetPrefs().gamePortAdapter.clear();
        m_input.m_host.SavePrefs();
    }

    ApplyJoyportToMachine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::ApplyJoyportToMachine
//
//  Attaches the running machine's Joyport while a player is in one of its
//  jacks, connects its paddle inputs while a player beside it stands in for
//  a paddle or a joystick in the rear sockets, and tells the controller
//  service whether the machine has one, so the players in its jacks play
//  their Joyport profiles. Runs once the machine is built -- at a cold start,
//  and on a machine switch right after the new devices are built, before the
//  power cycle that opens the Joyport's reset window -- and after every
//  change to the players' modes. The //c builds no Joyport, so there the
//  jacks play as joysticks and the modes are kept for the next machine that
//  has one.
//
//  Takes no lock: a machine switch calls it while holding the lifetime lock
//  exclusively, and SyncJoyport takes it shared.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::ApplyJoyportToMachine()
{
    SiriusJoyport  * joyport    = m_input.m_machine.GetJoyport();
    bool             hasJoyport = joyport != nullptr;
    PlayerEntries    entries;



    if (m_controllerService)
    {
        entries = m_controllerService->GetPlayerEntries();
        m_controllerService->SetJoyportAvailable (hasJoyport);
    }

    if (joyport != nullptr)
    {
        joyport->SetPaddlesConnected (PlayerModeRules::ArePaddlesConnected (entries, hasJoyport));
        joyport->SetAttached         (PlayerModeRules::IsJoyportOn (entries, hasJoyport));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::ApplyPendingControllerPick
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::ApplyPendingControllerPick()
{
    std::vector<std::wstring>  notices;
    bool                       haveEntriesChanged     = false;
    bool                       haveLastHoldersChanged = false;



    {
        std::lock_guard<std::mutex>  lock (m_controllerPickMutex);

        notices.swap (m_controllerPickNotices);
        haveEntriesChanged         = m_controllerPickHasEntries;
        haveLastHoldersChanged     = m_controllerPickHasHolders;
        m_controllerPickHasEntries = false;
        m_controllerPickHasHolders = false;
    }

    // A device arriving or leaving changes the rows even when it changes
    // nothing else, so the list is rebuilt on every one of these.
    ApplyControllerSlotsChange (notices, haveEntriesChanged, haveLastHoldersChanged);
    SyncPaddleSourceList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::SetControllersActive
//
//  Controllers are read while the emulator is the active app.
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::SetControllersActive (bool active)
{
    if (m_controllerService != nullptr)
    {
        m_controllerService->SetActive (active);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::InstallControllerService
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::InstallControllerService (IControllerBackend & backend)
{
    m_controllerService = std::make_unique<ControllerInputService> (backend, m_input.m_gamePortMixer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellControllers::Start()
{
    HRESULT  hr = S_OK;



    // Physical controllers. The backend is initialized on the controller
    // thread, which owns the window its device notifications arrive at, so
    // nothing here touches a device.
    m_controllerBackend = std::make_unique<Win32ControllerBackend>();
    m_controllerService = std::make_unique<ControllerInputService> (*m_controllerBackend, m_input.m_gamePortMixer);

    // Saved controller settings, calibrations and players, before the thread
    // starts reading, so its first evaluation already knows who was picked
    // and who last held each slot.
    LoadControllerPrefs();

    m_controllerThread  = std::make_unique<ControllerInputThread>();

    // A controller that held a slot and left is said over the picture, and so
    // is one Automatic gave a slot that another controller held last time;
    // the command bar's picker already shows what is playing after either.
    m_controllerService->SetSlotsChangedFn (
        [this] (const ControllerInputService::SlotsChange & change)
        {
            {
                std::lock_guard<std::mutex>  lock (m_controllerPickMutex);

                for (const std::wstring & description : change.departedDescriptions)
                {
                    m_controllerPickNotices.push_back (description + L" disconnected.");
                }

                m_controllerPickNotices.insert (m_controllerPickNotices.end(), change.notices.begin(), change.notices.end());
                m_controllerPickHasEntries = m_controllerPickHasEntries || change.haveEntriesChanged;
                m_controllerPickHasHolders = m_controllerPickHasHolders || change.haveLastHoldersChanged;
            }

            PostMessageW (m_input.m_host.GetMainWindow(), WM_APP_CONTROLLER_PICK, 0, 0);
        });

    // A connect or disconnect changes who owns the axes but tells the user
    // nothing, so it takes the same trip to the UI thread without a notice.
    m_controllerService->SetStateChangedFn ([this]
    {
        PostMessageW (m_input.m_host.GetMainWindow(), WM_APP_CONTROLLER_PICK, 0, 0);
    });

    // The Controllers page asking for a controller to be read wakes the
    // thread, which may be waiting on an event-driven controller that is not
    // moving.
    m_controllerService->SetWakeFn ([this]
    {
        m_controllerThread->Wake();
    });

    hr = m_controllerThread->Start (m_controllerBackend.get(), m_controllerService.get(),
                                    [this] { return m_controllerService->Tick(); });
    IGNORE_RETURN_VALUE (hr, S_OK);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ShellControllers::Stop()
{
    m_controllerThread.reset();

    // What automatic calibration learned this session, while the service that
    // holds it still exists and nothing is reading into it.
    SaveControllerPrefs();

    m_controllerService.reset();
    m_controllerBackend.reset();
}
