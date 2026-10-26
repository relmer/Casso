#include "Pch.h"

#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/UndoText.h"
#include "Ui/Debugger/ByteChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::Record
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::Record (Word address, Byte before, Byte after, Word pc, uint64_t cycles)
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
    m_undo.push_back ({ address, before, after });
    m_redo.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::OnSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::OnSnapshot (bool isPaused, Word pc, uint64_t cycles)
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





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::ToText
//
//  Each step is its address, its value before and its value after, apart at
//  colons.
//
////////////////////////////////////////////////////////////////////////////////

std::string StackHistory::ToText() const
{
    std::string  text = std::format ("{:04X}", m_pc);



    for (const std::vector<Step> * steps : { &m_undo, &m_redo })
    {
        text += "|";

        for (size_t i = 0; i < steps->size(); i++)
        {
            text += std::format ("{}{:04X}:{:02X}:{:02X}", (i == 0) ? "" : ",", (*steps)[i].address, (*steps)[i].before, (*steps)[i].after);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::FromText
//
////////////////////////////////////////////////////////////////////////////////

void StackHistory::FromText (const std::string & text)
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
            std::vector<std::string>  values  = UndoText::Split (field, ':');
            uint32_t                  address = 0;
            uint32_t                  before  = 0;
            uint32_t                  after   = 0;

            if (values.size() != 3 || !UndoText::TryParseHex (values[0], address) || !UndoText::TryParseHex (values[1], before) ||
                !UndoText::TryParseHex (values[2], after))
            {
                Clear();
                return;
            }

            ((list == 1) ? m_undo : m_redo).push_back ({ (Word) address, (Byte) before, (Byte) after });
        }
    }

    m_pc = (Word) pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StackHistory::GetUndoText() const
{
    return m_undo.empty() ? std::wstring() : ByteChanges::GetEditText (m_undo.back().address, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory::GetRedoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StackHistory::GetRedoText() const
{
    return m_redo.empty() ? std::wstring() : ByteChanges::GetEditText (m_redo.back().address, 1);
}
