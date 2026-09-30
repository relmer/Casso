#include "Pch.h"

#include "SettingsSheetSize.h"

#include "Config/GlobalUserPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetInitialSizeDip
//
//  The maximum depends on the pages' content, which is not known until they
//  are laid out, so it is applied once the sheet exists; the monitor's work
//  area is applied as the window is created.
//
////////////////////////////////////////////////////////////////////////////////

SIZE SettingsSheetSize::GetInitialSizeDip (const GlobalUserPrefs & prefs, const SIZE & designDip, const SIZE & minDip)
{
    bool  isRemembered = prefs.settingsWidthDip > 0 && prefs.settingsHeightDip > 0;
    SIZE  sizeDip      = isRemembered ? SIZE { prefs.settingsWidthDip, prefs.settingsHeightDip } : designDip;



    return DxuiWindow::ClampSize (sizeDip, minDip, sizeDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryStoreSizeDip
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsSheetSize::TryStoreSizeDip (GlobalUserPrefs & prefs, const SIZE & sizeDip)
{
    bool  isSame = prefs.settingsWidthDip == sizeDip.cx && prefs.settingsHeightDip == sizeDip.cy;



    prefs.settingsWidthDip  = sizeDip.cx;
    prefs.settingsHeightDip = sizeDip.cy;

    return !isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PxToDip
//
////////////////////////////////////////////////////////////////////////////////

SIZE SettingsSheetSize::PxToDip (const SIZE & sizePx, UINT dpi)
{
    return SIZE { MulDiv (sizePx.cx, USER_DEFAULT_SCREEN_DPI, (int) dpi),
                  MulDiv (sizePx.cy, USER_DEFAULT_SCREEN_DPI, (int) dpi) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DipToPx
//
////////////////////////////////////////////////////////////////////////////////

SIZE SettingsSheetSize::DipToPx (const SIZE & sizeDip, UINT dpi)
{
    return SIZE { MulDiv (sizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI),
                  MulDiv (sizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI) };
}
