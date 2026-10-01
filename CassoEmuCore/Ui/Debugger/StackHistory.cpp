#include "Pch.h"

#include "Ui/Debugger/StackHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::Record (Word address, Byte before, Byte after, Word pc)
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
    m_undo.push_back ({ address, before, after });
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::OnSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::OnSnapshot (bool isPaused, Word pc)
{
    if (!isPaused || pc != m_pc)
    {
        Clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::TryUndo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> StackHistory::TryUndo (CommandMode mode)
{
    Step  step;



    if (m_undo.empty())
    {
        return std::nullopt;
    }

    step = m_undo.back();
    m_undo.pop_back();
    m_redo.push_back (step);
    return DebuggerActions::GetEnterByte (step.address, step.before, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::TryRedo
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> StackHistory::TryRedo (CommandMode mode)
{
    Step  step;



    if (m_redo.empty())
    {
        return std::nullopt;
    }

    step = m_redo.back();
    m_redo.pop_back();
    m_undo.push_back (step);
    return DebuggerActions::GetEnterByte (step.address, step.after, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::Clear
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::Clear()
{
    m_undo.clear();
    m_redo.clear();
}
