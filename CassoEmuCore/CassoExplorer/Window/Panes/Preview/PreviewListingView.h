#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewArchiveView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"

struct PreviewContent;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView
//
//  A catalog's or an archive's rows under columns, in the preview's list.
//  An archive's rows are handled as the file list's are, by
//  PreviewArchiveView.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewListingView : public PreviewView
{
public:
    PreviewListingView (ExplorerPreviewManager & manager, ExplorerContext & context);

    static constexpr int  kRowHeightDip = 18;   // one line of fixed-width text, not a file listing's roomier row

    void                  Create               () override;
    void                  Configure            () override;
    void                  Layout               (const RECT & content) override;
    void                  Hide                 () override;
    bool                  UpdateScrollbarHover (POINT at) override;
    bool                  TickScrollbars       (int64_t nowMs) override;
    void                  Fill                 (const PreviewContent & preview, bool columns);
    void                  SetVisible           (bool visible);
    LPCWSTR               GetCursorForPoint    (POINT clientPx) const;
    bool                  TryGetScrollbarMenu  (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    DxuiListView        * GetList              () const { return m_list; }
    PreviewArchiveView  & GetArchive           ()       { return m_archive; }
    bool                  IsArchive            () const { return m_isArchive; }

    bool     HandleCapturedMouse (const DxuiMouseEvent & ev);
    bool     HandleMouse         (const DxuiMouseEvent & ev);
    bool     HandleShortcutKey   (const DxuiKeyEvent & ev);
    bool     HandleKey           (const DxuiKeyEvent & ev) { return m_list->OnKey (ev); }
    void     HandleFocusChanged  (bool focused)            { m_list->OnFocusChanged (focused); }

private:
    DxuiListView        * m_list      = nullptr;
    PreviewArchiveView    m_archive   { *this, m_context };
    bool                  m_isArchive = false;   // the rows are an archive's files, as the file list
};
