#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewHexSources.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView
//
//  A preview's bytes in the hex view, with the view's settings. A host file
//  is read from the file as the view draws it; an Apple file is already in
//  memory.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewHexView : public PreviewView
{
public:
    PreviewHexView (ExplorerPreviewManager & manager, ExplorerContext & context);

    void                                Create                () override;
    void                                Configure             () override;
    void                                Layout                (const RECT & content) override;
    void                                Hide                  () override;
    bool                                UpdateScrollbarHover  (POINT at) override;
    bool                                TickScrollbars        (int64_t nowMs) override;
    bool                                OpenHostFile          (const PreviewContent & preview, HostCharset & outCharset);
    void                                Fill                  (const PreviewContent & preview, bool hex, bool opened, HostCharset charset);
    std::wstring                        DescribeFileError     () const;
    void                                SetVisible            (bool visible);
    void                                SetZoom               (float zoom);
    bool                                IsShowing             () const;
    LPCWSTR                             GetCursorForPoint     (POINT clientPx) const;
    bool                                TryGetScrollbarMenu   (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    bool                                TryGetSelectionDetail (std::wstring & outDetail) const;
    bool                                IsShowingValues       () const                  { return m_hex->IsShowingValues(); }
    void                                SetGrouping           (int grouping);
    void                                SetColumns            (int columns);
    void                                SetFormat             (const char * format);
    void                                SetShowValues         (bool show);
    DxuiHexView                       * GetView               () const                  { return m_hex; }
    static DxuiHexView::TextEncoding    GetHexEncoding        (bool hostFile, HostCharset charset);
    static DxuiHexView::ValueFormat     ParseHexFormat        (const std::string & name);

    bool            HandleCapturedMouse   (const DxuiMouseEvent & ev);
    bool            HandleMouse           (const DxuiMouseEvent & ev);
    bool            HandleKey             (const DxuiKeyEvent & ev) { return m_hex->OnKey (ev); }
    bool            HandleTabKey          (const DxuiKeyEvent & ev);
    void            HandleFocusEntered    (bool forward);
    void            HandleFocusChanged    (bool focused)            { m_hex->OnFocusChanged (focused); }

private:
    DxuiHexView       * m_hex = nullptr;
    PreviewBytes        m_previewBytes;
    PreviewFileBytes    m_fileBytes;
};
