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
        if (chord.vk == vk && chord.ctrl == ctrl && chord.alt == alt && chord.shift == shift)
        {
            outCommandId = chord.id;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap::GetChordText
//
//  Modifiers in the order Windows writes them, Ctrl then Alt then Shift.
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

        text += chord.ctrl  ? L"Ctrl+"  : L"";
        text += chord.alt   ? L"Alt+"   : L"";
        text += chord.shift ? L"Shift+" : L"";
        text += GetKeyName (chord.vk);
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
