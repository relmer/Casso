#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerPreviewFind.h"
#include "CassoExplorer/CassoExplorerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::ExplorerPreviewFind
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPreviewFind::ExplorerPreviewFind (ExplorerPreview & preview, ExplorerContext & context)
    : m_preview (preview),
      m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewFind::Create()
{
    m_goToBox.SetHint         (L"Go to");
    m_goToBox.SetWidthDip     (130);
    m_goToBox.SetTooltip      (L"Go to an address (e.g., $08FF) or offset (e.g., +10, +$A0), or select a range (e.g., $0803-$0810, $0803,+10). Numbers are hex; # marks decimal, as in -#16.");
    m_goToBox.SetHwnd         (m_context.window.GetHwnd());
    m_goToBox.SetTextRenderer (m_context.window.GetTextRenderer());
    m_goToBox.SetOnChange     ([this] (const std::wstring &) { m_goToBox.SetError (false); });
    m_goToBox.SetOnSubmit     ([this] () { GoToTyped (m_goToBox.GetText()); });

    m_goToBox.SetOnCancel ([this] ()
    {
        m_goToBox.SetText  (L"");
        m_goToBox.SetError (false);
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    });

    m_goToBox.SetOnFocusRequest ([this] ()
    {
        m_preview.SetBarFocus (m_preview.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset));
        m_context.input.SetFocusPane (ExplorerPane::GoTo);
    });

    m_searchBox.SetGlyph        (s_kpszMdl2Search);
    m_searchBox.SetHint         (L"Search");
    m_searchBox.SetHwnd         (m_context.window.GetHwnd());
    m_searchBox.SetTextRenderer (m_context.window.GetTextRenderer());
    m_searchBox.SetOnChange     ([this] (const std::wstring & text) { HandleSearchChanged (text); });
    m_searchBox.SetOnSubmit     ([this] () { FindNext(); });

    m_searchBox.SetOnCancel ([this] ()
    {
        m_searchBox.SetText (L"");
        m_findBytes.clear();
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    });

    m_searchBox.SetOnFocusRequest ([this] ()
    {
        m_preview.SetBarFocus (m_preview.GetPreviewStopIndex (CassoExplorerCommands::kFind));
        m_context.input.SetFocusPane (ExplorerPane::Search);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewFind::HandleFocusChanged (ExplorerPane focus)
{
    m_searchBox.SetFocused (focus == ExplorerPane::Search);
    m_goToBox.SetFocused   (focus == ExplorerPane::GoTo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::CanFindNext
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewFind::CanFindNext() const
{
    return m_preview.IsHexPreviewShowing() && !m_findBytes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::HandleBoxKey
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewFind::HandleBoxKey (ExplorerPane focus, const DxuiKeyEvent & ev)
{
    return (focus == ExplorerPane::Search) ? m_searchBox.OnKey (ev) : m_goToBox.OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::IsCaretOn
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewFind::IsCaretOn (ExplorerPane focus) const
{
    return (focus == ExplorerPane::Search) ? m_searchBox.IsCaretOn() : m_goToBox.IsCaretOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::FocusGoTo
//
//  Focus moves to the Go to box while the hex view is visible. Returns
//  whether it moved.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewFind::FocusGoTo()
{
    if (!m_preview.IsHexPreviewShowing())
    {
        return false;
    }

    m_preview.SetBarFocus (m_preview.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset));
    m_context.input.SetFocusPane (ExplorerPane::GoTo);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::FocusFind
//
//  Find searches the hex preview while it or one of its boxes has focus.
//  Returns whether focus moved to the search box.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewFind::FocusFind()
{
    ExplorerPane  focus = m_context.input.GetFocusPane();



    if (!m_preview.IsHexPreviewShowing() || (focus != ExplorerPane::Preview && focus != ExplorerPane::Search && focus != ExplorerPane::GoTo))
    {
        return false;
    }

    m_preview.SetBarFocus (m_preview.GetPreviewStopIndex (CassoExplorerCommands::kFind));
    m_context.input.SetFocusPane (ExplorerPane::Search);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::GoToTyped
//
//  An address matches the offset column, which counts from the file's load
//  address when it has one; an offset moves from the caret. A target that
//  does not parse or is outside the file marks the box in error, and one
//  that is inside it moves the caret there and returns focus to the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewFind::GoToTyped (const std::wstring & text)
{
    int64_t   origin = (int64_t) m_preview.GetHexView()->GetOriginAddress();
    int64_t   caret  = origin + (int64_t) m_preview.GetHexView()->GetCaret();
    int64_t   count  = (m_preview.GetHexView()->GetSource() != nullptr) ? (int64_t) m_preview.GetHexView()->GetSource()->GetByteCount() : 0;
    int64_t   target = 0;
    int64_t   last   = 0;



    if (!m_preview.IsHexPreviewShowing())
    {
        return;
    }

    if (!ActionsGoTo::TryParseGoTo (text, caret, target, last) || target < origin || last >= origin + count)
    {
        m_goToBox.SetError (true);
        m_context.window.Invalidate();
        return;
    }

    m_goToBox.SetText  (L"");
    m_goToBox.SetError (false);
    m_preview.GetHexView()->GoToOffset ((uint64_t) (target - origin));

    //  A range selects from its first address to its last.
    if (last > target)
    {
        m_preview.GetHexView()->ExtendSelectionTo ((uint64_t) (last - origin));
        m_preview.GetHexView()->EnsureByteVisible ((uint64_t) (target - origin));
    }

    m_context.input.SetFocusPane (ExplorerPane::Preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::HandleSearchChanged
//
//  Each edit searches again from the start of the current match, so typing
//  more of a term keeps the match it has while it still fits. Hex digits find
//  bytes; other text finds characters with or without the high bit, since
//  Apple II text is stored both ways.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewFind::HandleSearchChanged (const std::wstring & text)
{
    if (!ActionsGoTo::TryParseSearch (text, m_findBytes, m_findIsText))
    {
        m_findBytes.clear();
        m_context.window.Invalidate();
        return;
    }

    m_findTyped = text;
    FindNext (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind::FindNext
//
//  Searches forward from just past the selection, wrapping at the end. An
//  incremental search starts at the selection itself and says nothing when
//  the term is not found, since the user is still typing it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewFind::FindNext (bool incremental)
{
    const IDxuiHexSource *  source = m_preview.GetHexView()->GetSource();
    uint64_t                start  = 0;
    uint64_t                found  = 0;



    if (!m_preview.IsHexPreviewShowing() || m_findBytes.empty() || source == nullptr)
    {
        return;
    }

    //  The search reads the whole file through the source, not just the part
    //  the view has drawn.
    start = m_preview.GetHexView()->HasSelection() ? m_preview.GetHexView()->GetSelectionFirst() + (incremental ? 0 : 1) : m_preview.GetHexView()->GetCaret();
    found = ActionsGoTo::FindInSource ([source] (uint64_t offset, std::span<uint8_t> out) { source->ReadBytes (offset, out); },
                                          source->GetByteCount(), m_findBytes, m_findIsText, start);

    if (found == CassoExplorerActions::kNotFoundOffset)
    {
        if (!incremental)
        {
            m_context.window.ShowMessage (std::format (L"{} isn't in this file.", m_findTyped).c_str(), MB_ICONINFORMATION);
        }

        return;
    }

    m_preview.GetHexView()->SelectByte        ((uint64_t) found, m_findIsText ? DxuiHexView::Column::Text : DxuiHexView::Column::Hex);
    m_preview.GetHexView()->ExtendSelectionTo ((uint64_t) (found + m_findBytes.size() - 1));
    m_preview.GetHexView()->EnsureByteVisible ((uint64_t) found);

    m_context.window.Invalidate();
}
