#include "Pch.h"

#include "Ui/Debugger/OpeningFocusDeferral.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OpeningFocusDeferral::Defer
//
//  The first layout could not reach the saved pane. A click or key that came
//  before this still counts, so the deferral starts already dropped.
//
////////////////////////////////////////////////////////////////////////////////

void OpeningFocusDeferral::Defer()
{
    m_isPending = !m_hasUserActed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpeningFocusDeferral::OnUserInput
//
//  The user put the focus somewhere, so a pane opening later does not move it.
//
////////////////////////////////////////////////////////////////////////////////

void OpeningFocusDeferral::OnUserInput()
{
    m_hasUserActed = true;
    m_isPending    = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpeningFocusDeferral::OnSnapshot
//
//  Place the focus the first time the saved pane is ready. Once the reopened
//  views have settled without it, it is not coming, and nothing more is done.
//
////////////////////////////////////////////////////////////////////////////////

OpeningFocusDeferral::Action OpeningFocusDeferral::OnSnapshot (bool isSavedPaneReady, bool isRestoringViews)
{
    Action  action = Action::None;



    if (m_isPending && isSavedPaneReady)
    {
        action = Action::Place;
    }

    if (action == Action::Place || !isRestoringViews)
    {
        m_isPending = false;
    }

    return action;
}
