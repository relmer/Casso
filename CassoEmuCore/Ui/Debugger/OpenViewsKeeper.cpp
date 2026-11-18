#include "Pch.h"

#include "Ui/Debugger/OpenViewsKeeper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsKeeper::BeginRestore
//
////////////////////////////////////////////////////////////////////////////////

void OpenViewsKeeper::BeginRestore (const std::string & saved)
{
    m_isRestored = true;
    m_saved      = saved;
    m_settling   = saved.empty() ? 0 : kSettlingSnapshots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsKeeper::ShouldSave
//
//  Once the reopened views show up, what is on screen is taken as the
//  starting point WITHOUT being written: a view reopened on its address can
//  land a line away from where it was saved, and writing that back would
//  walk it further every restart.
//
////////////////////////////////////////////////////////////////////////////////

bool OpenViewsKeeper::ShouldSave (const std::string & text)
{
    if (m_settling > 0)
    {
        m_settling = (text == m_saved) ? 0 : m_settling - 1;

        if (m_settling == 0)
        {
            m_saved = text;
        }

        return false;
    }

    if (text == m_saved)
    {
        return false;
    }

    m_saved = text;
    return true;
}
