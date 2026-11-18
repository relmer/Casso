#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerPane.h"

struct ExplorerContext;
class ExplorerPreviewManager;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFindBar
//
//  The hex view's Search and Go to boxes, on the Preview pane's toolbar.
//  Search finds bytes or Apple text, with or without the high bit, from the
//  selection on; Go to moves to an address, an offset or a range.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewFindBar
{
public:
    PreviewFindBar (ExplorerPreviewManager & preview, ExplorerContext & context);

    void                  Create       ();
    bool                  IsCaretOn    (ExplorerPane focus) const;
    DxuiToolbarEditBox  * GetSearchBox () { return &m_searchBox; }
    DxuiToolbarEditBox  * GetGoToBox   () { return &m_goToBox; }
    bool                  FocusGoTo    ();
    bool                  FocusFind    ();
    bool                  CanFindNext  () const;
    void                  FindNext     (bool incremental = false);
    void                  GoToTyped    (const std::wstring & text);

    void  HandleFocusChanged  (ExplorerPane focus);
    bool  HandleBoxKey        (ExplorerPane focus, const DxuiKeyEvent & ev);
    void  HandleSearchChanged (const std::wstring & text);

private:
    ExplorerPreviewManager  & m_preview;
    ExplorerContext         & m_context;
    DxuiToolbarEditBox        m_searchBox;
    DxuiToolbarEditBox        m_goToBox;

    //  The last search, as typed and as the bytes it matches.
    std::vector<Byte>      m_findBytes;
    std::wstring           m_findTyped;
    bool                   m_findIsText = false;
};