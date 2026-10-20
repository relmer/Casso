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
//  `buttonLabels`, when given, relabels the set's buttons in order -- an
//  "Overwrite" and "Cancel" for MB_OKCANCEL, say -- and each still returns
//  the id its place in the set does.
//
//  `text` may mark runs as semibold between pairs of `**`, as for text the
//  user types: "1. Type **IN#2** and press Return". A line with a mark is
//  drawn on one line, unwrapped; a line without one wraps.
//
//  The one departure from MessageBox is the required `theme`: a Dxui window
//  needs a theme to render. `owner` supplies the modal parent (disabled while
//  the box is up) and the HINSTANCE (falling back to the process module when
//  `owner` is null). Must be called on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

int  DxuiMessageBox (HWND                                 owner,
                     const IDxuiTheme                   * theme,
                     const wchar_t                      * text,
                     const wchar_t                      * caption,
                     UINT                                 uType,
                     const std::vector<const wchar_t *> & buttonLabels = {});





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRun
//
//  A piece of a message box's line in one weight.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextRun
{
    std::wstring  text;
    bool          semiBold = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiParseTextRuns
//
//  The text's lines, split at each newline, each as its runs between `**`
//  marks; the marks are dropped. An unpaired mark sets the rest of its line
//  semibold. A line with no marks is one run, and an empty line has none.
//
////////////////////////////////////////////////////////////////////////////////

void  DxuiParseTextRuns (const std::wstring & text, std::vector<std::vector<DxuiTextRun>> & outLines);
