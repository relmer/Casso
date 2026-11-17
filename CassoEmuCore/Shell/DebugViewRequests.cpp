#include "Pch.h"

#include "Shell/DebugViewRequests.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::DebugViewRequests
//
////////////////////////////////////////////////////////////////////////////////

DebugViewRequests::DebugViewRequests (DebuggerViewState & state, std::mutex & lock, std::function<void()> onChanged) :
    m_state     (state),
    m_lock      (lock),
    m_onChanged (std::move (onChanged))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::IsCodeView
//
////////////////////////////////////////////////////////////////////////////////

bool DebugViewRequests::IsCodeView (int view)
{
    return view >= 0 && view < DebuggerViewState::kMaxCodeViews;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetFollowView
//
//  The code view that follows the PC.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetFollowView (int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetFollowView (view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::CloseCodeView
//
//  The first view is never closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::CloseCodeView (int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (view < 1 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return;
    }

    m_state.CloseCodeView (view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::OpenCodeViewAt
//
//  A code view opened with an address on its top line, as it was when Casso
//  last closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::OpenCodeViewAt (int view, Word top)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.OpenCodeViewAt (view, top);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetCodeLines
//
//  The lines a code view fits.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetCodeLines (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetCodeLines (lines, view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetCodeShown
//
//  The lines a code view has on screen.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetCodeShown (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    if (!IsCodeView (view))
    {
        return;
    }

    m_state.SetCodeShown (lines, view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::ShowCode
//
//  A code view centered on an address, opened there when it isn't open, or
//  brought back to the PC with no address.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::ShowCode (int view, std::optional<Word> address)
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

    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::ScrollCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::ScrollCode (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.ScrollCode (lines, view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::HoldCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::HoldCode (bool isHeld, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.HoldCode (isHeld, view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SelectCode
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SelectCode (int view, Word address)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SelectCodeAt (address, view);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetMemoryWindow
//
//  Window 1 is the "memory" view and is never closed; 2 to 4 are "memory2"
//  and on, and no address closes one.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetMemoryWindow (int id, std::optional<Word> address)
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

    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetTraceView
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetTraceView (std::optional<uint64_t> first)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetTraceTop (first);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetTraceShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetTraceShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetTraceShown (shown);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetHeatMapShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapShown (shown);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetHeatColumnsShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetHeatColumnsShown (bool shown)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatColumnsShown (shown);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetHeatMapOptions (const HeatMapOptions & options)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapOptions (options);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetHeatMapHover
//
//  The next snapshot has the cell's last writer and reader.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetHeatMapHover (std::optional<Word> address)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetHeatMapHover (address);
    m_onChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests::SetBasicShown
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewRequests::SetBasicShown (bool isVisible)
{
    std::lock_guard<std::mutex>  viewHeld (m_lock);



    m_state.SetBasicShown (isVisible);
    m_onChanged();
}
