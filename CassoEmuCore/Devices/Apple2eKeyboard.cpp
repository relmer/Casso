#include "Pch.h"

#include "Apple2eKeyboard.h"
#include "Apple2eSoftSwitchBank.h"
#include "AppleMouse.h"
#include "AppleSpeaker.h"
#include "IInputEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eKeyboard
//
////////////////////////////////////////////////////////////////////////////////

Apple2eKeyboard::Apple2eKeyboard (MemoryBus * bus)
    : AppleKeyboard(),
      m_bus         (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Phase 6 / T060 / T061. The bus range $C000-$C063 covers more than the
//  keyboard logically owns; addresses outside the keyboard's logical
//  scope are forwarded to the canonical sibling device. The bank's
//  read-only status path enforces strobe-clear isolation (audit §1.2):
//  ONLY $C010 clears the strobe.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2eKeyboard::Read (Word address)
{
    Byte  value = 0;



    // Everything the soft-switch bank owns, forwarded identically:
    //   $C00C-$C00F  80COL / ALTCHARSET
    //   $C011-$C01F  status reads (T061 ownership split)
    //   $C028        //c ROM-bank flip-flop (ROMBANK), which toggles the
    //                visible firmware bank on any access; unused on the //e,
    //                where the sibling no-ops with no ROM-bank switch attached
    //   $C050-$C05F  video display switches
    bool  isSoftSwitch = (address >= 0xC00C && address <= 0xC00F)
                         || (address >= 0xC011 && address <= 0xC01F)
                         || (address == 0xC028)
                         || (address >= 0xC050 && address <= 0xC05F);


    // Each arm carries its own sibling test rather than sharing one up front:
    // with the sibling absent the address must keep falling through, and for
    // $C00C-$C00F that means reaching the base keyboard below.
    if (isSoftSwitch && m_softSwitchSibling != nullptr)
    {
        value = m_softSwitchSibling->Read (address);
    }
    else if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        // Speaker click.
        value = m_speakerSibling->Read (address);
    }
    else if (address == 0xC048 && m_mouse != nullptr)
    {
        // //c RSTXY: any access clears the mouse movement-interrupt latches.
        // No data behind the address — the read still returns 0.
        m_mouse->AccessRstXY();
    }
    else if (address == 0xC061)
    {
        // Open Apple (bit 7).
        value = m_openApple.load (memory_order_acquire) ? 0x80 : 0x00;
        EmitButtonRead (address, value);
    }
    else if (address == 0xC062)
    {
        // Closed Apple (bit 7).
        value = m_closedApple.load (memory_order_acquire) ? 0x80 : 0x00;
        EmitButtonRead (address, value);
    }
    else if (address == 0xC063)
    {
        // Mouse button on the //c (ACTIVE LOW; the //c wires the button where
        // the //e had its shift-key mod); Shift (bit 7) on the //e.
        value = (m_mouse != nullptr)
                    ? m_mouse->ReadButton()
                    : (m_shift.load (memory_order_acquire) ? 0x80 : 0x00);
        EmitButtonRead (address, value);
    }
    else if (address == kwEightyColumnSwitch && m_apple2cMode.load (memory_order_acquire))
    {
        // $C060 (RD80SW): the //c 80/40 case switch, bit 7. A switch pressed
        // in (down) reads bit 7 set (0x80) = 80 columns; a switch out (up)
        // reads bit 7 clear (0x00) = 40 columns (Apple TIL02094, matching the
        // constants). On the //e there is no device here — $C060 stays the
        // floating-bus 0.
        value = m_eightyColSwitchIn.load (memory_order_acquire) ? kEightyColSwitchIn
                                                                : kEightyColSwitchOut;
    }
    else if (address <= 0xC010)
    {
        // $C000-$C00B (keyboard data) and $C010 (strobe-clear) belong to the
        // base AppleKeyboard. Other unowned addresses ($C020-$C02F,
        // $C040-$C04F, $C060) keep the 0 — no device behind them on a //e.
        value = AppleKeyboard::Read (address);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitButtonRead
//
//  CPU thread. Coalesced emit for a guest read of $C061-$C063: fires only
//  when that button's returned byte (bit 7 = pressed) changed since the
//  last emit, so a tight button-poll loop yields one event per press /
//  release edge.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::EmitButtonRead (Word address, Byte value)
{
    IInputEventSink * sink = InputSink();
    int               idx  = static_cast<int> (address - kFirstButtonAddress);



    // No sink, or the same byte we last reported: either way there is no
    // edge to announce, which is what keeps a tight poll loop from flooding.
    if (sink != nullptr && m_lastEmittedButton[idx] != value)
    {
        m_lastEmittedButton[idx] = value;
        sink->OnButtonRead (address, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOpenApple
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::SetOpenApple (bool pressed)
{
    m_openApple.store (pressed, memory_order_release);
    EmitHostButton (0, pressed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClosedApple
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::SetClosedApple (bool pressed)
{
    m_closedApple.store (pressed, memory_order_release);
    EmitHostButton (1, pressed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitHostButton
//
//  Host UI thread. Coalesced emit for a host-set joystick button: fires
//  only when the staged button state changed since the last host-input emit.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::EmitHostButton (int index, bool pressed)
{
    IInputEventSink * sink  = InputSink();
    int               value = pressed ? 1 : 0;



    // Same coalescing rule as EmitButtonRead, on the host side.
    if (sink != nullptr && m_lastEmittedHostButton[index] != value)
    {
        m_lastEmittedHostButton[index] = value;
        sink->OnHostButton (index, pressed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyPressRaw
//
//  IIe keyboard supports lowercase — don't force uppercase.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::KeyPressRaw (Byte asciiChar)
{
    KeyPress (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapTypedChar
//
//  Route one physical keystroke through the //c keyboard-layout switch. When
//  the machine is a //c and the keyboard switch is engaged (Dvorak), the
//  character the host produced on a QWERTY key is remapped to the character
//  the Dvorak encoder would have produced for that same physical key. In every
//  other case the character passes through unchanged.
//
//  The remap only makes sense when the HOST layout is QWERTY. If the host is
//  itself Dvorak, the character we received is already the one the user
//  intended, so remapping would double-translate; we therefore leave the
//  stream untouched whenever the host is Dvorak, regardless of the switch.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2eKeyboard::MapTypedChar (Byte ascii) const
{
    bool  remaps = m_apple2cMode.load          (memory_order_acquire)
                   && m_keyboardSwitchDvorak.load (memory_order_acquire)
                   && !m_hostKeyboardDvorak.load  (memory_order_acquire);



    return remaps ? QwertyToDvorak (ascii) : ascii;
}





////////////////////////////////////////////////////////////////////////////////
//
//  US-QWERTY to US-Dvorak character map
//
//  One row per physical key, unshifted and shifted forms side by side, laid
//  out in the keyboard's own row order so a line reads as the key it
//  describes. Anything absent -- digits, whitespace, every control code --
//  sits on the same physical key in both layouts and passes through.
//
////////////////////////////////////////////////////////////////////////////////

struct DvorakKey
{
    char  qwerty;
    char  dvorak;
};

static constexpr DvorakKey  s_kDvorakMap[] =
{
    // Top letter row: q w e r t y u i o p [ ]
    { 'q' , '\'' },  { 'Q' , '"'  },
    { 'w' , ','  },  { 'W' , '<'  },
    { 'e' , '.'  },  { 'E' , '>'  },
    { 'r' , 'p'  },  { 'R' , 'P'  },
    { 't' , 'y'  },  { 'T' , 'Y'  },
    { 'y' , 'f'  },  { 'Y' , 'F'  },
    { 'u' , 'g'  },  { 'U' , 'G'  },
    { 'i' , 'c'  },  { 'I' , 'C'  },
    { 'o' , 'r'  },  { 'O' , 'R'  },
    { 'p' , 'l'  },  { 'P' , 'L'  },
    { '[' , '/'  },  { '{' , '?'  },
    { ']' , '='  },  { '}' , '+'  },

    // Home row: a s d f g h j k l ; '
    { 'a' , 'a'  },  { 'A' , 'A'  },
    { 's' , 'o'  },  { 'S' , 'O'  },
    { 'd' , 'e'  },  { 'D' , 'E'  },
    { 'f' , 'u'  },  { 'F' , 'U'  },
    { 'g' , 'i'  },  { 'G' , 'I'  },
    { 'h' , 'd'  },  { 'H' , 'D'  },
    { 'j' , 'h'  },  { 'J' , 'H'  },
    { 'k' , 't'  },  { 'K' , 'T'  },
    { 'l' , 'n'  },  { 'L' , 'N'  },
    { ';' , 's'  },  { ':' , 'S'  },
    { '\'', '-'  },  { '"' , '_'  },

    // Bottom row: z x c v b n m , . /
    { 'z' , ';'  },  { 'Z' , ':'  },
    { 'x' , 'q'  },  { 'X' , 'Q'  },
    { 'c' , 'j'  },  { 'C' , 'J'  },
    { 'v' , 'k'  },  { 'V' , 'K'  },
    { 'b' , 'x'  },  { 'B' , 'X'  },
    { 'n' , 'b'  },  { 'N' , 'B'  },
    { 'm' , 'm'  },  { 'M' , 'M'  },
    { ',' , 'w'  },  { '<' , 'W'  },
    { '.' , 'v'  },  { '>' , 'V'  },
    { '/' , 'z'  },  { '?' , 'Z'  },

    // Number-row tail: the '-' and '=' keys carry the bracket pair.
    { '-' , '['  },  { '_' , '{'  },
    { '=' , ']'  },  { '+' , '}'  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  QwertyToDvorak
//
//  Maps a character produced on a US-QWERTY key to the character the US Dvorak
//  Simplified Keyboard produces on the same physical key — the exact remap the
//  //c keyboard switch performs in its encoder. Digits, whitespace, and every
//  control code pass through untouched (their key positions are identical in
//  both layouts). Both letter cases and the shifted punctuation are covered.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2eKeyboard::QwertyToDvorak (Byte ascii)
{
    Byte  mapped = ascii;



    for (const DvorakKey & k : s_kDvorakMap)
    {
        if (k.qwerty == (char) ascii)
        {
            mapped = (Byte) k.dvorak;
            break;
        }
    }

    return mapped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Phase 6 / T060 strobe-clear isolation: ONLY $C010 (read OR write)
//  clears the strobe. $C011-$C01F writes are routed to the soft-switch
//  bank (status-read mirrors; the bank's Write is a no-op) and MUST NOT
//  fall through to the base which would clear the strobe.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::Write (Word address, Byte value)
{
    // The soft-switch bank's addresses. $C028 is the //c ROM-bank flip-flop,
    // which toggles on a write too (no-op on the //e). Note $C010 is NOT here:
    // it is tested first below so a strobe-clear write always reaches the base
    // even though $C000-$C00F otherwise belongs to the bank.
    bool  isSoftSwitch = (address >= 0xC000 && address <= 0xC00F)
                         || (address >= 0xC011 && address <= 0xC01F)
                         || (address == 0xC028)
                         || (address >= 0xC050 && address <= 0xC05F);



    if (address == 0xC010)
    {
        AppleKeyboard::Write (address, value);
    }
    else if (isSoftSwitch && m_softSwitchSibling != nullptr)
    {
        m_softSwitchSibling->Write (address, value);
    }
    else if (address >= 0xC030 && address <= 0xC03F && m_speakerSibling != nullptr)
    {
        m_speakerSibling->Write (address, value);
    }
    else if (address == 0xC048 && m_mouse != nullptr)
    {
        // //c RSTXY: any access — the firmware acks with STA $C048.
        m_mouse->AccessRstXY();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::Reset()
{
    AppleKeyboard::Reset();

    m_openApple.store   (false, memory_order_release);
    m_closedApple.store (false, memory_order_release);
    m_shift.store       (false, memory_order_release);

    m_lastEmittedButton[0] = -1;
    m_lastEmittedButton[1] = -1;
    m_lastEmittedButton[2] = -1;

    m_lastEmittedHostButton[0] = -1;
    m_lastEmittedHostButton[1] = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Real //e behavior: a CPU /RESET pulse does NOT lift the user's finger
//  off Open Apple, Closed Apple, or Shift. The firmware reads $C061 at
//  reset specifically to decide warm-vs-autoboot, so clearing those
//  modifiers here would break Ctrl+Open-Apple+Reset autoboot. Only the
//  latched-character byte at $C000 needs to clear (handled by the base
//  class) so a stale typeahead doesn't survive the reset.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2eKeyboard::SoftReset()
{
    // Bypass the virtual chain: AppleKeyboard::SoftReset -> Reset() would
    // dispatch to Apple2eKeyboard::Reset and clobber the modifiers we
    // need to preserve. Just clear the latched-character byte directly.
    AppleKeyboard::Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> Apple2eKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<Apple2eKeyboard> (&bus);
}
