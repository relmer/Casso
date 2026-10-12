#pragma once

#include "Pch.h"

#include "Controllers/ControllerInputService.h"
#include "Controllers/GamePortInputMixer.h"
#include "Controllers/InputModeRules.h"
#include "Seams/Win32ControllerBackend.h"
#include "Shell/ControllerInputThread.h"
#include "Shell/HeldHostInputs.h"
#include "Shell/Input/CapsLockTracker.h"
#include "Shell/MachineGamePortSink.h"
#include "Ui/UiCommandTypes.h"

class IControllerBackend;
class IInputHost;
class JsonValue;
class MachineHost;
class ShellDebugger;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput
//
//  Host input to the guest: the keys, Caps Lock and paste; the game port's
//  paddles and buttons from the arrow keys, the mouse and game controllers;
//  the //c mouse; the players' slots and the Joyport; and the input held
//  back behind live. The window's own handlers decide what is the chrome's
//  and hand the rest here. What the chrome, the window and the preferences
//  do for it goes through IInputHost; behind live it asks the debugger's
//  divergence gate.
//
////////////////////////////////////////////////////////////////////////////////

class ShellInput
{
public:

    ShellInput (IInputHost & host, MachineHost & machine, ShellDebugger & debugger);
    ~ShellInput();

    // The game port's sink and the controller stack, brought up once the
    // window and the machine exist, and torn down before the window goes:
    // the thread stops first, so nothing is inside the service when it goes.
    // Stop also saves what automatic calibration learned this session.
    HRESULT  Start ();
    void     Stop  ();

    // The viewport's keys and mouse, after the chrome has passed on them.
    bool  HandleViewportKey   (const DxuiKeyEvent   & ev);
    bool  HandleViewportMouse (const DxuiMouseEvent & ev);

    // The keys. ReleaseGuestKeys lets go of the latch, auto-repeat and
    // modifiers on focus loss, since the matching key-ups will never arrive;
    // ReleaseInputsLetGoBehindLive releases each key or mouse button the user
    // let go of while the machine was behind live, once it is live again.
    // ApplyAppleModifierKeys mirrors the host's Alt and Shift onto the //e's
    // Open- and Solid-Apple keys and Shift.
    void  ReleaseGuestKeys             ();
    void  ReleaseInputsLetGoBehindLive ();
    void  ApplyAppleModifierKeys       (WPARAM vk, bool keyDown);

    // Paste the clipboard's text into the guest keyboard with Caps Lock
    // applied, and say so when that changed the text. Ctrl+V and Edit > Paste.
    void  PasteClipboardText ();

    // Hands the //e keyboard the real time that has passed since the previous
    // CPU-thread frame, which is what its auto-repeat cadence runs on. Not
    // driven off the guest clock: Double would then repeat twice as fast and
    // Maximum, which is uncapped, faster than anyone can type against.
    void  TickKeyboardAutoRepeat ();

    // Whether the //e and //c Caps Lock key is down, and the keys and mouse
    // button the machine has down.
    CapsLockTracker  & GetCapsLock        () { return m_capsLock; }
    HeldHostInputs   & GetHeldHostInputs  () { return m_heldHostInputs; }

    // The input modes. The arrow keys and the pointer are two orthogonal
    // stand-ins: SetPointerMapping sets the pointer (Off, Paddle or Mouse);
    // ToggleInputMappingMode selects a mode from the Machine menu, or turns
    // mapping off when it is already the active one; CycleInputMappingMode
    // advances Off -> Joystick -> Paddle -> Off. SyncInputModeUi rebuilds the
    // picker and saves the mode for the machine. With a connected mouse and
    // no pointer mapping chosen, the //c defaults the pointer to Mouse.
    void              SetPointerMapping             (InputMappingMode pointer);
    void              ToggleInputMappingMode        (InputMappingMode target);
    void              CycleInputMappingMode         ();
    void              SyncInputModeUi               ();
    void              ApplyDefaultPointerForMachine ();
    InputMappingMode  GetPointerMode                () const { return m_pointerMode; }
    bool              IsArrowsJoystick              () const { return m_arrowsJoystick; }

    // The single mode the legacy toggle button displays: the pointer
    // mapping when active, else Joystick when the keys mapping is on.
    InputMappingMode  GetDisplayInputMode () const
    {
        return (m_pointerMode != InputMappingMode::Off) ? m_pointerMode
             : (m_arrowsJoystick ? InputMappingMode::Joystick : InputMappingMode::Off);
    }

    // The line the input-mode bar carries while the keys or the mouse drive
    // the game port, and empty when neither does, which is what decides
    // whether the bar and its band exist at all.
    std::wstring  GetStandInBannerText () const;

    // The //c mouse. Active while Mouse mode is selected and the machine has
    // a connected IOU mouse; live once guest software has turned it on, which
    // the firmware's SETMOUSE does through a hardware sequence garbage RAM
    // cannot fake, and which gates hiding and capturing the host pointer.
    // UpdateGuestMouseFromHost maps the host pointer onto the picture and
    // hands the fraction to the device. Connected mirrors the machine's
    // mouseConnected preference; disconnected leaves the silicon but feeds it
    // nothing, as an unplugged DB-9 would.
    bool  IsGuestMouseActive       () const;
    bool  IsGuestMouseLive         () const;
    void  UpdateGuestMouseFromHost (int xPx, int yPx);
    bool  IsMouseConnected         () const { return m_mouseConnected; }
    void  SetMouseConnected        (bool connected) { m_mouseConnected = connected; }

    // Paddle-mode mouse capture. Start hides and confines the cursor and
    // begins relative tracking (only in Paddle mode with the window focused);
    // Stop restores the cursor and releases the clip. ClipPaddleCursorToClient
    // confines the pointer and parks it in the middle, shared by the grab and
    // the re-take after a cancel the layout caused. UpdatePaddleFromMouse
    // turns one mouse move into the held paddle axes, and PushPaddleButton
    // one mouse button into a fire button.
    void  StartPaddleCapture       ();
    void  StopPaddleCapture        ();
    void  ClipPaddleCursorToClient ();
    void  UpdatePaddleFromMouse    (int xClient, int yClient);
    void  PushPaddleButton         (int index, bool pressed);

    // A left-button press over the picture, as the //c guest mouse's button.
    void  PressGuestMouseButton    (int x, int y);
    bool  IsPaddleCaptured         () const { return m_paddleCaptured; }

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

    // The game port after a machine is built, and the paddles and buttons a
    // submission from another thread left for the UI thread to apply.
    void  OnMachineRebuilt ();
    void  FlushGamePort    ();

    // The input mapping and the controllers, per machine. Adopt seeds the
    // live state from a machine's $cassoUiPrefs block (null for a machine
    // with none, which falls back to the legacy global setting) and touches
    // state only, since the machine-switch path runs it on the CPU thread;
    // both callers sync the chrome on the UI thread afterwards. `machineId`
    // is the machine being entered, which the machine cannot yet answer for.
    // Persist writes the live state back into the current machine's block.
    void  LoadControllerPrefs        ();
    void  SaveControllerPrefs        ();
    void  AdoptInputModeForMachine   (const JsonValue * uiPrefs, const std::string & machineId);
    void  PersistInputModeForMachine ();
    void  MigrateJoyportAtLaunch     (const JsonValue * uiPrefs);
    void  ApplyJoyportToMachine      ();
    void  SyncJoyport                ();

    // Reachable by a test: a controller service over a scripted backend, and
    // Player 1's stand-ins set as saved prefs would have left them.
    void  InstallControllerService (IControllerBackend & backend);
    void  SetStandIns              (bool arrowsJoystick, InputMappingMode pointer);

private:

    // The keys' parts: the game-port lines a key event drives, the character
    // a typed key latches, and behind live, the latch a held event leaves.
    void                 UpdateJoystickAxesFromKeys    ();
    void                 UpdateJoystickButtonsFromKeys ();
    bool                 IsDrivingJoystickFromKeys     () const;
    void                 RouteKeyToGamePort            (const DxuiKeyEvent & ev);
    Byte                 MapHostCharToApple            (WPARAM ch);
    std::optional<Byte>  GetHeldKeyLatch               (const DxuiKeyEvent & ev, bool & outIsMachineKey);

    // True when the host's keyboard layout is a Dvorak variant.
    static bool  IsHostLayoutDvorak ();

    // Hands PDL0/PDL1 to whichever host input mode currently drives them.
    void  SyncGamePortAxisOwner ();

    // The combined preset (Joystick = keys only, Paddle and Mouse = pointer
    // only, Off = both off), and what each setter does to the other stand-in
    // with no sync, since the setter syncs once with both already changed.
    void  SetInputMappingMode    (InputMappingMode mode);
    void  SetArrowsJoystick      (bool on);
    void  DropPaddleMode         ();
    void  DropArrowsJoystick     ();
    void  ReleaseArrowKeySources ();
    void  SyncSelectorState      ();
    void  PushPaddlePosition     ();

    // The players' slots and profiles, and the Joyport they can stand in.
    void  ApplyControllerSlotsChange (const std::vector<std::wstring> & notices, bool haveEntriesChanged, bool haveLastHoldersChanged);
    void  PickPlayerEntry            (size_t player, const PlayerEntry & entry);
    void  AdoptControllerForMachine  (const JsonValue * uiPrefs, const std::string & machineId);
    bool  IsJoyportInEffect          () const;
    bool  IsPlayerOneOnJoyport       () const;

    std::map<std::string, InputModeRules::ProfileChoices>  GetPickerProfileChoices (const ControllerInputService::Snapshot & snapshot) const;

    IInputHost     & m_host;
    MachineHost    & m_machine;
    ShellDebugger  & m_debugger;

    // How the host's arrow keys and pointer stand in for the game port, and
    // whether the players' modes were saved before this launch, which marks
    // the Joyport setting they replaced as read.
    InputMappingMode  m_pointerMode         = InputMappingMode::Off;   // Off/Paddle/Mouse
    bool              m_arrowsJoystick      = false;                    // Keys axis
    bool              m_hadSavedPlayerModes = false;
    bool              m_mouseConnected      = true;

    // Last arrow key pressed for each emulated joystick axis pair (0 if
    // none). Lets opposing directions resolve last-pressed-wins so a
    // rolling reversal flips the axis instead of canceling to center.
    WPARAM            m_lastHorizontalArrowVk = 0;
    WPARAM            m_lastVerticalArrowVk   = 0;

    // The single writer of the paddles and pushbuttons. Every host input
    // source submits to the mixer; only the sink touches the machine.
    GamePortInputMixer                    m_gamePortMixer;
    std::unique_ptr<MachineGamePortSink>  m_gamePortSink;

    // Physical controllers: the devices, the rules that read them, and the
    // thread they are read on. Declared after the mixer so they are torn down
    // before it, since the service submits to it.
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

    // Paddle-mode mouse capture. While captured, the cursor is hidden and
    // confined, relative motion drives the paddle axes (held, no recenter),
    // and the mouse buttons drive the fire buttons. m_paddleAxis* are float
    // accumulators (0..255) so sub-unit motion isn't lost between events.
    bool              m_paddleCaptured = false;
    float             m_paddleAxisX    = 127.0f;
    float             m_paddleAxisY    = 127.0f;

    // What the captured mouse asks of the game port, and what the //e's
    // modifier keys do: left Alt as Open-Apple (PB0), right Alt as
    // Solid-Apple (PB1), Shift as PB2.
    GamePortContribution  m_mousePaddleContribution;
    GamePortContribution  m_appleModifierContribution;

    // The keys and mouse button the machine has down, and those let go of
    // while the host input gate kept the release from it.
    HeldHostInputs        m_heldHostInputs;

    // Whether the //e and //c Caps Lock key is down: down until the first
    // Caps Lock press in this window, the host's from then on. Session-scoped,
    // so a machine switch keeps it and every launch starts over.
    CapsLockTracker       m_capsLock;
};
