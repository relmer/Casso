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

bool DxuiSystemSettings::ReadFlag (UINT action, bool fallback)
{
    BOOL  value = FALSE;



    if (!SystemParametersInfoW (action, 0, &value, 0))
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

int DxuiSystemSettings::ReadUint (UINT action, int fallback)
{
    UINT  value = 0;



    if (!SystemParametersInfoW (action, 0, &value, 0) || value == 0)
    {
        return fallback;
    }

    return (int) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings::Refresh
//
//  Re-reads every setting. Cheap enough to call on `WM_SETTINGCHANGE`
//  without filtering on which parameter changed.
//
//  The wheel is the one with a sentinel rather than a count: "one screen at
//  a time" reports `WHEEL_PAGESCROLL`, which is UINT_MAX and would scroll
//  four billion lines if it were taken at face value.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemSettings::Refresh()
{
    UINT  wheelLines = 0;
    UINT  wheelChars = 0;



    m_animations   = ReadFlag (SPI_GETCLIENTAREAANIMATION, kDefaultAnimations);
    m_keyboardCues = ReadFlag (SPI_GETKEYBOARDCUES,        kDefaultKeyboardCues);

    m_menuShowDelayMs   = ReadUint (SPI_GETMENUSHOWDELAY, kDefaultMenuShowDelayMs);
    m_messageDurationMs = ReadUint (SPI_GETMESSAGEDURATION, kDefaultMessageSeconds) * kMsPerSecond;

    m_wheelLines = kDefaultWheelLines;
    m_wheelChars = kDefaultWheelChars;

    if (SystemParametersInfoW (SPI_GETWHEELSCROLLLINES, 0, &wheelLines, 0))
    {
        m_wheelLines = (wheelLines == WHEEL_PAGESCROLL) ? kWheelPageScroll : (int) wheelLines;
    }

    if (SystemParametersInfoW (SPI_GETWHEELSCROLLCHARS, 0, &wheelChars, 0))
    {
        m_wheelChars = (wheelChars == WHEEL_PAGESCROLL) ? kWheelPageScroll : (int) wheelChars;
    }
}
