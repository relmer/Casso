#include "Pch.h"

#include "DxuiSystemSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::Instance
//
////////////////////////////////////////////////////////////////////////////////

DxuiSystemSettings & DxuiSystemSettings::Instance()
{
    static DxuiSystemSettings  s_instance;



    return s_instance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::DxuiSystemSettings
//
////////////////////////////////////////////////////////////////////////////////

DxuiSystemSettings::DxuiSystemSettings()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::ReadFlag
//
//  One BOOL-valued system parameter. A failed query keeps the caller's
//  fallback, which is the Windows default for that parameter.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSystemSettings::ReadFlag (UINT action, bool fallback) const
{
    BOOL  value = FALSE;



    if (!m_pfnRead (action, 0, &value, 0))
    {
        return fallback;
    }

    return value != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::ReadUint
//
//  One UINT-valued system parameter, as an int. A zero is treated as a
//  failure: every parameter read through here is a duration or a count for
//  which zero has no meaning, and a zero delay or zero line count would read
//  as "instant" rather than as "unset".
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSystemSettings::ReadUint (UINT action, int fallback) const
{
    UINT  value = 0;



    if (!m_pfnRead (action, 0, &value, 0) || value == 0)
    {
        return fallback;
    }

    return (int) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::ReadWheel
//
//  One wheel-scroll parameter. "One screen at a time" reports
//  `WHEEL_PAGESCROLL`, which is UINT_MAX and would scroll four billion lines
//  if it were taken at face value, so it maps to `kWheelPageScroll`.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSystemSettings::ReadWheel (UINT action, int fallback) const
{
    UINT  value = 0;



    if (!m_pfnRead (action, 0, &value, 0))
    {
        return fallback;
    }

    return (value == WHEEL_PAGESCROLL) ? kWheelPageScroll : (int) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::Refresh
//
//  Re-reads every setting. Cheap enough to call on `WM_SETTINGCHANGE`
//  without filtering on which parameter changed; DxuiHwndSource does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemSettings::Refresh()
{
    m_animations    = ReadFlag (SPI_GETCLIENTAREAANIMATION, kDefaultAnimations);
    m_keyboardCues  = ReadFlag (SPI_GETKEYBOARDCUES,        kDefaultKeyboardCues);
    m_menuAnimation = ReadFlag (SPI_GETMENUANIMATION,       kDefaultMenuAnimation);
    m_menuFade      = ReadFlag (SPI_GETMENUFADE,            kDefaultMenuFade);

    m_menuShowDelayMs   = ReadUint (SPI_GETMENUSHOWDELAY,   kDefaultMenuShowDelayMs);
    m_messageDurationMs = ReadUint (SPI_GETMESSAGEDURATION, kDefaultMessageSeconds) * kMsPerSecond;

    m_wheelLines = ReadWheel (SPI_GETWHEELSCROLLLINES, kDefaultWheelLines);
    m_wheelChars = ReadWheel (SPI_GETWHEELSCROLLCHARS, kDefaultWheelChars);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::SetParameterReader
//
//  Replaces the system-parameters query. Takes effect at the next Refresh,
//  so a test installs its reader, refreshes, and restores with nullptr.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemSettings::SetParameterReader (ParameterReader reader)
{
    m_pfnRead = (reader != nullptr) ? reader : SystemParametersInfoW;
}




