#pragma once

#include "Pch.h"

#include "DialogDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShowStandaloneDialog
//
//  Spins up a one-shot D3D11 device + DialogPrimitive to display the
//  supplied dialog at startup, before EmulatorShell exists. The
//  device is torn down when the dialog returns. Returns the chosen
//  button's resultCode, or -1 on close-gesture. For dialogs invoked
//  after EmulatorShell is running, prefer EmulatorShell::ShowModalDialog.
//
//  `themeName` is the persisted GlobalUserPrefs::activeTheme value; it
//  is resolved to a CassoTheme via CassoTheme::ForName so startup
//  dialogs honour the user's theme choice. Unknown names fall back to
//  Skeuomorphic.
//
////////////////////////////////////////////////////////////////////////////////

int  ShowStandaloneDialog (HINSTANCE                hInstance,
                           HWND                     hwndOwner,
                           std::string_view         themeName,
                           const DialogDefinition & def);
