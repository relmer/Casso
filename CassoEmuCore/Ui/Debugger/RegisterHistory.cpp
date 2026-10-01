#include "Pch.h"

#include "Ui/Debugger/RegisterHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::Record (const std::string & name, Byte before, Byte after, Word pc)
{
    if (before == after)
    {
        return;
    }

    if (!m_undo.empty() && pc != m_pc)
    {
        m_undo.clear();
    }

    m_pc = pc;
    m_undo.push_back ({ name, before, after });
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::OnSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::OnSnapshot (bool isPaused, Word pc)
{
    if (!isPaused || pc != m_pc)
    {
        Clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::TryUndo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> RegisterHistory::TryUndo (CommandMode mode)
{
    Step  step;



    if (m_undo.empty())
    {
        return std::nullopt;
    }

    step = m_undo.back();
    m_undo.pop_back();
    m_redo.push_back (step);
    return DebuggerActions::GetSetRegister (step.name, step.before, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> RegisterHistory::TryRedo (CommandMode mode)
{
    Step  step;



    if (m_redo.empty())
    {
        return std::nullopt;
    }

    step = m_redo.back();
    m_redo.pop_back();
    m_undo.push_back (step);
    return DebuggerActions::GetSetRegister (step.name, step.after, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::Clear
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::Clear()
{
    m_undo.clear();
    m_redo.clear();
}
