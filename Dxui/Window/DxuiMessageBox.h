#pragma once

#include "Pch.h"


class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBox
//
//  A themed, DPI-aware modal message box drawn with Dxui instead of the Win32
//  MessageBox, so confirmation / notice popups match the app chrome (dark
//  theme, custom caption) rather than punching through a system dialog.
//
//  Deliberately shaped like Win32 MessageBox / MessageBoxEx: `uType` takes the
//  standard MB_* flags and the return value is the standard control id, so it
//  is a near drop-in replacement.
//
//    Button set (uType & MB_TYPEMASK):
//        MB_OK, MB_OKCANCEL, MB_YESNO, MB_YESNOCANCEL, MB_RETRYCANCEL
//    Icon (uType & MB_ICONMASK):
//        MB_ICONINFORMATION/ASTERISK, MB_ICONWARNING/EXCLAMATION,
//        MB_ICONERROR/HAND/STOP, MB_ICONQUESTION
//    Default button (uType & MB_DEFMASK):
//        MB_DEFBUTTON1 / MB_DEFBUTTON2 / MB_DEFBUTTON3
//    Returns:
//        IDOK / IDCANCEL / IDYES / IDNO / IDRETRY (IDCANCEL on Escape / close).
//
//  The one departure from MessageBox is the required `theme`: a Dxui window
//  needs a theme to render. `owner` supplies the modal parent (disabled while
//  the box is up) and the HINSTANCE (falling back to the process module when
//  `owner` is null). Must be called on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

int  DxuiMessageBox (HWND               owner,
                     const IDxuiTheme * theme,
                     const wchar_t    * text,
                     const wchar_t    * caption,
                     UINT               uType);
