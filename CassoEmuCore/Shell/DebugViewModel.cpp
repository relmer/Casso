#include "Pch.h"

#include "Shell/DebugViewModel.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::IsCodeView
//
////////////////////////////////////////////////////////////////////////////////

bool DebugViewModel::IsCodeView (int view)
{
    return view >= 0 && view < DebuggerViewState::kMaxCodeViews;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetFollowView
//
//  The code view that follows the PC.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetFollowView (int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetFollowView (view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::CloseCodeView
//
//  The first view is never closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::CloseCodeView (int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (view < 1 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return;
    }

    m_state.CloseCodeView (view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::OpenCodeViewAt
//
//  A code view opened with an address on its top line, as it was when Casso
//  last closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::OpenCodeViewAt (int view, Word top)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.OpenCodeViewAt (view, top);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetCodeLines
//
//  The lines a code view fits.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetCodeLines (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetCodeLines (lines, view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetCodeShown
//
//  The lines a code view has on screen.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetCodeShown (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetCodeShown (lines, view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::ShowCode
//
//  A code view centered on an address, opened there when it isn't open, or
//  brought back to the PC with no address.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::ShowCode (int view, std::optional<Word> address)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    if (address.has_value() && !m_state.IsCodeViewOpen (view))
    {
        m_state.OpenCodeView (view, *address);
    }
    else if (address.has_value())
    {
        m_state.CenterCodeOn (*address, view);
    }
    else
    {
        m_state.ShowPcIn (view);
    }

    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::ScrollCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::ScrollCode (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.ScrollCode (lines, view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::HoldCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::HoldCode (bool isHeld, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.HoldCode (isHeld, view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SelectCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SelectCode (int view, Word address)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SelectCodeAt (address, view);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetMemoryWindow
//
//  Window 1 is the "memory" view and is never closed; 2 to 4 are "memory2"
//  and on, and no address closes one.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetMemoryWindow (int id, std::optional<Word> address)
{
    static constexpr int         kFirstWindow = 1;
    static constexpr int         kLastWindow  = 4;
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (id < kFirstWindow || id > kLastWindow || (id == kFirstWindow && !address.has_value()))
    {
        return;
    }

    if (id == kFirstWindow)
    {
        m_state.SetMemoryAddress (*address);
    }
    else if (address.has_value())
    {
        m_state.OpenMemoryWindow (id, *address);
    }
    else
    {
        m_state.CloseMemoryWindow (id);
    }

    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetTraceView
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetTraceView (std::optional<uint64_t> first)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetTraceTop (first);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetTraceShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetTraceShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetTraceShown (shown);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetHeatMapShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapShown (shown);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetHeatColumnsShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetHeatColumnsShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatColumnsShown (shown);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetHeatMapOptions (const HeatMapOptions & options)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapOptions (options);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetHeatMapHover
//
//  The next snapshot has the cell's last writer and reader.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetHeatMapHover (std::optional<Word> address)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapHover (address);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel::SetBasicShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewModel::SetBasicShown (bool isVisible)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetBasicShown (isVisible);
    m_isDirty = true;
}
