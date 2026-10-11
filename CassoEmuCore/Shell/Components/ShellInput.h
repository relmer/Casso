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

class EmulatorShell;
class IInputHost;
class JsonValue;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput
//
//  Host input to the guest: the keys, Caps Lock and paste; the game port's
//  paddles and buttons from the arrow keys, the mouse and game controllers;
//  the //c mouse; the players' slots and the Joyport; and the input held
//  back behind live. The window's own handlers decide what is the chrome's
//  and hand the rest here.
//
////////////////////////////////////////////////////////////////////////////////

class ShellInput
{
public:

    ShellInput (EmulatorShell & shell, IInputHost & host);
    ~ShellInput();

    // The viewport's keys and mouse, after the chrome has passed on them.
    bool  HandleViewportKey   (const DxuiKeyEvent   & ev);
    bool  HandleViewportMouse (const DxuiMouseEvent & ev);

    // Release the guest keyboard latch + auto-repeat + modifiers. Called on
    // focus loss: the matching key-ups will never arrive once focus moves.
    void               ReleaseGuestKeys ();

    // Inject a release for each key or mouse button the user let go of while
    // the machine was behind live, once it is live again.
    void               ReleaseInputsLetGoBehindLive ();

    // Paste the clipboard's text into the guest keyboard with Caps Lock
    // applied, and say so when that changed the text. Ctrl+V and Edit > Paste.
    void PasteClipboardText();

    // Hands the //e keyboard the real time that has passed since the previous
    // CPU-thread frame, which is what its auto-repeat cadence runs on. Not
    // driven off the guest clock: Double would then repeat twice as fast and
    // Maximum, which is uncapped, faster than anyone can type against.
    void TickKeyboardAutoRepeat();

    void        ApplyAppleModifierKeys       (WPARAM vk, bool keyDown);

    // Stage the emulated joystick axes from the host arrow keys.
    void    UpdateJoystickAxesFromKeys ();

    // Stage the emulated joystick fire buttons from the host X / Y keys.
    void    UpdateJoystickButtonsFromKeys ();

    // A key event's parts: the game-port lines it drives, the character a
    // typed key latches, and behind live, the latch a held event will leave.
    bool                 IsDrivingJoystickFromKeys () const;
    void                 RouteKeyToGamePort        (const DxuiKeyEvent & ev);
    Byte                 MapHostCharToApple        (WPARAM ch);
    std::optional<Byte>  GetHeldKeyLatch           (const DxuiKeyEvent & ev, bool & outIsMachineKey);

    // Hands PDL0/PDL1 to whichever host input mode currently drives them.
    void    SyncGamePortAxisOwner ();

    // The line the input-mode bar carries while the keys or the mouse drive
    // the game port, and empty when neither does, which is what decides
    // whether the bar and its band exist at all.
    std::wstring  GetStandInBannerText () const;

    // Every step between a controller moving and the game port changing,
    // once a second to the debugger, while CASSO_CONTROLLER_TRACE is set.
    void    TraceControllerState ();
    int64_t m_controllerTraceMs = 0;

    // The players' slots changed, or a controller came or went: the axis
    // owner, the picker and the prefs follow on the UI thread.
    void    ApplyControllerSlotsChange (const std::vector<std::wstring> & notices, bool haveEntriesChanged, bool haveLastHoldersChanged);
    void    PickPlayerEntry        (size_t player, const PlayerEntry & entry);
    void    LoadControllerPrefs        ();
    void    SaveControllerPrefs        ();
    void    SyncPaddleSourceList   ();

    std::map<std::string, InputModeRules::ProfileChoices>  GetPickerProfileChoices (const ControllerInputService::Snapshot & snapshot) const;

    // BY VALUE for the same reason as PickPlayer. Empty for the mode's
    // built-in profile.
    void    PickControllerProfile  (ControllerUnitKey unit, std::string profileName);

    // Opens Settings on the Controllers page with the New Profile dialog up,
    // for the given controller. BY VALUE for the same reason as PickPlayer.
    void    StartNewControllerProfile (ControllerUnitKey unit);

    // Set the host input mapping mode (Off / Joystick / Paddle): persists
    // it, re-syncs the game port (resolving joystick axes / buttons from
    // current keys, centering on leave), and starts or stops mouse capture
    // for Paddle mode.
    // Split input model. SetArrowsJoystick / SetPointerMapping set
    // the two orthogonal axes independently (menu items); SetInputMappingMode
    // applies a combined PRESET (button cycle + legacy callers): Joystick =
    // keys-only, Paddle/Mouse = pointer-only, Off = both off. Paddle<->Mouse
    // stay exclusive (both claim the host pointer).
    void    SetInputMappingMode (InputMappingMode mode);
    void    SetArrowsJoystick   (bool on);
    void    SetPointerMapping   (InputMappingMode pointer);   // Off/Paddle/Mouse

    // What each setter does to the OTHER stand-in, with no sync: the setter
    // that calls it syncs once, with both already changed.
    void    DropPaddleMode         ();
    void    DropArrowsJoystick     ();
    void    ReleaseArrowKeySources ();

    // The single mode the legacy toggle button displays: the pointer
    // mapping when active, else Joystick when the keys mapping is on.
    InputMappingMode  GetDisplayInputMode() const
    {
        return (m_pointerMode != InputMappingMode::Off) ? m_pointerMode
             : (m_arrowsJoystick ? InputMappingMode::Joystick : InputMappingMode::Off);
    }

    // With a connected mouse and no pointer mapping chosen, the //c
    // defaults Pointer to Mouse (runtime nudge, not persisted; invisible
    // until mouse software runs thanks to the firmware-live gate).
    void    ApplyDefaultPointerForMachine();

    // Whether the running machine's Joyport is on, which it is while a
    // player's mode puts that player in one of its jacks. Never on the //c,
    // which has none. UI thread.
    bool             IsJoyportInEffect  () const;

    // The input mapping, per machine. Adopt seeds the live state from a
    // machine's $cassoUiPrefs block (null for a machine with none, which
    // falls back to the legacy global setting); Persist writes the live
    // state back into the current machine's block.
    //
    // Adopt touches STATE ONLY -- no chrome -- because the machine-switch
    // path runs it on the CPU thread, and the selector sync measures text
    // through Dxui, which asserts the UI thread. Both callers sync the
    // chrome on the UI thread afterwards.
    // `machineId` is the machine being ENTERED. The switch path runs these
    // before it adopts the new config, so m_machine cannot answer for it.
    void    AdoptInputModeForMachine   (const JsonValue * uiPrefs, const std::string & machineId);
    void    AdoptControllerForMachine (const JsonValue * uiPrefs, const std::string & machineId);
    void    PersistInputModeForMachine ();

    // Radio-group toggle for the Machine-menu items: selects `target`, or
    // turns mapping Off if `target` is already the active mode.
    void    ToggleInputMappingMode (InputMappingMode target);

    // The user picked an entry in a player's submenu. BY VALUE, not by
    // reference. The entry arrives from a picker row's dispatch, and picking
    // rebuilds the rows, so a reference into the row would outlive the row it
    // refers to.
    void    PickPlayer             (size_t player, PlayerEntry entry);

    // The user set a player's mode, Joystick or Paddle. Player 1's keys and
    // mouse are a joystick and a paddle, so a mode that cannot have them
    // turns them off.
    void    SetPlayerMode          (size_t player, PlayerMode mode);

    // Whether the running machine can take a Joyport, which is what the
    // picker's row and the Controllers page's switch are offered on.
    bool    IsJoyportOffered       () const;

    // //c mouse mode. True while Mouse mode is selected AND the
    // current machine has the IOU mouse — every runtime consumer guards on
    // this, so a persisted Mouse mode on a mouse-less machine is inert.
    bool    IsGuestMouseActive     () const;

    // True when guest software has actually turned the mouse on: the
    // firmware's SETMOUSE programs ENBXY through the IOU for every active
    // mode, a hardware sequence ($C079 -> $C059 -> $C078) that garbage RAM
    // cannot fake. Gates the cursor-hide and button capture so the host
    // pointer never vanishes (or gets swallowed) while nothing mouse-aware
    // is running — which in turn makes Mouse mode safe to leave on.
    bool    IsGuestMouseLive       () const;

    // Absolute host→guest mapping: the host position inside the emulator
    // viewport maps proportionally into the firmware's live clamp window
    // (read from the slot-7 screen holes along with the current position),
    // and the delta is queued as movement units. Self-correcting — any units
    // the firmware clamps away are re-derived from the holes on the next
    // move. No-op until the guest app has initialized the mouse firmware
    // (garbage holes fail the sanity checks).
    void    UpdateGuestMouseFromHost (int xPx, int yPx);

    // Advance the input mapping mode Off -> Joystick -> Paddle -> Off,
    // routed from the drive-bar widget, the Machine menu, and Ctrl+Shift+J.
    void    CycleInputMappingMode ();

    // Paddle-mode mouse capture. Start hides + confines the cursor and
    // begins relative tracking (no-op unless the mode is Paddle and the
    // window is focused); Stop restores the cursor and releases the clip.
    // UpdatePaddleFromMouse maps one WM_MOUSEMOVE into the held paddle
    // axes via the recenter-on-move trick. PushPaddleButtons stages the
    // mouse buttons onto the emulated fire buttons.
    void    StartPaddleCapture     ();
    void    StopPaddleCapture      ();

    // Confines the pointer to the client and parks it in the middle, which is
    // what "captured" means to the user. Shared by the grab and by the re-take
    // after a cancel the layout caused.
    void    ClipPaddleCursorToClient ();

    void    UpdatePaddleFromMouse  (int xClient, int yClient);
    void    PushPaddlePosition     ();
    void    PushPaddleButton       (int index, bool pressed);

    // For the Settings sheet's Controllers page. Null before the shell has
    // initialized its controller stack and after it has torn it down.
    ControllerInputService *  GetControllerService () const { return m_controllerService.get(); }

    void  MigrateJoyportAtLaunch           (const JsonValue * uiPrefs);
    void  ApplyJoyportToMachine            ();
    void  SyncJoyport                      ();
    bool  IsPlayerOneOnJoyport             () const;

    // //c only: whether the mouse peripheral is plugged into the DB-9 port
    // Mirrors $cassoUiPrefs.mouseConnected (default CONNECTED);
    // flipped live by IDM_MOUSE_CONNECT/DISCONNECT. Disconnected = the IOU
    // silicon stays but IsGuestMouseActive() is false (no host input feeds
    // the device) and the input-mode cycle hides Mouse -- indistinguishable
    // from an unplugged DB-9 on real hardware.
    bool                     m_mouseConnected = true;

    // Last arrow key pressed for each emulated joystick axis pair (0 if
    // none). Lets opposing directions resolve last-pressed-wins so a
    // rolling reversal flips the axis instead of canceling to center.
    WPARAM          m_lastHorizontalArrowVk = 0;
    WPARAM          m_lastVerticalArrowVk   = 0;


    //  Reachable by a test subclass: Player 1's stand-ins, the mixer and the
    //  controller service are what a pick in the picker changes, and a test
    //  installs a service over a scripted backend to watch the entries it is
    //  handed. Declared here, in their original order, so the teardown order
    //  below is unchanged.

    // How host arrow / pointer input is mapped onto the emulated game
    // port (Off / Joystick / Paddle). Mirrors
    // GlobalUserPrefs (split model) and is cycled via the Machine
    // menu's "Cycle Input Mode" item, Ctrl+Shift+J, and the drive-bar widget.
    InputMappingMode  m_pointerMode    = InputMappingMode::Off;   // Off/Paddle/Mouse
    bool              m_arrowsJoystick = false;                    // Keys axis

    // Whether the players' modes were saved before this launch, which marks
    // the Joyport setting they replaced as read. Taken when the controller
    // prefs load, before the adoption can save any.
    bool              m_hadSavedPlayerModes = false;

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


    // Written by the controller thread when the players' slots change,
    // read on the UI thread once WM_APP_CONTROLLER_PICK arrives: persisting
    // prefs and raising a notice are both UI-thread work.
    std::mutex                               m_controllerPickMutex;
    std::vector<std::wstring>                m_controllerPickNotices;
    bool                                     m_controllerPickHasEntries = false;
    bool                                     m_controllerPickHasHolders = false;

    // Each controller seen this session, by unit token: how the picker shows
    // a picked controller after it is unplugged. UI thread only.
    std::map<std::string, std::wstring>      m_controllerDescriptions;


    // Paddle-mode mouse capture. While captured, the cursor is hidden and
    // confined, relative motion drives the paddle axes (held, no recenter),
    // and the mouse buttons drive the fire buttons. m_paddleAxis* are float
    // accumulators (0..255) so sub-unit motion isn't lost between events.
    bool              m_paddleCaptured = false;
    float             m_paddleAxisX    = 127.0f;
    float             m_paddleAxisY    = 127.0f;

    // What the captured mouse asks of the game port: the held paddle axes and
    // the two mouse buttons, submitted to the mixer together.
    GamePortContribution  m_mousePaddleContribution;

    // What the //e modifier keys ask of the game port: left Alt as
    // Open-Apple (PB0), right Alt as Solid-Apple (PB1), Shift as PB2.
    GamePortContribution  m_appleModifierContribution;

    // The keys and mouse button the machine has down, and those let go of
    // while the host input gate kept the release from it.
    HeldHostInputs        m_heldHostInputs;

    // Whether the //e and //c Caps Lock key is down: down until the first
    // Caps Lock press in this window, the host's from then on. Session-scoped,
    // so a machine switch keeps it and every launch starts over.
    CapsLockTracker                           m_capsLock;

    // True when the host's keyboard layout is a Dvorak variant.
    static bool  IsHostLayoutDvorak ();

    EmulatorShell  & m_shell;
    IInputHost     & m_host;
};
