#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyChord
//
//  One key with its modifiers, and the application command it stands for. The
//  fields are the ones an application's own key table already carries, so a
//  table written for one converts to the other without translation.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiKeyChord
{
    WPARAM  vk;
    bool    ctrl;
    bool    alt;
    bool    shift;
    int     id;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMap
//
//  A named table from key chord to an application's command id. A DxuiWindow
//  holds one, borrowed, and consults it for a key-down that no control claimed;
//  an application with several keyboard schemes builds one map per scheme and
//  swaps which one the window holds.
//
//  The library defines no application commands. The standard focus-following
//  ones (Copy, Undo and the rest) are DxuiCommandRouter's; everything in a map
//  belongs to the application, under its own ids.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiKeyMap
{
public:
    DxuiKeyMap () = default;
    DxuiKeyMap (std::wstring name, std::span<const DxuiKeyChord> chords);

    const std::wstring &                GetName   () const { return m_name; }
    const std::vector<DxuiKeyChord> &  GetChords () const { return m_chords; }

    //  The command a chord stands for. The first chord that matches wins, and
    //  the modifiers must match exactly: Shift+F11 is not F11.
    bool  TryTranslate (WPARAM vk, bool ctrl, bool alt, bool shift, int & outCommandId) const;

    //  `Shift+F11`, for a menu row or a tooltip: the first chord bound to the
    //  command, or empty when none is.
    std::wstring  GetChordText (int commandId) const;

private:
    static std::wstring  GetKeyName (WPARAM vk);

    std::wstring               m_name;
    std::vector<DxuiKeyChord>  m_chords;
};
