#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OptionsDialog
//
//  View -> Options... modal dialog (spec 005-disk-ii-audio FR-006).
//  Exposes the global "Drive Audio" toggle and the per-machine Disk
//  II mechanism dropdown (Shugart SA400 / Alps 2124A). The dialog is
//  built procedurally with DialogBoxIndirectParam so it carries no
//  .rc footprint.
//
////////////////////////////////////////////////////////////////////////////////

class OptionsDialog
{
public:
    // Show the dialog modally. On OK, `outDriveAudioEnabled` is set
    // to the new checkbox state, `outMechanism` is set to L"Shugart"
    // or L"Alps", and S_OK is returned. On Cancel, S_FALSE is
    // returned and the out parameters are left unmodified.
    static HRESULT Show (
        HWND            hwndParent,
        HINSTANCE       hInstance,
        bool            currentDriveAudioEnabled,
        const wstring & currentMechanism,
        bool          & outDriveAudioEnabled,
        wstring       & outMechanism);
};
