#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"


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
//  user types: "1. Type **IN#2** and press Return". Every line wraps at its
//  spaces, whether or not it has a mark.
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





//  A button of a box with buttons of its own: its label, an & before the
//  accelerator's letter, and the id the box returns when it is pressed.
struct DxuiMessageBoxButton
{
    std::wstring  label;
    int           id = 0;
};

//  The box with its own buttons, defaultId the default one's; only uType's
//  icon bits are used. Escape and the close box return IDCANCEL.
int  DxuiMessageBox (HWND                                      owner,
                     const IDxuiTheme                        * theme,
                     const wchar_t                           * text,
                     const wchar_t                           * caption,
                     UINT                                      uType,
                     const std::vector<DxuiMessageBoxButton>  & buttons,
                     int                                       defaultId);

//  The box with uType's buttons, MB_OK or MB_OKCANCEL, and a check box under
//  the text, such as a notice's box that turns it off; ioChecked is its state
//  before and after.
int  DxuiMessageBox (HWND                 owner,
                     const IDxuiTheme   * theme,
                     const wchar_t      * text,
                     const wchar_t      * caption,
                     UINT                 uType,
                     const std::wstring & checkLabel,
                     bool               & ioChecked);

//  How the box's window is created for owner: centered on the owner, on the
//  owner's monitor and at that monitor's DPI, as every other dialog is.
DxuiWindow::CreateParams  DxuiMakeMessageBoxParams (HWND owner, const wchar_t * caption, int heightDip);

//  The system box type with exactly these buttons, MB_YESNO for Yes and No
//  and so on, or UINT_MAX for buttons that are no system set.
UINT  DxuiGetSystemMessageBoxType (const std::vector<DxuiMessageBoxButton> & buttons);





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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPlacedTextRun
//
//  A piece of a run, a word and the spaces after it, at its place in the
//  wrapped text: `x` from the left edge, on row `row`.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPlacedTextRun
{
    std::wstring  text;
    bool          semiBold = false;
    float         x        = 0.0f;
    int           row      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLayOutTextRuns
//
//  The lines' runs wrapped at `widthPx`: a word that would run past the right
//  edge starts the next row, and each line starts a new row. `measure`
//  returns a piece's width in its weight, trailing spaces included. Returns
//  the number of rows; an empty line takes one.
//
////////////////////////////////////////////////////////////////////////////////

using DxuiMeasureTextRunFn = std::function<float (const std::wstring & text, bool semiBold)>;

int  DxuiLayOutTextRuns (const std::vector<std::vector<DxuiTextRun>> & lines,
                         float                                         widthPx,
                         const DxuiMeasureTextRunFn                  & measure,
                         std::vector<DxuiPlacedTextRun>              & outRuns);
