#include "Pch.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/MachineHost.h"
#include "Config/GlobalUserPrefs.h"
#include "Shell/ShellDebugger.h"
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

ShellInput::ShellInput (IInputHost & host, MachineHost & machine, ShellDebugger & debugger) :
    m_host     (host),
    m_machine  (machine),
    m_debugger (debugger)
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
    std::shared_lock<std::shared_mutex>  gate;
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
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    std::shared_lock<std::shared_mutex>  gate;
    std::vector<WPARAM>                  releases;
    DxuiKeyEvent                         release;



    if (m_machine.GetHostInputGate().IsHeld())
    {
        return;
    }

    releases     = m_heldHostInputs.TakeReleases();
    release.kind = DxuiKeyEventKind::Up;

    for (WPARAM input : releases)
    {
        if (input != HeldHostInputs::kMouseButton)
        {
            release.vk = input;
            (void) HandleViewportKey (release);
        }
        else if (lifetime.owns_lock() && m_machine.GetMouse() != nullptr && m_machine.GetHostInputGate().TryEnter (gate))
        {
            m_machine.GetMouse()->SetButton (false);
        }
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
    std::shared_lock<std::shared_mutex>  gate;
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
    verdict = m_debugger.GetDivergenceGate().JudgeKey (!isGateOpen, ev);

    if (verdict == DivergenceVerdict::Hold)
    {
        latch = GetHeldKeyLatch (ev, isMachineKey);

        m_debugger.GetDivergenceGate().HoldKey (ev, latch, isMachineKey);
        RouteKeyToGamePort (ev);
        m_debugger.PublishHeldInput();

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
    if (IsPlayerOneOnJoyport())
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
                                                 IsJoyportInEffect());

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
    StopPaddleCapture();
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
//  SyncJoyport
//
//  UI thread, after the players' modes may have changed: the running
//  machine's Joyport follows them on the next button read, with no reset.
//  The keys are resubmitted, since Player 1's keys in a jack close its
//  switches rather than drive the paddle inputs, and the Alt keys drop out
//  of the fire keys while the Joyport is on.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SyncJoyport()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock());
    std::shared_lock<std::shared_mutex>  gate;
    bool                                 isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);



    // Behind live the Joyport holds the recorded settings, which the guest
    // reads; the new ones reach it once the machine is live.
    if (isGateOpen)
    {
        ApplyJoyportToMachine();
    }
    else
    {
        m_debugger.OweJoyportSync();
    }

    gate.unlock();
    lifetime.unlock();

    if (m_arrowsJoystick)
    {
        UpdateJoystickAxesFromKeys();
        UpdateJoystickButtonsFromKeys();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportInEffect
//
//  Whether the running machine reads the Joyport: a player is in one of its
//  jacks, on a machine that has one. A machine builds a Joyport exactly when
//  it has the annunciators to drive one.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsJoyportInEffect() const
{
    PlayerEntries  entries;



    if (m_controllerService != nullptr)
    {
        entries = m_controllerService->GetPlayerEntries();
    }

    return PlayerModeRules::IsJoyportOn (entries, m_machine.GetJoyport() != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPlayerOneOnJoyport
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsPlayerOneOnJoyport() const
{
    PlayerEntries  entries;



    if (m_controllerService != nullptr)
    {
        entries = m_controllerService->GetPlayerEntries();
    }

    return PlayerModeRules::IsOnJoyport (entries, 0, m_machine.GetJoyport() != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportOffered
//
//  Whether the running machine can take a Joyport, which is what the picker's
//  row and the Controllers page's switch are offered on: both apply at once
//  to the running machine, so a machine staged elsewhere has no say.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsJoyportOffered() const
{
    return m_machine.GetJoyport() != nullptr;
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
        StopPaddleCapture();
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

        m_paddleAxisX = (float) s_kPaddleCenterByte;
        m_paddleAxisY = (float) s_kPaddleCenterByte;
        PushPaddlePosition();

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

        StartPaddleCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickPlayer
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

void ShellInput::PickPlayer (size_t player, PlayerEntry entry)
{
    bool  isPlayerOne = player == 0;



    if (isPlayerOne && entry.kind == PlayerEntryKind::ArrowKeys)
    {
        SetArrowsJoystick (true);
        return;
    }

    if (isPlayerOne && entry.kind == PlayerEntryKind::MousePaddle)
    {
        SetPointerMapping (InputMappingMode::Paddle);
        return;
    }

    PickPlayerEntry (player, entry);

    if (isPlayerOne && m_arrowsJoystick)
    {
        SetArrowsJoystick (false);
    }

    if (isPlayerOne && m_pointerMode == InputMappingMode::Paddle)
    {
        SetPointerMapping (InputMappingMode::Off);
    }

    SyncJoyport();
    SyncGamePortAxisOwner();
    SyncInputModeUi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPlayerMode
//
//  UI thread. The mode is set FIRST, and the service returns Player 1 to
//  Automatic when the new mode cannot have the keys or the mouse; turning
//  those off afterwards then finds Player 1 already on Automatic and picks
//  nothing on the way. The machine's Joyport follows the new modes, and the
//  entries are saved with the global prefs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetPlayerMode (size_t player, PlayerMode mode)
{
    bool  isPlayerOne = player == 0;



    if (m_controllerService == nullptr)
    {
        return;
    }

    m_controllerService->SetPlayerMode (player, mode);

    if (isPlayerOne && PlayerModeRules::IsPaddleMode (mode) && m_arrowsJoystick)
    {
        SetArrowsJoystick (false);
    }

    if (isPlayerOne && !PlayerModeRules::IsPaddleMode (mode) && m_pointerMode == InputMappingMode::Paddle)
    {
        SetPointerMapping (InputMappingMode::Off);
    }

    SyncJoyport();
    SyncGamePortAxisOwner();
    SyncInputModeUi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickPlayerEntry
//
//  Hands one player's entry to the service and wakes its thread, so the
//  choice takes effect on the next read rather than at the end of whatever
//  wait the thread is in.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PickPlayerEntry (size_t player, const PlayerEntry & entry)
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
//  SyncPaddleSourceList
//
//  Rebuilds the picker from what is attached and what is playing: a row for
//  each player, the profiles of whatever controller plays there, and the
//  word the closed picker wears. The menu holds the rows by pointer, so the
//  strip is handed the new list whenever they are rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SyncPaddleSourceList()
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

    m_host.SetPlayerPicker (InputModeRules::BuildPicker (source));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPickerProfileChoices
//
//  Each attached controller's profiles of the kind it plays -- its player's
//  mode, either Joyport jack being the Joyport kind -- that kind's built-in
//  profile first, which a model with nothing saved still lists, and its
//  active one.
//  By unit token, since two pads of one model can play different profiles.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, InputModeRules::ProfileChoices> ShellInput::GetPickerProfileChoices (
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
//  PickControllerProfile
//
//  UI thread. The user chose the profile one controller plays with. The rate
//  paddles return to center, since the old profile's position means nothing
//  to the new one, and the choice is saved with the global controller
//  settings, since it belongs to the controller on every machine.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PickControllerProfile (ControllerUnitKey unit, std::string profileName)
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
//  StartNewControllerProfile
//
//  New... at the foot of a player's profile section. Settings opens on the
//  Controllers page with Editing on that section's controller, and the New
//  Profile dialog comes up for it. Opened without the Controllers page's own
//  landing, which would move Editing to player one's controller first.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::StartNewControllerProfile (ControllerUnitKey unit)
{
    m_host.StartNewControllerProfile (unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyControllerSlotsChange
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

void ShellInput::ApplyControllerSlotsChange (
    const std::vector<std::wstring>  & notices,
    bool                               haveEntriesChanged,
    bool                               haveLastHoldersChanged)
{
    SyncGamePortAxisOwner();

    if (haveEntriesChanged)
    {
        SyncInputModeUi();
    }
    else if (haveLastHoldersChanged)
    {
        SaveControllerPrefs();
    }

    for (const std::wstring & notice : notices)
    {
        m_host.ShowNotice (notice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceControllerState
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

void ShellInput::TraceControllerState()
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

    state.arrowsJoystick       = m_arrowsJoystick;
    state.mousePaddle          = (m_pointerMode == InputMappingMode::Paddle);
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

    if (m_controllerService != nullptr)
    {
        snapshot  = m_controllerService->GetSnapshot();
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
            PickPlayerEntry (0, playerOne);
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
    bool    isBehindLive   = m_debugger.IsBehindLiveForUi();
    Byte    first          = 0;
    size_t  length         = 0;



    if (m_host.GetClipboard() == nullptr || !m_debugger.AllowCommand (IDM_EDIT_PASTE, std::string()))
    {
        return;
    }

    if (isBehindLive && !m_debugger.GetDivergenceGate().HasHeldPaste())
    {
        m_debugger.SetPasteLengthBeforeHold (m_host.GetClipboard()->GetPasteLength (first));
    }

    raisedLetters = m_host.GetClipboard()->PasteFromClipboard (m_host.GetMainWindow(), capsLockOn);
    length        = m_host.GetClipboard()->GetPasteLength (first);

    if (isBehindLive && length > m_debugger.GetPasteLengthBeforeHold() && m_machine.GetRefs().keyboard != nullptr)
    {
        m_debugger.GetDivergenceGate().HoldPaste (m_machine.GetRefs().keyboard->GetTypedLatch (first));
        m_debugger.PublishHeldInput();
    }

    if (raisedLetters)
    {
        m_host.ShowNotice (CapsLockTracker::kpszPasteRaisedNotice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClipPaddleCursorToClient
//
//  The pointer confined to the picture and parked in the middle -- the half of
//  "captured" the user can see. Kept apart from StartPaddleCapture because the
//  clip has to be laid on twice: once at the grab, and again when a cancel the
//  chrome's own layout raised has to be taken back (see OnCancelMode).
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ClipPaddleCursorToClient()
{
    RECT   client  = {};
    POINT  topLeft = {};
    POINT  botRt   = {};
    RECT   clip    = {};
    POINT  center  = {};



    if (m_host.GetMainWindow() == nullptr || !GetClientRect (m_host.GetMainWindow(), &client))
    {
        return;
    }

    topLeft.x = client.left;
    topLeft.y = client.top;
    botRt.x   = client.right;
    botRt.y   = client.bottom;
    ClientToScreen (m_host.GetMainWindow(), &topLeft);
    ClientToScreen (m_host.GetMainWindow(), &botRt);

    clip.left   = topLeft.x;
    clip.top    = topLeft.y;
    clip.right  = botRt.x;
    clip.bottom = botRt.y;
    ClipCursor (&clip);

    center.x = (client.right  - client.left) / 2;
    center.y = (client.bottom - client.top)  / 2;
    ClientToScreen (m_host.GetMainWindow(), &center);
    SetCursorPos (center.x, center.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartPaddleCapture
//
//  Hides and confines the cursor to the client area, parks it at center,
//  and begins relative tracking. No-op unless the mode is Paddle, the
//  window owns the foreground, and capture isn't already active. The
//  current (held) paddle position is pushed so a re-grab after an Esc
//  release resumes from where the dial was left.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::StartPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_pointerMode != InputMappingMode::Paddle, S_OK);
    BAIL_OUT_IF (m_paddleCaptured,                        S_OK);
    BAIL_OUT_IF (m_host.GetMainWindow() == nullptr,                       S_OK);
    BAIL_OUT_IF (GetForegroundWindow() != m_host.GetMainWindow(),         S_OK);

    m_paddleCaptured = true;

    //  THE BAND IS CLAIMED BEFORE THE MOUSE IS, and the order is the point.
    //  Docking resizes the picture, and Windows answers a resize by cancelling
    //  the mode of whoever holds the pointer -- so the layout runs while there
    //  is no grab to lose, and the grab is taken once it has settled. Traced
    //  in that order, no cancel arrives at all; the two guards below exist for
    //  the case where one does anyway.
    //
    //  NOT IN FULLSCREEN, where the band is zero however this ends: the bar
    //  hangs off the top edge there instead. A layout pass would be a settle
    //  of the whole scene plus a synchronous repaint for no change at all --
    //  and one caller of this grab is the fullscreen strip's own tick, which
    //  runs inside the frame the repaint would re-enter.
    m_host.ClaimCaptureBand();

    SetCapture (m_host.GetMainWindow());

    // Drive the per-thread ShowCursor counter negative so the arrow hides.
    while (ShowCursor (FALSE) >= 0)
    {
    }

    ClipPaddleCursorToClient();

    PushPaddlePosition();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopPaddleCapture
//
//  Releases the mouse capture and cursor clip, restores the cursor, and
//  clears the fire buttons so a held mouse button doesn't stick. No-op
//  when not captured. Leaves the input mode unchanged, so the dial holds
//  its position for a later re-grab.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::StopPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    m_paddleCaptured = false;

    ClipCursor (nullptr);

    if (GetCapture() == m_host.GetMainWindow())
    {
        ReleaseCapture();
    }

    while (ShowCursor (TRUE) < 0)
    {
    }

    PushPaddleButton (0, false);
    PushPaddleButton (1, false);

    //  ...and the picture takes the height back -- windowed, where it gave any
    //  up. See StartPaddleCapture for why fullscreen is left alone.
    m_host.ReleaseCaptureBand();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePaddleFromMouse
//
//  Maps one WM_MOUSEMOVE while paddle-captured: the motion relative to the
//  client center is scaled (s_kPaddleSweepInches of DPI-scaled travel =
//  full range) into the held paddle axes, then the cursor is snapped back
//  to center for unbounded relative motion. The zero-delta move our own
//  recenter generates is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::UpdatePaddleFromMouse (int xClient, int yClient)
{
    HRESULT  hr         = S_OK;
    RECT     client     = {};
    int      centerX    = 0;
    int      centerY    = 0;
    int      dx         = 0;
    int      dy         = 0;
    UINT     dpi        = 96;
    float    unitsPerPx = 0.0f;
    POINT    center     = {};



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    GetClientRect (m_host.GetMainWindow(), &client);
    centerX = (client.right  - client.left) / 2;
    centerY = (client.bottom - client.top)  / 2;
    dx      = xClient - centerX;
    dy      = yClient - centerY;

    // The SetCursorPos recenter below re-enters here with a zero delta.
    BAIL_OUT_IF (dx == 0 && dy == 0, S_OK);

    dpi = GetDpiForWindow (m_host.GetMainWindow());
    if (dpi == 0)
    {
        dpi = 96;
    }

    unitsPerPx    = s_kPaddleRange / (s_kPaddleSweepInches * (float) dpi);
    m_paddleAxisX = std::clamp (m_paddleAxisX + (float) dx * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);
    m_paddleAxisY = std::clamp (m_paddleAxisY + (float) dy * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);

    PushPaddlePosition();

    center.x = centerX;
    center.y = centerY;
    ClientToScreen (m_host.GetMainWindow(), &center);
    SetCursorPos (center.x, center.y);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddlePosition
//
//  Submits the held paddle axes to the game-port mixer as the mouse paddle's
//  contribution, keeping its buttons as they are.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PushPaddlePosition()
{
    Byte  x = (Byte) (m_paddleAxisX + 0.5f);
    Byte  y = (Byte) (m_paddleAxisY + 0.5f);



    m_mousePaddleContribution.paddle[0] = x;
    m_mousePaddleContribution.paddle[1] = y;
    m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddleButton
//
//  Submits a paddle fire button (0 or 1) to the game-port mixer as part of
//  the mouse paddle's contribution, keeping its axes as they are.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PushPaddleButton (int index, bool pressed)
{
    constexpr int  kMouseButtonCount = 2;
    HRESULT        hr                = S_OK;



    CBRA (index >= 0 && index < kMouseButtonCount);

    m_mousePaddleContribution.buttons.set (static_cast<size_t> (index), pressed);
    m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGuestMouseActive
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsGuestMouseActive() const
{
    // The fullscreen drive strip's hotkey summon "releases" the guest mouse
    // for the interaction; the FSM restores it when the strip hides.
    return m_pointerMode == InputMappingMode::Mouse && m_machine.GetMouse() != nullptr
        && m_mouseConnected && !m_host.IsGuestMouseSuppressed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGuestMouseLive
//
//  "Mouse-aware software is actually running", so Mouse mode stays invisible
//  at a BASIC prompt: hardware truth (the IOU's own interrupt enables, which
//  only the $C079 / $C05x programming sequence can set) rather than anything
//  garbage RAM could fake.
//
//  EITHER enable counts. The gate first read X/Y alone, on the assumption
//  that SETMOUSE programs ENBXY for every active mode -- it does not.
//  MousePaint's main app asks for mode $09 (mouse on + VBL interrupt, no
//  movement interrupt), so the firmware issues DISXY / ENVBL and X/Y stays
//  masked for as long as the app runs. That read as a dead mouse: the pointer
//  froze wherever the firmware left it and clicks went nowhere, while the
//  startup screen and tutorial -- different modes -- worked.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellInput::IsGuestMouseLive() const
{
    return IsGuestMouseActive() &&
           (m_machine.GetMouse()->AreXyInterruptsEnabled() || m_machine.GetMouse()->AreVblInterruptsEnabled());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateGuestMouseFromHost
//
//  Absolute host->guest mapping for //c Mouse mode. The mouse firmware owns
//  position and clamping, publishing both through the slot-7 screen holes:
//
//      position   $047F/$057F (X lo/hi)   $04FF/$05FF (Y lo/hi)
//      clamp min  $047D/$057D (X)         $04FD/$05FD (Y)
//      clamp max  $067D/$077D (X)         $06FD/$07FD (Y)
//
//  The host position's fraction across the viewport maps into the live
//  clamp window, and the delta from the firmware's current position is
//  queued as movement units. Self-correcting: any units the firmware
//  clamps away are re-derived from the holes on the next move. Sanity
//  checks make this a no-op until the guest app has initialized the mouse
//  firmware (pre-INITMOUSE holes are garbage). PeekByte reads the CPU's
//  memory array directly (same cross-thread pattern as screen scraping).
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::UpdateGuestMouseFromHost (int xPx, int yPx)
{
    bool                                  isGateOpen = false;
    std::optional<uint32_t>               target;
    std::shared_lock<std::shared_mutex>   gate;



    // Only Mouse mode with the guest's mouse on makes the pointer guest
    // input; anything else over the picture is the host's own pointer.
    if (!IsGuestMouseLive())
    {
        return;
    }

    // Through the glass when the CRT is shown, so only the picture counts.
    // Off the picture the guest mouse is released to wherever the firmware
    // last put it. The DEVICE projects the fraction into the firmware's live
    // clamp window on the CPU thread (AppleMouse::Tick -> RetargetFromHoles):
    // guest memory must not be read from the UI thread.
    target = m_host.MapPointerToPicture (xPx, yPx);

    // Behind live in reverse execution's history the mouse holds a recorded
    // position, so the pointer's place is held instead, until the guest's
    // mouse would move to it or the machine is live.
    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);

    if (!isGateOpen)
    {
        m_debugger.GetDivergenceGate().SetMouseTarget (target);
        m_debugger.PublishHeldInput();
    }
    else if (target.has_value())
    {
        m_machine.GetMouse()->SetHostTargetFraction (static_cast<uint16_t> (*target >> 16), static_cast<uint16_t> (*target & 0xFFFF));
    }
    else
    {
        m_machine.GetMouse()->ClearHostTarget();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadControllerPrefs
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

void ShellInput::LoadControllerPrefs()
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

    store.FromJson (m_host.GetPrefs().controllers, rejected);
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
        m_host.LoadMachineUiPrefs (doc, uiPrefs);
        entries   = MachineInputPrefs::ReadAdoptedPlayers (uiPrefs, lastHolders);
        isAdopted = true;
    }

    m_controllerService->SetLastHolders   (lastHolders);
    m_controllerService->SetPlayerEntries (entries);

    m_arrowsJoystick = m_controllerService->GetPlayerEntries()[0].kind == PlayerEntryKind::ArrowKeys;

    if (isAdopted)
    {
        SaveControllerPrefs();
    }

    if (!rejected.empty())
    {
        m_host.PostNotice (L"Some saved controller settings couldn't be read, so those settings were reset.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveControllerPrefs
//
//  Writes every controller model's settings, every unit's calibration, the
//  active profiles, the players' entries and the last holders into the
//  global prefs, and saves them only when that changed what they hold: an
//  automatic calibration that learned nothing new costs no write.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SaveControllerPrefs()
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
    controllers                 = store.ToJson (m_host.GetPrefs().controllers);

    if (JsonWriter::Write (controllers) == JsonWriter::Write (m_host.GetPrefs().controllers))
    {
        return;
    }

    m_host.GetPrefs().controllers = std::move (controllers);
    m_host.SavePrefs();
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

    if (m_controllerService != nullptr)
    {
        playerOne = m_controllerService->GetPlayerEntries()[0].kind;
    }

    m_arrowsJoystick = playerOne == PlayerEntryKind::ArrowKeys;

    if (playerOne == PlayerEntryKind::MousePaddle)
    {
        m_pointerMode = InputMappingMode::Paddle;
    }

    AdoptControllerForMachine (uiPrefs, machineId);

    // The mixer is thread-safe and writes on the UI thread, so handing the
    // axes over from here is safe on the CPU thread too.
    SyncGamePortAxisOwner();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptControllerForMachine
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

void ShellInput::AdoptControllerForMachine (const JsonValue * uiPrefs, const std::string & machineId)
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
    // From `machineId`, NOT from m_machine. On the switch path this runs
    // before the new config is adopted, so m_machine is still the machine
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



    SaveControllerPrefs();

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
//  MigrateJoyportAtLaunch
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

void ShellInput::MigrateJoyportAtLaunch (const JsonValue * uiPrefs)
{
    JoyportMigration  migration = PlayerModeRules::MigrateAdapter (m_hadSavedPlayerModes, m_host.GetPrefs().gamePortAdapter,
                                                                   uiPrefs, m_machine.GetJoyport() != nullptr);



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
        m_host.GetPrefs().gamePortAdapter.clear();
        m_host.SavePrefs();
    }

    ApplyJoyportToMachine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyJoyportToMachine
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

void ShellInput::ApplyJoyportToMachine()
{
    SiriusJoyport  * joyport    = m_machine.GetJoyport();
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
    m_gamePortSink->SetDivergenceGate (&m_debugger.GetDivergenceGate(), [this]
    {
        m_debugger.PublishHeldInput();
    });

    m_gamePortMixer.SetApplyThread (std::this_thread::get_id(), [hwnd = m_host.GetMainWindow()]
    {
        PostMessageW (hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
    });

    m_gamePortMixer.SetSink (m_gamePortSink.get());

    // Physical controllers. The backend is initialized on the controller
    // thread, which owns the window its device notifications arrive at, so
    // nothing here touches a device.
    m_controllerBackend = std::make_unique<Win32ControllerBackend>();
    m_controllerService = std::make_unique<ControllerInputService> (*m_controllerBackend, m_gamePortMixer);

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

            PostMessageW (m_host.GetMainWindow(), WM_APP_CONTROLLER_PICK, 0, 0);
        });

    // A connect or disconnect changes who owns the axes but tells the user
    // nothing, so it takes the same trip to the UI thread without a notice.
    m_controllerService->SetStateChangedFn ([this]
    {
        PostMessageW (m_host.GetMainWindow(), WM_APP_CONTROLLER_PICK, 0, 0);
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
//  ShellInput::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::Stop()
{
    m_controllerThread.reset();

    // What automatic calibration learned this session, while the service that
    // holds it still exists and nothing is reading into it.
    SaveControllerPrefs();

    m_controllerService.reset();
    m_controllerBackend.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::ApplyPendingControllerPick
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::ApplyPendingControllerPick()
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
//  ShellInput::SetControllersActive
//
//  Controllers are read while the emulator is the active app.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::SetControllersActive (bool active)
{
    if (m_controllerService != nullptr)
    {
        m_controllerService->SetActive (active);
    }
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
//  ShellInput::InstallControllerService
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::InstallControllerService (IControllerBackend & backend)
{
    m_controllerService = std::make_unique<ControllerInputService> (backend, m_gamePortMixer);
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
    SyncPaddleSourceList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInput::PressGuestMouseButton
//
//  Only once guest software has turned the mouse on, so clicks are not
//  swallowed at a BASIC prompt, and only over the picture: through the glass
//  when the CRT is on, else across the viewport. Behind live the press is
//  deferred until the guest reads the button; a release made behind live is
//  injected once the machine is live.
//
////////////////////////////////////////////////////////////////////////////////

void ShellInput::PressGuestMouseButton (int x, int y)
{
    std::shared_lock<std::shared_mutex>  gate;
    bool                                 isGateOpen = false;



    if (!IsGuestMouseLive() || !m_host.MapPointerToPicture (x, y).has_value())
    {
        return;
    }

    isGateOpen = m_machine.GetHostInputGate().TryEnter (gate);
    m_heldHostInputs.OnPress (HeldHostInputs::kMouseButton, isGateOpen);

    if (!isGateOpen)
    {
        m_debugger.HoldInputBehindLive (HeldInput::MousePress);
        return;
    }

    m_machine.GetMouse()->SetButton (true);
}
