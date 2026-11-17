#include "Pch.h"

#include "Core/DxuiInputState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputState readers
//
//  Empty until a test sets one; each call is then the Win32 call.
//
////////////////////////////////////////////////////////////////////////////////

DxuiInputState::KeyReader     DxuiInputState::s_readKey;
DxuiInputState::KeyReader     DxuiInputState::s_readAsyncKey;
DxuiInputState::CursorReader  DxuiInputState::s_readCursor;
