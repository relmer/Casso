#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyStroke
//
//  One key with its modifiers. A vk of zero is no key.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiKeyStroke
{
    WPARAM  vk    = 0;
    bool    ctrl  = false;
    bool    alt   = false;
    bool    shift = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyChord
//
//  One key with its modifiers, and the application command it stands for. The
//  fields are the ones an application's own key table already carries, so a
//  table written for one converts to the other without translation.
//
//  A chord of two keys, Visual Studio's Ctrl+R, F11, gives its first key as
//  prefix; the key and modifiers before it are the second key. A chord of one
//  key leaves prefix empty.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiKeyChord
{
    WPARAM         vk;
    bool           ctrl;
    bool           alt;
    bool           shift;
    int            id;
    DxuiKeyStroke  prefix = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMatch
//
//  What a key-down is to a key map. Command: it completes a chord. Prefix: it
//  is the first key of a two-key chord, so the next key-down completes it or
//  not. Modifier: Ctrl, Alt or Shift pressed alone while a prefix waits,
//  which neither completes nor abandons the chord. None: nothing.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiKeyMatch
{
    None,
    Command,
    Prefix,
    Modifier,
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

    //  The command a one-key chord stands for. The first chord that matches
    //  wins, and the modifiers must match exactly: Shift+F11 is not F11.
    bool  TryTranslate (WPARAM vk, bool ctrl, bool alt, bool shift, int & outCommandId) const;

    //  The same for a key-down that may be either key of a two-key chord:
    //  pending is the first key, when one was pressed just before. With a
    //  first key pending only a chord that starts with it can match.
    DxuiKeyMatch  Match (const DxuiKeyStroke & stroke, const std::optional<DxuiKeyStroke> & pending, int & outCommandId) const;

    //  `Shift+F11` or `Ctrl+R, F11`, for a menu row or a tooltip: every chord
    //  bound to the command, joined by " or ", or empty when none is.
    std::wstring  GetChordText (int commandId) const;

private:
    static std::wstring  GetKeyName    (WPARAM vk);
    static std::wstring  GetStrokeText (const DxuiKeyStroke & stroke);
    static bool          IsSameStroke  (const DxuiKeyStroke & a, const DxuiKeyStroke & b);
    static bool          IsModifierKey (WPARAM vk);

    std::wstring               m_name;
    std::vector<DxuiKeyChord>  m_chords;
};
