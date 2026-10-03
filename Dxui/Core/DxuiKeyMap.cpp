#include "Pch.h"

#include "DxuiKeyMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::DxuiKeyMap
//
////////////////////////////////////////////////////////////////////////////////

DxuiKeyMap::DxuiKeyMap (std::wstring name, std::span<const DxuiKeyChord> chords) :
    m_name   (std::move (name)),
    m_chords (chords.begin(), chords.end())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::TryTranslate
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiKeyMap::TryTranslate (WPARAM vk, bool ctrl, bool alt, bool shift, int & outCommandId) const
{
    for (const DxuiKeyChord & chord : m_chords)
    {
        if (chord.prefix.vk == 0 && chord.vk == vk && chord.ctrl == ctrl && chord.alt == alt && chord.shift == shift)
        {
            outCommandId = chord.id;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::Match
//
//  A one-key chord is matched before a prefix, so a key that is both is the
//  command; with a first key pending, only a second key is looked for.
//
////////////////////////////////////////////////////////////////////////////////

DxuiKeyMatch DxuiKeyMap::Match (
    const DxuiKeyStroke                 & stroke,
    const std::optional<DxuiKeyStroke>  & pending,
    int                                 & outCommandId) const
{
    DxuiKeyStroke  second;



    if (pending.has_value() && IsModifierKey (stroke.vk))
    {
        return DxuiKeyMatch::Modifier;
    }

    for (const DxuiKeyChord & chord : m_chords)
    {
        second = DxuiKeyStroke { chord.vk, chord.ctrl, chord.alt, chord.shift };

        if (!IsSameStroke (second, stroke))
        {
            continue;
        }

        if (pending.has_value() ? IsSameStroke (chord.prefix, *pending) : chord.prefix.vk == 0)
        {
            outCommandId = chord.id;
            return DxuiKeyMatch::Command;
        }
    }

    for (const DxuiKeyChord & chord : m_chords)
    {
        if (!pending.has_value() && chord.prefix.vk != 0 && IsSameStroke (chord.prefix, stroke))
        {
            return DxuiKeyMatch::Prefix;
        }
    }

    return DxuiKeyMatch::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::GetChordText
//
//  A two-key chord is written with a comma between its keys: Ctrl+R, F11.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiKeyMap::GetChordText (int commandId) const
{
    std::wstring  text;



    for (const DxuiKeyChord & chord : m_chords)
    {
        if (chord.id != commandId)
        {
            continue;
        }

        if (chord.prefix.vk != 0)
        {
            text = GetStrokeText (chord.prefix) + L", ";
        }

        text += GetStrokeText (DxuiKeyStroke { chord.vk, chord.ctrl, chord.alt, chord.shift });
        break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::GetKeyName
//
//  The name a menu shows for a key. Letters and digits are their own
//  character; the rest are the names Windows menus use.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiKeyMap::GetKeyName (WPARAM vk)
{
    static constexpr WPARAM  kFirstFunctionKey = VK_F1;
    static constexpr WPARAM  kLastFunctionKey  = VK_F24;
    std::wstring             name;



    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
    {
        name = std::wstring (1, (wchar_t) vk);
    }
    else if (vk >= kFirstFunctionKey && vk <= kLastFunctionKey)
    {
        name = L"F" + std::to_wstring (vk - kFirstFunctionKey + 1);
    }
    else
    {
        switch (vk)
        {
        case VK_SPACE:     name = L"Space";     break;
        case VK_RETURN:    name = L"Enter";     break;
        case VK_ESCAPE:    name = L"Esc";       break;
        case VK_TAB:       name = L"Tab";       break;
        case VK_BACK:      name = L"Backspace"; break;
        case VK_DELETE:    name = L"Del";       break;
        case VK_INSERT:    name = L"Ins";       break;
        case VK_HOME:      name = L"Home";      break;
        case VK_END:       name = L"End";       break;
        case VK_PRIOR:     name = L"PgUp";      break;
        case VK_NEXT:      name = L"PgDn";      break;
        case VK_UP:        name = L"Up";        break;
        case VK_DOWN:      name = L"Down";      break;
        case VK_LEFT:      name = L"Left";      break;
        case VK_RIGHT:     name = L"Right";     break;
        case VK_PAUSE:     name = L"Pause";     break;
        case VK_OEM_PLUS:  name = L"+";         break;
        case VK_OEM_MINUS: name = L"-";         break;
        default:           name = L"Key " + std::to_wstring (vk); break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::GetStrokeText
//
//  Modifiers in the order Windows writes them, Ctrl then Alt then Shift.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiKeyMap::GetStrokeText (const DxuiKeyStroke & stroke)
{
    std::wstring  text;



    text += stroke.ctrl  ? L"Ctrl+"  : L"";
    text += stroke.alt   ? L"Alt+"   : L"";
    text += stroke.shift ? L"Shift+" : L"";
    text += GetKeyName (stroke.vk);
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::IsSameStroke
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiKeyMap::IsSameStroke (const DxuiKeyStroke & a, const DxuiKeyStroke & b)
{
    return a.vk == b.vk && a.ctrl == b.ctrl && a.alt == b.alt && a.shift == b.shift;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::IsModifierKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiKeyMap::IsModifierKey (WPARAM vk)
{
    switch (vk)
    {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
        return true;

    default:
        return false;
    }
}





