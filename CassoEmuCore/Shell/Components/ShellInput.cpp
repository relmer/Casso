#include "Pch.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/MachineHost.h"
#include "Config/GlobalUserPrefs.h"
#include "Shell/Components/ShellHistory.h"
#include "Shell/IInputHost.h"
#include "Core/JsonWriter.h"
#include "Machines/MachineDefinition.h"
#include "Shell/ClipboardManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"
#include "resource.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Config/DiskSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::ShellInput
//
////////////////////////////////////////////////////////////////////////////////

ShellInput::ShellInput (IInputHost & host, MachineHost & machine, ShellHistory & history) :
    m_host     (host),
    m_machine  (machine),
    m_history  (history)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::~ShellInput
//
////////////////////////////////////////////////////////////////////////////////

ShellInput::~ShellInput() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGuestKeys
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ReleaseGuestKeys()
{
    std::shared_lock<std::shared_mutex>  lifetime   (m_machine.GetLifetimeLock(), std::defer_lock);
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;



    // The modifier and fire keys go through the mixer, which takes the lock
    // itself when it writes: the release reaches the machine now, or when a
    // rebuild in progress finishes.
    m_appleModifierContribution = GamePortContribution();
    m_gamePortMixer.ReleaseSource (GamePortSource::AppleModifierKeys);
    m_gamePortMixer.ReleaseSource (GamePortSource::FireKeys);

    // A machine switch holds the lock exclusively while it replaces the
    // devices; the new machine starts with every key up anyway.
    if (!lifetime.try_lock())
    {
        return;
    }

    // Behind live in reverse execution's history the keyboard holds a
    // recorded position, which a release must not change; the releases are
    // kept for when the machine is live again.
    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    m_heldHostInputs.OnReleaseAll (isGateOpen);

    if (!isGateOpen)
    {
        return;
    }

    if (m_machine.GetRefs().keyboard != nullptr)
    {
        m_machine.GetRefs().keyboard->SetKeyDown (false);
        m_machine.GetRefs().keyboard->EndKeyRepeat();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInputsLetGoBehindLive
//
//  Once the machine is live again it stands where it left live, with the
//  keys and mouse button held then still down. A release for each one the
//  user let go of in the meantime goes through the same path a real release
//  takes, so the modifiers and the arrow joystick follow too. Run while the
//  gate is still held, it waits for the next call.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ReleaseInputsLetGoBehindLive()
{
    std::vector<WPARAM>  releases;
    DxuiKeyEvent         release;



    if (m_machine.GetHostInputGate().IsHeld())
    {
        return;
    }

    releases     = m_heldHostInputs.TakeReleases();
    release.kind = DxuiKeyEventKind::Up;

    //  The mouse's locks are taken and let go for it alone, so a key's
    //  release takes them afresh in HandleViewportKey rather than nested.
    for (WPARAM input : releases)
    {
        if (input != HeldHostInputs::kMouseButton)
        {
            release.vk = input;
            (void) HandleViewportKey (release);
        }
        else
        {
            ReleaseMouseButton();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseMouseButton
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ReleaseMouseButton()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;



    if (lifetime.owns_lock() && m_machine.GetMouse() != nullptr && m_machine.GetHostInputGate().TryEnter (gate))
    {
        m_machine.GetMouse()->SetButton (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAppleModifierKeys
//
//  Submits the host modifier state to the game-port mixer as the //e modifier
//  keys' contribution: left Alt -> Open Apple (PB0, $C061), right Alt ->
//  Closed Apple (PB1, $C062), Shift -> Shift (PB2, $C063). GetKeyState gives
//  the canonical left/right state, so a modifier stays asserted while either
//  physical key is still down. A no-op on the ][/][+ where the keyboard is
//  not an Apple //e keyboard.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ApplyAppleModifierKeys (WPARAM vk, bool keyDown)
{
    constexpr size_t  kOpenAppleButton  = 0;
    constexpr size_t  kSolidAppleButton = 1;
    constexpr size_t  kShiftButton      = 2;
    HRESULT           hr                = S_OK;
    auto            * iieKbd            = m_machine.GetRefs().iieKeyboard;
    bool              isAlt             = vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU;
    bool              isShift           = vk == VK_SHIFT;



    BAIL_OUT_IF (iieKbd == nullptr || !(isAlt || isShift), S_OK);

    if (isAlt)
    {
        m_appleModifierContribution.buttons.set (kOpenAppleButton,  (GetKeyState (VK_LMENU) & 0x8000) != 0);
        m_appleModifierContribution.buttons.set (kSolidAppleButton, (GetKeyState (VK_RMENU) & 0x8000) != 0);
    }
    else
    {
        m_appleModifierContribution.buttons.set (kShiftButton, keyDown);
    }

    m_gamePortMixer.Submit (GamePortSource::AppleModifierKeys, m_appleModifierContribution);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleViewportKey
//
//  Applies one guest keystroke to the Apple ][ keyboard latch and game port.
//  Everything arriving here has already been classified as the guest's.
//
//  Down, Up, and Char are three different jobs, not three cases of one:
//  Down handles control codes and joystick axes, Up disarms, and Char is the
//  only path that types a printable character.
//
//  Auto-repeat is the subtlest part. Control-code presses are gated on the
//  repeat bit so the HOST's repeat never reaches the latch -- a fresh press
//  arms the $C000 strobe once and registers the key with BeginKeyRepeat, and
//  the emulated //e then generates its own authentic repeat cadence in Tick.
//  Letting both repeats run would double the rate and sound wrong.
//
//  A key-up always calls EndKeyRepeat. The //e latch holds exactly one
//  key, so a release necessarily ends the current repeat; clearing it also
//  stops a later non-character press (a bare modifier, say) from resurrecting
//  the previous character's repeat.
//
//  Joystick emulation overlays the arrows and X / Z, and only when the mode
//  is on AND a game-port paddle bank exists. Three details matter:
//
//    - whether the keys drive the joystick is asked per event, so a mode
//      change between a press and its release is honored and nothing is
//      left held
//    - arrows are WITHHELD from the keyboard latch in this mode; a held
//      direction would otherwise flood $C000 and starve a game's reads
//    - the fire buttons are re-resolved on EVERY key event, not just X / Z,
//      so an Alt press re-applies its Open / Closed-Apple mapping without
//      clobbering a still-held X, and a released X cannot leave a button
//      stuck down while Alt is still held
//
//  Opposing arrows resolve last-pressed-wins via the per-axis memory, which
//  is what makes a quick left-right reversal read as a reversal rather than
//  as centered.
//
//  The Char path feeds MapTypedChar so the //c keyboard switch can remap to
//  Dvorak. The host's own layout is probed live and pushed in, so a host that
//  is ALREADY Dvorak skips the remap instead of translating twice. Clipboard
//  paste bypasses this path entirely and calls PressKey directly -- pasted
//  text is never remapped, matching the hardware encoder.
//
//  Behind live every event is held, with the key it would latch, until the
//  replayed guest reads the keyboard where it would differ or the machine
//  is live; the game-port side goes to the mixer at once, which holds it.
//
//  Always returns true: nothing here bubbles back to the framework, since the
//  shell's escape routes live in the OnKeyDown pre-checks, not in the sink.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::HandleViewportKey (const DxuiKeyEvent & ev)
{
    std::shared_lock<std::shared_mutex>  lifetime     (m_machine.GetLifetimeLock(), std::try_to_lock);
    HostInputGate::Entry                 gate;
    bool                                 hasKeyboard  = false;
    bool                                 isGateOpen   = false;
    bool                                 isMachineKey = false;
    std::optional<Byte>                  latch;
    DivergenceVerdict                    verdict      = DivergenceVerdict::Proceed;



    // A machine switch holds the lock exclusively while it replaces the
    // devices; a key that lands in that window is dropped.
    if (!lifetime.owns_lock())
    {
        return false;
    }

    // Behind live in reverse execution's history the devices hold a recorded
    // position, so the guest gets no keys until the machine is live again.
    // Each press and release is tracked either way, so a key let go of behind
    // live is released once the machine is live.
    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);

    if (ev.kind == DxuiKeyEventKind::Down && !ev.repeat)
    {
        m_heldHostInputs.OnPress (ev.vk, isGateOpen);
    }
    else if (ev.kind == DxuiKeyEventKind::Up)
    {
        m_heldHostInputs.OnRelease (ev.vk, isGateOpen);
    }

    // A key into the machine while it replays history would change it. It is
    // held, with no question, until the replayed guest reads the keyboard
    // where the key would change what it reads, or the machine is live; the
    // keys that follow wait with it. A key that drives the game port goes to
    // the mixer now, which holds it there the same way.
    verdict = m_history.GetDivergenceGate().JudgeKey (!isGateOpen, ev);

    if (verdict == DivergenceVerdict::Hold)
    {
        latch = GetHeldKeyLatch (ev, isMachineKey);

        m_history.GetDivergenceGate().HoldKey (ev, latch, isMachineKey);
        RouteKeyToGamePort (ev);
        m_history.PublishHeldInput();

        // Paused, the replay runs, so the guest reads the key.
        if (ev.kind == DxuiKeyEventKind::Down)
        {
            m_history.RunForHeldInput();
        }

        return true;
    }

    if (!isGateOpen)
    {
        return true;
    }

    // The guest owns every key that reaches here either way; with no keyboard
    // device there is simply nothing to deliver it to.
    hasKeyboard = m_machine.GetRefs().keyboard != nullptr;

    if (hasKeyboard && ev.kind == DxuiKeyEventKind::Down)
    {
        WPARAM           vk         = ev.vk;
        Byte             appleCode  = 0;
        AppleSpecialKey  specialKey = AppleSpecialKey::Left;
        bool             isSpecial  = AppleKeyMapping::TryMapVkToSpecialKey (vk, specialKey);
        bool             hasTheKey  = !isSpecial ||
                                      m_machine.GetRefs().keyboard->MapSpecialKey (specialKey) != 0;

        // A named key the running machine's keyboard does not have was never
        // pressed as far as the guest is concerned, so it must not raise
        // any-key-down either -- $C010 reporting a key held while $C000 holds
        // nothing is a state the hardware cannot be in.
        if (hasTheKey)
        {
            m_machine.GetRefs().keyboard->SetKeyDown (true);
        }

        // TAB and Escape reach us twice: once as this keydown, and again as
        // the WM_CHAR Windows manufactures from it. The key route below is
        // the authoritative one -- it is the only one that can tell the TAB
        // key from Ctrl+I, which sends the same $09 -- so the character is
        // swallowed. Set on repeats too, since each repeated keydown brings
        // its own character along. Nothing here depends on that character
        // actually arriving: OnKeyDown clears the flag on the way in, so an
        // Alt-held TAB or Escape, whose WM_SYSCHAR never reaches OnChar,
        // cannot leave it armed for the next key.
        if (isSpecial && AppleKeyMapping::DoesSpecialKeySynthesizeChar (specialKey))
        {
            m_host.SwallowNextChar();
        }

        // Arrows / TAB / Escape / DELETE are delivered as KEYS, and the
        // device decides both which code each sends and whether this machine
        // has it at all. Gated on the auto-repeat bit so the host OS repeat
        // never reaches the latch; a fresh press arms the $C000 strobe once
        // and registers the key for the emulator's own authentic //e
        // auto-repeat cadence (Tick). With "Map arrows to joystick" on (and a
        // game-port paddle bank present), arrow keys are withheld from the
        // keyboard latch so a held direction cannot flood $C000 and starve a
        // joystick game's reads.
        if (!ev.repeat && isSpecial && !(IsDrivingJoystickFromKeys() && AppleKeyMapping::IsArrowVk (vk)))
        {
            appleCode = m_machine.GetRefs().keyboard->PressSpecialKey (specialKey);

            if (appleCode != 0)
            {
                m_machine.GetRefs().keyboard->BeginKeyRepeat (appleCode);
            }
        }

        RouteKeyToGamePort (ev);
    }
    else if (hasKeyboard && ev.kind == DxuiKeyEventKind::Up)
    {
        WPARAM           vk         = ev.vk;
        AppleSpecialKey  specialKey = AppleSpecialKey::Left;
        bool             isSpecial  = AppleKeyMapping::TryMapVkToSpecialKey (vk, specialKey);
        bool             hasTheKey  = !isSpecial ||
                                      m_machine.GetRefs().keyboard->MapSpecialKey (specialKey) != 0;

        // The same question the press asked, asked again: a key this machine
        // does not have was never pressed, so its release must not undo the
        // state some other key is still holding. Gating only the press left
        // the up arrow on a ][+ able to clear any-key-down, and disarm the
        // repeat, while a real key was still down -- $C010 reporting nothing
        // held while $C000 holds a character.
        if (hasTheKey)
        {
            m_machine.GetRefs().keyboard->SetKeyDown (false);

            // Disarm auto-repeat on release. The //e latch holds a single
            // key, so a key-up always ends the current repeat; this also
            // clears any stale armed key so a later non-character press
            // (e.g. a bare modifier) can never resurrect the previous
            // character's repeat.
            m_machine.GetRefs().keyboard->EndKeyRepeat();
        }

        RouteKeyToGamePort (ev);
    }
    else if (hasKeyboard && ev.vk <= 127)   // DxuiKeyEventKind::Char
    {
        // $00 is a real character: Windows sends it for Ctrl+Shift+2, the
        // PC's Ctrl+@, and some games wait for it.
        Byte  code = MapHostCharToApple (ev.vk);

        m_machine.GetRefs().keyboard->PressKey (code);
        m_machine.GetRefs().keyboard->BeginKeyRepeat (code);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDrivingJoystickFromKeys
//
//  Arrow keys double as the emulated joystick axes / the X / Z keys as fire
//  buttons when "Map arrows to joystick" is on AND a game-port paddle bank
//  is present. Asked per event so a mode change between press and release
//  is always honored.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsDrivingJoystickFromKeys() const
{
    return m_arrowsJoystick &&
           (m_machine.GetRefs().iieSoftSwitches != nullptr || m_machine.GetRefs().gamePort != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RouteKeyToGamePort
//
//  The game-port side of a key event: the //e's Open and Closed Apple and
//  Shift lines, and with the joystick on the keys, the arrows' axes and the
//  fire buttons. Each goes through the mixer, which writes it live and holds
//  it behind live.
//
//  Opposing arrows resolve last-pressed-wins via the per-axis memory, which
//  is what makes a quick left-right reversal read as a reversal rather than
//  as centered. The fire buttons are re-resolved on EVERY key event in
//  joystick mode, not just X / Z, so an Alt press/release re-applies its
//  Open/Closed-Apple mapping without clobbering a still-held X / Z, and a
//  released X / Z can't leave a button stuck while Alt is down. The matching
//  X / Z WM_CHAR is suppressed in OnChar so the letters don't also type into
//  the //e keyboard latch.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::RouteKeyToGamePort (const DxuiKeyEvent & ev)
{
    WPARAM  vk            = ev.vk;
    bool    driveJoystick = IsDrivingJoystickFromKeys();



    if (m_machine.GetRefs().keyboard == nullptr)
    {
        return;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        ApplyAppleModifierKeys (vk, true);

        if (driveJoystick && AppleKeyMapping::IsArrowVk (vk))
        {
            if (vk == VK_LEFT || vk == VK_RIGHT)
            {
                m_lastHorizontalArrowVk = vk;
            }
            else
            {
                m_lastVerticalArrowVk = vk;
            }

            UpdateJoystickAxesFromKeys();
        }

        if (driveJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else if (ev.kind == DxuiKeyEventKind::Up)
    {
        // Release the //e Open/Closed-Apple and Shift modifiers as the host
        // releases the physical keys.
        ApplyAppleModifierKeys (vk, false);

        if (m_arrowsJoystick && AppleKeyMapping::IsArrowVk (vk))
        {
            UpdateJoystickAxesFromKeys();
        }

        if (m_arrowsJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapHostCharToApple
//
//  A typed character as the keyboard will latch it. The //c keyboard switch
//  remaps to Dvorak when engaged; the host's own layout is probed live and
//  pushed in, so a host that is ALREADY Dvorak skips the remap instead of
//  translating twice. Clipboard paste bypasses this path entirely and calls
//  PressKey directly -- pasted text is never remapped, matching the hardware
//  encoder.
//
//  Caps Lock brackets the remap. The host's comes out first, so the remap
//  sees the key the user pressed with only Shift applied; the emulated one
//  goes on last, because the encoder raises the letter a key produces, and
//  on Dvorak some QWERTY letter keys produce punctuation it must leave
//  alone. Paste skips both, keeping its case.
//
////////////////////////////////////////////////////////////////////////////////

Byte ShellInput::MapHostCharToApple (WPARAM ch)
{
    Byte  code         = static_cast<Byte> (ch);
    bool  hostCapsLock = (GetKeyState (VK_CAPITAL) & 1) != 0;



    code = CapsLockTracker::RemoveHostCapsLock (code, hostCapsLock);

    if (m_machine.GetRefs().iieKeyboard != nullptr)
    {
        m_machine.GetRefs().iieKeyboard->SetHostKeyboardDvorak (IsHostLayoutDvorak());

        code = m_machine.GetRefs().iieKeyboard->MapTypedChar (code);
    }

    return CapsLockTracker::ApplyCapsLock (code, m_capsLock.IsOn (hostCapsLock));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHeldKeyLatch
//
//  Behind live, what a key event will leave in the latch once it goes in: a
//  fresh press of a special key the machine has, unless the arrows drive the
//  joystick, or a typed character. Whether the event is a key this
//  machine's keyboard has decides whether it holds any-key-down.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> ShellInput::GetHeldKeyLatch (
    const DxuiKeyEvent  & ev,
    bool                & outIsMachineKey)
{
    AppleKeyboard        * keyboard   = m_machine.GetRefs().keyboard;
    AppleSpecialKey        specialKey = AppleSpecialKey::Left;
    bool                   isSpecial  = false;
    Byte                   code       = 0;
    std::optional<Byte>    latch;



    outIsMachineKey = false;

    if (keyboard == nullptr)
    {
        return latch;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        isSpecial       = AppleKeyMapping::TryMapVkToSpecialKey (ev.vk, specialKey);
        code            = isSpecial ? keyboard->MapSpecialKey (specialKey) : 0;
        outIsMachineKey = !isSpecial || code != 0;

        if (!ev.repeat && code != 0 && !(IsDrivingJoystickFromKeys() && AppleKeyMapping::IsArrowVk (ev.vk)))
        {
            latch = keyboard->GetTypedLatch (code);
        }
    }
    else if (ev.kind == DxuiKeyEventKind::Char && ev.vk <= 127)
    {
        latch = keyboard->GetTypedLatch (MapHostCharToApple (ev.vk));
    }

    return latch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleViewportMouse
//
//  IDxuiViewportInputSink. The Apple ][ has no viewport-rect mouse mapping
//  in the current build: paddle input is a captured relative-motion mode
//  driven directly from the window's OnMouseMove (SetCapture snaps the cursor to
//  center), and the joystick maps to arrow keys -- neither fits the
//  viewport's absolute-rect forwarding. Returns false so any future
//  in-viewport click continues to bubble to the chrome hit-testing that
//  owns it today.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::HandleViewportMouse (const DxuiMouseEvent & ev)
{
    UNREFERENCED_PARAMETER (ev);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickAxesFromKeys
//
//  Host UI thread. Resolves the four arrow keys into the two emulated
//  joystick axes and submits them to the game-port mixer as the arrow keys'
//  contribution. The mixer writes them to whichever game port the machine
//  has while the arrow keys own the axes; the PREAD timer ($C070 /
//  $C064-$C067) turns them into analog readings.
//
//  Reads real-time physical key state via GetAsyncKeyState rather than the
//  per-thread GetKeyState table, which can desync (and leave an axis stuck)
//  if a key-up is lost to a focus change. Opposing keys resolve
//  last-pressed-wins so a rolling reversal flips the axis instead of
//  canceling to center.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::UpdateJoystickAxesFromKeys()
{
    GamePortContribution  contribution;
    bool                  left         = false;
    bool                  right        = false;
    bool                  up           = false;
    bool                  down         = false;
    Byte                  x            = GamePortState::kPaddleCenter;
    Byte                  y            = GamePortState::kPaddleCenter;



    left  = (GetAsyncKeyState (VK_LEFT)  & 0x8000) != 0;
    right = (GetAsyncKeyState (VK_RIGHT) & 0x8000) != 0;
    up    = (GetAsyncKeyState (VK_UP)    & 0x8000) != 0;
    down  = (GetAsyncKeyState (VK_DOWN)  & 0x8000) != 0;

    if (left && right)
    {
        x = (m_lastHorizontalArrowVk == VK_RIGHT) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (left)
    {
        x = s_kPaddleAxisMin;
    }
    else if (right)
    {
        x = s_kPaddleAxisMax;
    }

    if (up && down)
    {
        y = (m_lastVerticalArrowVk == VK_DOWN) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (up)
    {
        y = s_kPaddleAxisMin;
    }
    else if (down)
    {
        y = s_kPaddleAxisMax;
    }

    contribution.paddle[0] = x;
    contribution.paddle[1] = y;

    // Player 1's keys in a jack close its switches, on both jacks until the
    // controllers place the players.
    if (m_controllers.IsPlayerOneOnJoyport())
    {
        contribution.keyJacks.set();
    }

    m_gamePortMixer.Submit (GamePortSource::ArrowKeys, contribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickButtonsFromKeys
//
//  Host UI thread. Resolves the X / Z letter keys into the two emulated
//  joystick fire buttons and submits them to the game-port mixer as the fire
//  keys' contribution. The mixer writes PB0/PB1 to the //e keyboard's
//  Open/Closed-Apple lines or the ][/][+ game port's pushbuttons ($C061,
//  $C062), whichever the machine has.
//
//  Reads real-time physical key state via GetAsyncKeyState, matching the
//  axis helper so a key-up lost to a focus change can't wedge a button on.
//  The fire state is OR'd with the host left/right Alt keys so the X / Z
//  mapping coexists with the existing Alt->button mapping instead of
//  clobbering a held Alt.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::UpdateJoystickButtonsFromKeys()
{
    GamePortContribution  contribution;
    bool                  xDown        = false;
    bool                  zDown        = false;
    bool                  leftAltDown  = false;
    bool                  rightAltDown = false;
    std::bitset<2>        buttons;



    // Only read the physical keys while WE are the foreground app. The async
    // key state is global, so a held Alt during the Alt-Tab switcher (or any
    // time another app is active) would otherwise keep re-pressing Open-Apple /
    // button 0 in the guest every frame -- e.g. re-triggering a Print Shop
    // print on the way out. Foreground reads normally (no added latency); not
    // foreground leaves the buttons released. Matches the input gate used
    // elsewhere (GetForegroundWindow() != m_host.GetMainWindow()).
    if (GetForegroundWindow() == m_host.GetMainWindow())
    {
        xDown        = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton0Vk)) & 0x8000) != 0;
        zDown        = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton1Vk)) & 0x8000) != 0;
        leftAltDown  = (GetKeyState      (VK_LMENU)                                & 0x8000) != 0;
        rightAltDown = (GetKeyState      (VK_RMENU)                                & 0x8000) != 0;
    }

    buttons = InputModeRules::GetFireKeyButtons (xDown, zDown, leftAltDown, rightAltDown,
                                                 m_controllers.IsJoyportInEffect());

    contribution.buttons.set (0, buttons.test (0));
    contribution.buttons.set (1, buttons.test (1));
    m_gamePortMixer.Submit (GamePortSource::FireKeys, contribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputMappingMode
//
//  Sets the host input mapping mode (Off / Joystick / Paddle) and persists
//  it. Leaving Paddle drops the mouse capture. Joystick resolves the axes
//  and fire buttons from the current key state so a held arrow / X / Z
//  takes effect immediately. Off and Paddle both neutralize the
//  key-derived stick and buttons so the game port reads neutral; Paddle
//  then centers the paddle and grabs the mouse.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetInputMappingMode (InputMappingMode mode)
{
    // Combined PRESET setter (button cycle + legacy callers): selects BOTH
    // axes of the split model. The Machine-menu items toggle the
    // axes independently via SetArrowsJoystick / SetPointerMapping, so
    // e.g. Joystick keys + Mouse pointer can coexist (disjoint game-port
    // lines); presets deliberately reset the other axis.
    switch (mode)
    {
        case InputMappingMode::Joystick:
            SetPointerMapping (InputMappingMode::Off);
            SetArrowsJoystick (true);
            break;

        case InputMappingMode::Paddle:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Mouse);
            break;

        case InputMappingMode::Off:
        default:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Off);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetArrowsJoystick
//
//  Keys axis of the split input model: maps arrows + X/Z onto the joystick
//  axes / fire buttons. Independent of the Mouse pointer (disjoint hardware),
//  but mutually exclusive with Paddle -- both drive the game-port paddle
//  lines -- so enabling it drops an active Paddle. Turning it off neutralizes
//  the key-derived stick and buttons so a held key can't stay stuck (axes
//  left alone while Paddle owns them).
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetArrowsJoystick (bool on)
{
    // Mirror of the rule in SetPointerMapping: the Keys axis drives PDL0/1,
    // so enabling it must drop an active Paddle (they fight over the same
    // game-port lines). Mouse uses a separate slot card and may coexist.
    // Dropped WITHOUT a sync of its own, so the one below finds the keys on
    // and the paddle off together: Player 1's entry then goes from the mouse
    // straight to the keys, never through Automatic, which would hand Player
    // 1 a controller for a moment and announce it.
    if (on && m_pointerMode == InputMappingMode::Paddle)
    {
        DropPaddleMode();
    }

    m_arrowsJoystick = on;
    SyncGamePortAxisOwner();
    SyncInputModeUi();

    if (on)
    {
        UpdateJoystickAxesFromKeys();
        UpdateJoystickButtonsFromKeys();
        return;
    }

    ReleaseArrowKeySources();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropPaddleMode
//
//  The paddle's half of turning the keys on: the capture ends and the
//  pointer mapping goes Off. No sync; SetArrowsJoystick runs one with both
//  changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::DropPaddleMode()
{
    m_mouseInput.StopPaddleCapture();
    m_pointerMode = InputMappingMode::Off;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropArrowsJoystick
//
//  The keys' half of turning the paddle on. No sync; SetPointerMapping runs
//  one with both changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::DropArrowsJoystick()
{
    m_arrowsJoystick = false;
    ReleaseArrowKeySources();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseArrowKeySources
//
//  The arrows drive nothing now: the axes go to whichever owner is left
//  (center when none) and the fire buttons release. Alt held as Open or
//  Solid-Apple is the modifier keys' own contribution, so it stays pressed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ReleaseArrowKeySources()
{
    m_gamePortMixer.ReleaseSource (GamePortSource::ArrowKeys);
    m_gamePortMixer.ReleaseSource (GamePortSource::FireKeys);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPointerMapping
//
//  Pointer axis of the split input model: Off / Paddle (capturing) /
//  Mouse (non-capturing //c IOU mouse). Paddle and Mouse are mutually
//  exclusive by construction -- both claim the host pointer.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetPointerMapping (InputMappingMode pointer)
{
    InputMappingMode  prev = m_pointerMode;



    if (pointer == InputMappingMode::Joystick)   // not a pointer mode
    {
        pointer = InputMappingMode::Off;
    }

    // Paddle owns the game-port paddle axes (PDL0/1) -- the same lines the
    // arrows->joystick remap drives -- so entering Paddle must drop the Keys
    // axis. Mouse is a separate slot card (disjoint lines) and may coexist
    // with Joystick, so only Paddle clears it. Enforced here (not just in the
    // SetInputMappingMode presets) so the per-segment / menu toggle paths
    // honor the same paddle-vs-joystick exclusivity. Dropped without a sync
    // of its own, for the reason SetArrowsJoystick gives.
    if (pointer == InputMappingMode::Paddle && m_arrowsJoystick)
    {
        DropArrowsJoystick();
    }

    if (prev == InputMappingMode::Paddle && pointer != InputMappingMode::Paddle)
    {
        m_mouseInput.StopPaddleCapture();
    }

    // Leaving Mouse: release a held guest button so it can't stick, and
    // drop the absolute target so the guest mouse stops tracking.
    if (prev == InputMappingMode::Mouse && pointer != InputMappingMode::Mouse
        && m_machine.GetMouse() != nullptr)
    {
        m_machine.GetMouse()->SetButton (false);
        m_machine.GetMouse()->ClearHostTarget();
    }

    m_pointerMode = pointer;
    SyncGamePortAxisOwner();
    SyncInputModeUi();

    if (pointer == InputMappingMode::Paddle)
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_mouseInput.m_paddleAxisX = (float) s_kPaddleCenterByte;
        m_mouseInput.m_paddleAxisY = (float) s_kPaddleCenterByte;
        m_mouseInput.PushPaddlePosition();

        // THE HUD NOTICE SAYS THIS NOW, in both presentations. Entering
        // paddle mode used to force a tooltip up for eight seconds, because
        // the capture means the hover that would normally dismiss one never
        // fires -- so it had to time out instead. That put a panel over the
        // chrome it was anchored to, on top of whatever tooltip the pointer
        // had already summoned, and it said what the notice over the picture
        // now says for exactly as long as the capture lasts.
        //
        // AND WHATEVER IS ALREADY UP GOES NOW. The click that turns paddle
        // mode on is a click ON a control, so its tooltip is showing -- and
        // the capture that follows takes the pointer, so the move that would
        // dismiss it never comes. It would sit there until its lifetime ran
        // out, which is a long time to leave a balloon over a game.
        m_host.HideTooltips();

        m_mouseInput.StartPaddleCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStandInBannerText
//
//  The line the input-mode bar carries, or empty when there is no bar. The
//  rule is in InputModeRules; this only gathers what it asks about.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellInput::GetStandInBannerText() const
{
    InputModeRules::State  state;



    state.arrowsJoystick = m_arrowsJoystick;
    state.mousePaddle    = (m_pointerMode == InputMappingMode::Paddle);

    return InputModeRules::GetStandInBannerText (state);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncGamePortAxisOwner
//
//  Who owns each axis: Player 1's keys or mouse own PDL0 and PDL1 while one of
//  them is picked -- the keys only outside the Joyport's jacks, and the mouse
//  PDL0 alone while Player 2 plays, outside Two paddles mode -- and the
//  controller source owns the rest,
//  and all four otherwise. The rule itself is in InputModeRules so it can be
//  asserted without a machine. Which controller drives which axis is settled
//  inside the controller source, which leaves an axis it does not drive at
//  center.
//
//  THE KEYS AND THE MOUSE ARE PLAYER 1'S ENTRY, so the players follow them:
//  a controller then plays as Player 2 beside them rather than as Player 1.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SyncGamePortAxisOwner()
{
    InputModeRules::State             state;
    InputModeRules::AxisOwners        owners;
    ControllerInputService::Snapshot  snapshot;
    PlayerEntry                       playerOne;
    PlayerEntryKind                   wanted    = PlayerEntryKind::Automatic;
    size_t                            axis      = 0;



    state.arrowsJoystick = m_arrowsJoystick;
    state.mousePaddle    = (m_pointerMode == InputMappingMode::Paddle);

    if (m_controllers.GetControllerService() != nullptr)
    {
        snapshot  = m_controllers.GetControllerService()->GetSnapshot();
        playerOne = snapshot.entries[0];
        wanted    = playerOne.kind;

        state.isKeysOnJoyport   = PlayerModeRules::IsOnJoyport (snapshot.entries, 0, m_machine.GetJoyport() != nullptr);
        state.isSecondPlaying   = PlayerSlotPolicy::IsDrivingSlot (snapshot.slots[1]);
        state.isMouseTwoPaddles = PlayerModeRules::ResolveMode (snapshot.entries, 0, m_machine.GetJoyport() != nullptr) == PlayerMode::TwoPaddles;

        if (state.mousePaddle)
        {
            wanted = PlayerEntryKind::MousePaddle;
        }
        else if (state.arrowsJoystick)
        {
            wanted = PlayerEntryKind::ArrowKeys;
        }
        else if (wanted == PlayerEntryKind::ArrowKeys || wanted == PlayerEntryKind::MousePaddle)
        {
            wanted = PlayerEntryKind::Automatic;
        }

        if (wanted != playerOne.kind)
        {
            playerOne.kind = wanted;
            playerOne.unit.reset();
            m_controllers.PickPlayerEntry (0, playerOne);
        }
    }

    owners = InputModeRules::GetAxisOwners (state);

    for (axis = 0; axis < owners.size(); axis++)
    {
        m_gamePortMixer.SetAxisOwner (axis, owners[axis]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDefaultPointerForMachine
//
//  A //c with its mouse connected and no pointer mapping chosen
//  defaults Pointer to Mouse -- a runtime nudge, not persisted, and
//  invisible until guest mouse software runs (firmware-live gate). Called
//  after the per-machine connected states are seeded at launch and on
//  machine switch.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ApplyDefaultPointerForMachine()
{
    if (m_machine.GetMouse() != nullptr && m_mouseConnected
        && m_pointerMode == InputMappingMode::Off)
    {
        // State only -- NO chrome work here. This runs on the CPU thread
        // during SwitchMachine, and the selector sync measures text through
        // the Dxui renderer, which is UI-thread-only (DxuiAssertUiThread
        // fired on a //c -> //e switch). Both paths already sync on the UI
        // thread afterwards: a machine switch posts WM_APP_DXUI_UPDATE_TITLE,
        // whose handler runs ReflowChromeForMachineChange, and the launch
        // path lays out the chrome later in Initialize.
        m_pointerMode = InputMappingMode::Mouse;

        // SyncSelectorState touches Dxui (text measurement) and asserts the
        // UI thread. On a machine switch this runs
        // on the CPU thread, so defer the chrome reflection to the post-switch
        // handler on the UI thread (WM_APP_DXUI_UPDATE_TITLE, posted by the
        // UpdateWindowTitle at the end of SwitchMachine). On the UI thread
        // (launch, or before the window exists) reflect it immediately.
        bool  offUiThread = (m_host.GetMainWindow() != nullptr) &&
                            (GetWindowThreadProcessId (m_host.GetMainWindow(), nullptr) != GetCurrentThreadId());

        if (offUiThread)
        {
            return;
        }

        SyncSelectorState();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleInputMappingMode
//
//  Advances the input mapping mode Off -> Joystick -> Paddle -> Off.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::CycleInputMappingMode()
{
    InputMappingMode  next    = InputMappingMode::Off;
    InputMappingMode  current = GetDisplayInputMode();



    // Mouse (mouse-capable machines only) deliberately precedes Paddle:
    // entering Paddle CAPTURES the pointer (clicks become fire buttons),
    // so any mode placed after Paddle would be unreachable by clicking
    // the toggle. Mouse mode is non-capturing, so the toggle stays
    // clickable and Paddle remains reachable from it.
    switch (current)
    {
        case InputMappingMode::Off:
            next = InputMappingMode::Joystick;
            break;

        case InputMappingMode::Joystick:
            next = (m_machine.GetMouse() != nullptr && m_mouseConnected)
                       ? InputMappingMode::Mouse
                       : InputMappingMode::Paddle;
            break;

        case InputMappingMode::Mouse:
            next = InputMappingMode::Paddle;
            break;

        case InputMappingMode::Paddle:
        default:
            next = InputMappingMode::Off;
            break;
    }

    SetInputMappingMode (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleInputMappingMode
//
//  Radio-group selection for the Machine-menu Joystick / Paddle items:
//  picks `target`, or turns mapping Off when `target` is already active so
//  re-selecting the current mode clears it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ToggleInputMappingMode (InputMappingMode target)
{
    switch (target)
    {
        case InputMappingMode::Joystick:
            SetArrowsJoystick (!m_arrowsJoystick);
            break;

        case InputMappingMode::Paddle:
            SetPointerMapping (m_pointerMode == InputMappingMode::Paddle
                                   ? InputMappingMode::Off : InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetPointerMapping (m_pointerMode == InputMappingMode::Mouse
                                   ? InputMappingMode::Off : InputMappingMode::Mouse);
            break;

        default:
            SetInputMappingMode (InputMappingMode::Off);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteClipboardText
//
//  Ctrl+V and Edit > Paste. Caps Lock applies only where the machine has the
//  key: the //e and //c keyboards. The ][ and ][+ have no lower case, and their
//  keyboard raises every letter itself, so there is nothing to explain.
//
//  The notice appears only when the paste actually differs from the
//  clipboard, which is the moment the user can see the result and wonder why.
//
//  Behind live a paste is held as typed keys are: its text waits in the
//  buffer, which drains only once the machine is live, and the question
//  whether to discard the history after this point waits until the guest
//  reads the keyboard where the first character would change what it reads.
//  A yes types the text in live from there, a no drops it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PasteClipboardText()
{
    bool    hasCapsLockKey = m_machine.GetRefs().iieKeyboard != nullptr;
    bool    hostCapsLock   = (GetKeyState (VK_CAPITAL) & 1) != 0;
    bool    capsLockOn     = hasCapsLockKey && m_capsLock.IsOn (hostCapsLock);
    bool    raisedLetters  = false;
    bool    isBehindLive   = m_history.IsBehindLiveForUi();
    Byte    first          = 0;
    size_t  length         = 0;



    if (m_host.GetClipboard() == nullptr || !m_history.AllowCommand (IDM_EDIT_PASTE, std::string()))
    {
        return;
    }

    if (isBehindLive && !m_history.GetDivergenceGate().HasHeldPaste())
    {
        m_history.SetPasteLengthBeforeHold (m_host.GetClipboard()->GetPasteLength (first));
    }

    raisedLetters = m_host.GetClipboard()->PasteFromClipboard (m_host.GetMainWindow(), capsLockOn);
    length        = m_host.GetClipboard()->GetPasteLength (first);

    if (isBehindLive && length > m_history.GetPasteLengthBeforeHold() && m_machine.GetRefs().keyboard != nullptr)
    {
        m_history.GetDivergenceGate().HoldPaste (m_machine.GetRefs().keyboard->GetTypedLatch (first));
        m_history.PublishHeldInput();
    }

    if (raisedLetters)
    {
        m_host.ShowNotice (CapsLockTracker::kpszPasteRaisedNotice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptInputModeForMachine
//
//  Seeds the live input mapping. Player 1's keys and mouse are the global
//  players' entries and follow the user from machine to machine; only the
//  //c's own mouse comes from the machine's $cassoUiPrefs block, and a
//  machine that has never stored it falls back to the legacy global setting,
//  so upgrading from a build where the mapping was global keeps it. The mouse
//  picked as Player 1's paddle takes the pointer, so it outranks that mouse.
//
//  STATE ONLY, no chrome. The machine-switch path calls this on the CPU
//  thread, and SyncSelectorState measures text through Dxui, which asserts
//  the UI thread; both callers reflect the state on the UI thread afterwards
//  (the switch through the post-switch reflow, launch through the layout that
//  follows). This is the same rule ApplyDefaultPointerForMachine follows, and
//  it runs before that one so the //c mouse nudge sees the restored value.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::AdoptInputModeForMachine (const JsonValue * uiPrefs, const std::string & machineId)
{
    PlayerEntryKind  playerOne     = PlayerEntryKind::Automatic;
    bool             machineArrows = false;



    MachineInputPrefs::ReadFromUiPrefs (uiPrefs,
                                        m_host.GetPrefs().pointerMapping,
                                        machineArrows,
                                        m_pointerMode);

    if (m_controllers.GetControllerService() != nullptr)
    {
        playerOne = m_controllers.GetControllerService()->GetPlayerEntries()[0].kind;
    }

    m_arrowsJoystick = playerOne == PlayerEntryKind::ArrowKeys;

    if (playerOne == PlayerEntryKind::MousePaddle)
    {
        m_pointerMode = InputMappingMode::Paddle;
    }

    m_controllers.AdoptControllerForMachine (uiPrefs, machineId);

    // The mixer is thread-safe and writes on the UI thread, so handing the
    // axes over from here is safe on the CPU thread too.
    SyncGamePortAxisOwner();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PersistInputModeForMachine
//
//  Saves the live mapping. The players' entries, Player 1's keys and mouse
//  among them, go to the global prefs. The machine's $cassoUiPrefs block
//  takes only its pointer mapping, which is the //c's own mouse: a pointer
//  mode that holds the pointer is written as Off.
//
//  THE MACHINE'S OLD SELECTION IS LEFT ALONE. Its controller, two-player
//  block and arrows-to-joystick are neither written nor removed, so an older
//  build reading the same file keeps its own behavior.
//
//  Best-effort: a missing store or machine name, or a write failure, just
//  leaves the on-disk state as it was.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PersistInputModeForMachine()
{
    HRESULT                                         hr = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  entries;



    m_controllers.SaveControllerPrefs();

    if (m_host.GetConfigStore() == nullptr || m_machine.GetCurrentMachineName().empty())
    {
        return;
    }

    entries = MachineInputPrefs::BuildUiPrefEntries (m_pointerMode);

    hr = DiskSettings::WriteSavedUiPrefs (
             *m_host.GetConfigStore(), m_host.GetFileSystem(), m_machine.GetCurrentMachineName(), entries);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickKeyboardAutoRepeat
//
//  Advances the //e keyboard's auto-repeat cadence by the real time since the
//  previous CPU-thread frame.
//
//  The cadence used to be counted in guest cycles, ticked once per
//  instruction. That reads as authentic and is not: the //e's repeat is
//  generated in the keyboard encoder, off an oscillator that knows nothing
//  about the 6502, and a typist's finger rests in real seconds either way. So
//  the guest clock dragged it along -- Double repeated at twice the rate off
//  half the delay, and Maximum, which runs uncapped at tens of times real,
//  turned a held key into hundreds of characters a second and made the
//  machine impossible to type on.
//
//  Once a frame is resolution enough for a 500 ms delay and a 15 cps rate,
//  and the clock read replaces one call per instruction. The stamp is
//  advanced by the whole microseconds handed over rather than set to now, so
//  the sub-microsecond remainder is carried instead of being dropped every
//  frame -- at Maximum speed the frames are short enough for that to matter.
//
//  Each repeat that fires is journaled as a key press at the cycle it landed
//  on, so a replay applies the same presses without this host-time timer.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::TickKeyboardAutoRepeat()
{
    uint32_t  elapsed = 0;
    Byte      fired   = 0;



    if (m_machine.GetRefs().keyboard == nullptr)
    {
        return;
    }

    // The first frame since the machine came up starts the interval rather
    // than reporting the whole time since the epoch, and a stall is charged
    // once at the device's initial delay: the device caps anything past it at
    // one repeat anyway. Both rules are the clock's, and tested there.
    elapsed = m_host.TakeKeyRepeatElapsedUs (AppleKeyboard::kKeyRepeatDelayUs);

    if (elapsed == 0)
    {
        return;
    }

    fired = m_machine.GetRefs().keyboard->TickAutoRepeat (static_cast<uint32_t> (elapsed));

    if (fired != 0)
    {
        m_machine.RecordInput (InputKind::AutoRepeat, fired, 0, {});
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHostLayoutDvorak
//
//  True when the host's active keyboard layout is a Dvorak variant. Probed
//  behaviorally rather than by KLID string, so it catches every Dvorak layout
//  (US, left/right-hand, third-party) without a hard-coded list: VkKeyScanEx
//  reports which physical key (VK code) produces 'o'. On QWERTY that is VK 'O';
//  on Dvorak 'o' lives on the physical 'S' key, so it reports VK 'S'. Unlike
//  ToUnicode this leaves no dead-key state behind. The //c keyboard switch only
//  needs to remap when the host is QWERTY -- see Apple2eKeyboard::MapTypedChar.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsHostLayoutDvorak()
{
    HKL    hkl = GetKeyboardLayout (0);
    SHORT  vk  = VkKeyScanExW (L'o', hkl);



    // vk == -1 means 'o' is unreachable on this layout -- assume QWERTY.
    return vk != -1 && LOBYTE (vk) == 'S';
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellInput::Start()
{
    HRESULT  hr = S_OK;



    // Every paddle and pushbutton write goes through the mixer from here on.
    // Writes happen on this (the UI) thread; a submission from any other
    // thread posts a flush back to the window.
    m_gamePortSink = std::make_unique<MachineGamePortSink> (m_machine.GetLifetimeLock(), [this]
    {
        GamePortTargets            targets;
        const MachineDefinition  * definition = MachineDefinitions::Find (m_machine.GetConfig().machineId);

        targets.gamePort    = m_machine.GetRefs().gamePort;
        targets.iieSwitches = m_machine.GetRefs().iieSoftSwitches;
        targets.iieKeyboard = m_machine.GetRefs().iieKeyboard;
        targets.joyport     = m_machine.GetJoyport();

        if (definition != nullptr)
        {
            targets.axisCount = static_cast<size_t> (definition->gamePortAxisCount);
        }

        return targets;
    });

    // Held back while reverse execution has the machine behind live.
    m_gamePortSink->SetInputGate (&m_machine.GetHostInputGate());

    // Held back, a real press or deflection is held line by line, and the
    // lines go to the watch the replay checks the guest's reads against.
    m_gamePortSink->SetDivergenceGate (&m_history.GetDivergenceGate(), [this]
    {
        m_history.PublishHeldInput();
    });

    m_gamePortMixer.SetApplyThread (std::this_thread::get_id(), [hwnd = m_host.GetMainWindow()]
    {
        PostMessageW (hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
    });

    m_gamePortMixer.SetSink (m_gamePortSink.get());

    hr = m_controllers.Start();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::Stop()
{
    m_controllers.Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::OnMachineRebuilt
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::OnMachineRebuilt()
{
    m_gamePortMixer.NotifyMachineRebuilt();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::FlushGamePort
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::FlushGamePort()
{
    m_gamePortMixer.FlushPending();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::SetStandIns
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetStandIns (bool arrowsJoystick, InputMappingMode pointer)
{
    m_arrowsJoystick = arrowsJoystick;
    m_pointerMode    = pointer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncInputModeUi
//
//  Common tail for the axis setters: refresh the toggle button's displayed
//  mode, then persist the pair into the current machine's prefs.
//
//  The two setters call each other to enforce paddle-vs-joystick exclusivity,
//  so one user action can land here twice. Both passes write the same live
//  state, and the second is what ends up on disk.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SyncInputModeUi()
{
    // SyncSelectorState rebuilds the picker rows; calling SyncPaddleSourceList
    // here as well rebuilt them twice for every mode change.
    SyncSelectorState();
    PersistInputModeForMachine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncSelectorState
//
//  Re-reads what the strip's input controls show. Both read their own state
//  through functors, so nothing is pushed into them; the paddle picker's
//  rows are the one thing that has to be rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SyncSelectorState()
{
    m_controllers.SyncPaddleSourceList();
}





