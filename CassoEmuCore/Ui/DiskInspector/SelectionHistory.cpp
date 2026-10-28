#include "Pch.h"

#include "Ui/DiskInspector/SelectionHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionHistory::Record
//
//  A selection equal to the current one is not added again.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionHistory::Record (const InspectorSelection & selection)
{
    if (m_entries.empty() || !(m_entries[m_current] == selection))
    {
        m_entries.resize (m_entries.empty() ? 0 : m_current + 1);
        m_entries.push_back (selection);

        if (m_entries.size() > kMaxEntries)
        {
            m_entries.erase (m_entries.begin());
        }

        m_current = m_entries.size() - 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionHistory::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool SelectionHistory::GoBack (InspectorSelection & outSelection)
{
    bool  isMoved = CanGoBack();



    if (isMoved)
    {
        m_current--;
        outSelection = m_entries[m_current];
    }

    return isMoved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionHistory::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool SelectionHistory::GoForward (InspectorSelection & outSelection)
{
    bool  isMoved = CanGoForward();



    if (isMoved)
    {
        m_current++;
        outSelection = m_entries[m_current];
    }

    return isMoved;
}
