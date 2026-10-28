#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerPane.h"

struct ExplorerContext;
class ExplorerPreview;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewFind
//
//  The hex view's Search and Go to boxes, on the Preview pane's toolbar.
//  Search finds bytes or Apple text, with or without the high bit, from the
//  selection on; Go to moves to an address, an offset or a range.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreviewFind
{
public:
    ExplorerPreviewFind (ExplorerPreview & preview, ExplorerContext & context);

    void  Create          ();
    void  HandleFocusChanged  (ExplorerPane focus);
    bool  HandleBoxKey        (ExplorerPane focus, const DxuiKeyEvent & ev);
    bool  IsCaretOn       (ExplorerPane focus) const;

    DxuiToolbarEditBox *  GetSearchBox () { return &m_searchBox; }
    DxuiToolbarEditBox *  GetGoToBox   () { return &m_goToBox; }

    //  Focus to a box while the hex view is visible. Each returns whether focus
    //  moved.
    bool  FocusGoTo       ();
    bool  FocusFind       ();

    bool  CanFindNext     () const;
    void  FindNext        (bool incremental = false);
    void  GoToTyped       (const std::wstring & text);
    void  HandleSearchChanged (const std::wstring & text);

private:
    ExplorerPreview  & m_preview;
    ExplorerContext  & m_context;

    DxuiToolbarEditBox     m_searchBox;
    DxuiToolbarEditBox     m_goToBox;

    //  The last search, as typed and as the bytes it matches.
    std::vector<Byte>      m_findBytes;
    std::wstring           m_findTyped;
    bool                   m_findIsText = false;
};