#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  StopFocus
//
//  Where the focus goes when the machine stops. Focus stays in the console
//  only when the console received the last input: a command typed there, or
//  a click in it. A click or key anywhere else, or a switch to another
//  window, clears that, and at the stop the focus moves from the console to
//  the disassembly.
//
////////////////////////////////////////////////////////////////////////////////

class StopFocus
{
public:
    void  OnInput    (bool isConsole)  { m_isConsoleLast = isConsole; }
    bool  OnSnapshot (bool isPaused);

private:
    bool  m_isConsoleLast = false;
    bool  m_wasPaused     = true;
};
