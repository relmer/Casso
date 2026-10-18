#include "Pch.h"

#include "Ui/Debugger/StopFocus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StopFocus::OnSnapshot
//
//  True at the snapshot where a running machine stops, unless the console
//  received the last input.
//
////////////////////////////////////////////////////////////////////////////////

bool StopFocus::OnSnapshot (bool isPaused)
{
    bool  isStop = isPaused && !m_wasPaused;



    m_wasPaused = isPaused;

    return isStop && !m_isConsoleLast;
}
