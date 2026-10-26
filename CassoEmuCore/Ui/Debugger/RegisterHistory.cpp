#include "Pch.h"

#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/UndoText.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::Record (const std::string & name, Byte before, Byte after, Word pc, uint64_t cycles)
{
    if (before == after)
    {
        return;
    }

    if (!m_undo.empty() && (pc != m_pc || !IsSameStop (cycles)))
    {
        m_undo.clear();
    }

    m_pc     = pc;
    m_cycles = cycles;
    m_undo.push_back ({ name, before, after });
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::OnSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::OnSnapshot (bool isPaused, Word pc, uint64_t cycles)
{
    //  A run that stops again at the same PC within one frame is seen only
    //  as a paused snapshot at that PC, but its cycles differ from those of
    //  the stop the steps were made at.
    if (!isPaused || pc != m_pc || !IsSameStop (cycles))
    {
        Clear();
    }
    else if (m_cycles == kAnyCycles)
    {
        m_cycles = cycles;
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





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::ToText
//
//  Each step is its register's name, its value before and its value after,
//  apart at colons.
//
////////////////////////////////////////////////////////////////////////////////

std::string RegisterHistory::ToText() const
{
    std::string  text = std::format ("{:04X}", m_pc);



    for (const std::vector<Step> * steps : { &m_undo, &m_redo })
    {
        text += "|";

        for (size_t i = 0; i < steps->size(); i++)
        {
            text += std::format ("{}{}:{:02X}:{:02X}", (i == 0) ? "" : ",", (*steps)[i].name, (*steps)[i].before, (*steps)[i].after);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::FromText
//
////////////////////////////////////////////////////////////////////////////////

void RegisterHistory::FromText (const std::string & text)
{
    std::vector<std::string>  parts = UndoText::Split (text, '|');
    uint32_t                  pc    = 0;



    Clear();

    if (parts.size() != 3 || !UndoText::TryParseHex (parts[0], pc))
    {
        return;
    }

    for (size_t list = 1; list <= 2; list++)
    {
        for (const std::string & field : UndoText::Split (parts[list], ','))
        {
            std::vector<std::string>  values = UndoText::Split (field, ':');
            uint32_t                  before = 0;
            uint32_t                  after  = 0;

            if (values.size() != 3 || values[0].empty() || !UndoText::TryParseHex (values[1], before) || !UndoText::TryParseHex (values[2], after))
            {
                Clear();
                return;
            }

            ((list == 1) ? m_undo : m_redo).push_back ({ values[0], (Byte) before, (Byte) after });
        }
    }

    m_pc = (Word) pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RegisterHistory::GetUndoText() const
{
    return m_undo.empty() ? std::wstring() : L"change register " + TextEncoding::NarrowToWide (m_undo.back().name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory::GetRedoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RegisterHistory::GetRedoText() const
{
    return m_redo.empty() ? std::wstring() : L"change register " + TextEncoding::NarrowToWide (m_redo.back().name);
}
