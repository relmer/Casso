#pragma once

#include "Pch.h"

#include "Controllers/ControllerInputService.h"
#include "Controllers/InputModeRules.h"
#include "Seams/Win32ControllerBackend.h"
#include "Shell/ControllerInputThread.h"

class IControllerBackend;
class JsonValue;
class ShellInput;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellControllers
//
//  Game controllers and the players: the controller service, its backend and
//  the thread it is read on; the players' slots, modes and profiles; the
//  Joyport; and the controller preferences, per machine.
//  The service submits to ShellInput's game-port mixer, so
//  ShellInput::m_controllers is declared after the mixer and torn down
//  before it.
//
////////////////////////////////////////////////////////////////////////////////

class ShellControllers
{
public:
    explicit ShellControllers (ShellInput & input) : m_input (input) {}

    // The controller stack, started once the game port's sink exists, and
    // stopped before the window goes: the thread stops first, so nothing is
    // inside the service when it goes. Stop also saves what automatic
    // calibration learned this session.
    HRESULT  Start ();
    void     Stop  ();

    // Controllers and players. PickPlayer and PickControllerProfile take
    // their arguments BY VALUE: they arrive from a picker row's dispatch, and
    // picking rebuilds the rows. StartNewControllerProfile opens Settings on
    // the Controllers page with the New Profile dialog up. A mode that cannot
    // have Player 1's keys and mouse turns them off.
    void  PickPlayer                (size_t player, PlayerEntry entry);
    void  SetPlayerMode             (size_t player, PlayerMode mode);
    void  PickControllerProfile     (ControllerUnitKey unit, std::string profileName);
    void  StartNewControllerProfile (ControllerUnitKey unit);
    void  SyncPaddleSourceList      ();
    void  SetControllersActive      (bool active);

    // The players' slots changed, or a controller came or went, on the
    // controller thread: the UI thread takes what it recorded, says so, and
    // rebuilds the picker and the prefs.
    void  ApplyPendingControllerPick ();

    // Every step between a controller moving and the game port changing,
    // once a second to the debugger, while CASSO_CONTROLLER_TRACE is set.
    void  TraceControllerState ();

    // For the Settings sheet's Controllers page. Null before Start and after
    // Stop.
    ControllerInputService *  GetControllerService () const { return m_controllerService.get(); }

    // Whether the running machine can take a Joyport, which is what the
    // picker's row and the Controllers page's switch are offered on.
    bool  IsJoyportOffered () const;

    // The controllers, per machine. Load and Save read and write the global
    // controller settings, calibrations and players. MigrateJoyportAtLaunch
    // reads the Joyport setting the players' modes replaced, while no player
    // mode has been saved.
    void  LoadControllerPrefs    ();
    void  SaveControllerPrefs    ();
    void  MigrateJoyportAtLaunch (const JsonValue * uiPrefs);
    void  ApplyJoyportToMachine  ();
    void  SyncJoyport            ();

    // Reachable by a test: a controller service over a scripted backend.
    void  InstallControllerService (IControllerBackend & backend);

private:
    friend class ShellInput;

    // The players' slots and profiles, and the Joyport they can stand in.
    void  ApplyControllerSlotsChange (const std::vector<std::wstring> & notices, bool haveEntriesChanged, bool haveLastHoldersChanged);
    void  PickPlayerEntry            (size_t player, const PlayerEntry & entry);
    void  AdoptControllerForMachine  (const JsonValue * uiPrefs, const std::string & machineId);
    bool  IsJoyportInEffect          () const;
    bool  IsPlayerOneOnJoyport       () const;

    std::map<std::string, InputModeRules::ProfileChoices>  GetPickerProfileChoices (const ControllerInputService::Snapshot & snapshot) const;

    ShellInput  & m_input;

    // Whether the players' modes were saved before this launch, which marks
    // the Joyport setting they replaced as read.
    bool  m_hadSavedPlayerModes = false;

    // Physical controllers: the devices, the rules that read them, and the
    // thread they are read on.
    std::unique_ptr<Win32ControllerBackend>  m_controllerBackend;
    std::unique_ptr<ControllerInputService>  m_controllerService;
    std::unique_ptr<ControllerInputThread>   m_controllerThread;

    // Written by the controller thread when the players' slots change, read
    // on the UI thread once WM_APP_CONTROLLER_PICK arrives: persisting prefs
    // and raising a notice are both UI-thread work.
    std::mutex                               m_controllerPickMutex;
    std::vector<std::wstring>                m_controllerPickNotices;
    bool                                     m_controllerPickHasEntries = false;
    bool                                     m_controllerPickHasHolders = false;

    // Each controller seen this session, by unit token: how the picker shows
    // a picked controller after it is unplugged. UI thread only.
    std::map<std::string, std::wstring>      m_controllerDescriptions;
    int64_t                                  m_controllerTraceMs = 0;
};
