#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputState
//
//  The keyboard and pointer state that code reads outside of a message: a
//  key's state, its asynchronous state and the cursor's position. Each call
//  is the Win32 call of the same name until a test sets a reader, so a unit
//  test controls the state its code reads and doesn't read the desktop's.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiInputState
{
public:
    using KeyReader    = std::function<SHORT (int vk)>;
    using CursorReader = std::function<BOOL (POINT * outScreenPx)>;

    static SHORT  GetKeyState      (int vk)               { return s_readKey      ? s_readKey      (vk)          : ::GetKeyState      (vk); }
    static SHORT  GetAsyncKeyState (int vk)               { return s_readAsyncKey ? s_readAsyncKey (vk)          : ::GetAsyncKeyState (vk); }
    static BOOL   GetCursorPos     (POINT * outScreenPx)  { return s_readCursor   ? s_readCursor   (outScreenPx) : ::GetCursorPos     (outScreenPx); }

    //  Whether a key is down, from its state.
    static bool   IsKeyDown        (int vk)               { return (GetKeyState (vk) & kDownBit) != 0; }

    //  Null restores the Win32 call.
    static void   SetKeyReader      (KeyReader reader)     { s_readKey      = std::move (reader); }
    static void   SetAsyncKeyReader (KeyReader reader)     { s_readAsyncKey = std::move (reader); }
    static void   SetCursorReader   (CursorReader reader)  { s_readCursor   = std::move (reader); }

private:
    static constexpr SHORT  kDownBit = static_cast<SHORT> (0x8000);

    static inline KeyReader     s_readKey;
    static inline KeyReader     s_readAsyncKey;
    static inline CursorReader  s_readCursor;
};
